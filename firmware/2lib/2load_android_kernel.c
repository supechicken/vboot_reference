/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions to load and verify an Android kernel.
 */

#include "2api.h"
#include "2avb.h"
#include "2common.h"
#include "2load_android_kernel.h"
#include "2misc.h"
#include "cgptlib.h"
#include "cgptlib_internal.h"
#include "gpt_misc.h"
#include "vboot_api.h"
#include <vb2_android_bootimg.h>

#define __ALIGN_MASK(x, mask) (((x)+(mask))&~(mask))
#define ALIGN_UP(x, a) __ALIGN_MASK(x, (__typeof__(x))(a)-1UL)
#define ANDROID_GKI_BOOT_HDR_SIZE 4096

#define GPT_ENT_NAME_ANDROID_A_SUFFIX "_a"
#define GPT_ENT_NAME_ANDROID_B_SUFFIX "_b"



static int vb2_map_libavb_errors(AvbSlotVerifyResult avb_error)
{
	/* Map AVB error into VB2 */
	switch (avb_error) {
	case AVB_SLOT_VERIFY_RESULT_OK:
		return VB2_SUCCESS;
	case AVB_SLOT_VERIFY_RESULT_ERROR_OOM:
		return VB2_ERROR_AVB_OOM;
	case AVB_SLOT_VERIFY_RESULT_ERROR_IO:
		return VB2_ERROR_AVB_ERROR_IO;
	case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
		return VB2_ERROR_AVB_ERROR_VERIFICATION;
	case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
		return VB2_ERROR_AVB_ERROR_ROLLBACK_INDEX;
	case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
		return VB2_ERROR_AVB_ERROR_PUBLIC_KEY_REJECTED;
	case AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_METADATA:
		return VB2_ERROR_AVB_ERROR_INVALID_METADATA;
	case AVB_SLOT_VERIFY_RESULT_ERROR_UNSUPPORTED_VERSION:
		return VB2_ERROR_AVB_ERROR_UNSUPPORTED_VERSION;
	case AVB_SLOT_VERIFY_RESULT_ERROR_INVALID_ARGUMENT:
		return VB2_ERROR_AVB_ERROR_INVALID_ARGUMENT;
	default:
		return VB2_ERROR_AVB_ERROR_VERIFICATION;
	}
}

/*
 * Copy bootconfig into separate buffer, it can be overwritten when ramdisks
 * are concatenated. Bootconfig buffer will be processed by depthcharge.
 */
static int save_bootconfig(struct vendor_boot_img_hdr_v4 *vendor_hdr,
			    struct vb2_kernel_params *params)
{
	uint8_t *bootconfig;
	uint32_t page_size = vendor_hdr->page_size;

	params->bootconfig_size = vendor_hdr->bootconfig_size;
	if (!params->bootconfig_size)
		return 0;

	bootconfig = (uint8_t *)vendor_hdr +
		     ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4), page_size) +
		     ALIGN_UP(vendor_hdr->vendor_ramdisk_size, page_size) +
		     ALIGN_UP(vendor_hdr->dtb_size, page_size) +
		     ALIGN_UP(vendor_hdr->vendor_ramdisk_table_size, page_size);

	params->bootconfig = malloc(params->bootconfig_size);
	if (!params->bootconfig) {
		VB2_DEBUG("Cannot malloc %ld bytes for bootconfig", params->bootconfig_size);
		return -1;
	}

	memcpy(params->bootconfig, bootconfig, params->bootconfig_size);
	return 0;
}


/*
 * This function validates the partitions magic numbers and move them into place requested
 * from linux.
 */
static int android_rearrange_partitions(struct vb2_kernel_params *params)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	uint8_t *vendor_ramdisk_end = 0;
	int ret;

	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)parts[GPT_ANDROID_VENDOR_BOOT].buffer;
	if (memcmp(vendor_hdr->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("VENDOR_BOOT_MAGIC mismatch!\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	/* Save bootconfig for depthcharge, it can be overwritten when ramdisk are moved */
	ret = save_bootconfig(vendor_hdr, params);
	if (ret)
		return VB2_ERROR_LK_NO_KERNEL_FOUND;


	/* Validate init_boot partition */
	init_hdr = (struct boot_img_hdr_v4 *)parts[GPT_ANDROID_INIT_BOOT].buffer;
	if (memcmp(init_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("BOOT_MAGIC mismatch!\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}
	if (init_hdr->kernel_size != 0) {
		printf("GKI: Kernel size on init_boot partition has to be zero\n");
		return -1;
	}

	/* On init_boot there's no kernel, so ramdisk follows the header */
	uint8_t *init_boot_ramdisk = (uint8_t *)init_hdr + ANDROID_GKI_BOOT_HDR_SIZE;
	size_t init_boot_ramdisk_size = init_hdr->ramdisk_size;

	/*
	 * Move init_boot ramdisk to directly follow the vendor_boot ramdisk.
	 * This is a requirement from Android system. The cpio/gzip/lz4
	 * compression formats support this type of concatenation. After
	 * the kernel decompresses, it extracts contatenated file into
	 * an initramfs, which results in a file structure that's a generic
	 * ramdisk (from init_boot) overlaid on the vendor ramdisk (from
	 * vendor_boot) file structure.
	 */
	vendor_ramdisk_end = (uint8_t *)vendor_hdr +
		ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4), vendor_hdr->page_size) +
		vendor_hdr->vendor_ramdisk_size;
	VB2_ASSERT(vendor_ramdisk_end < init_boot_ramdisk);
	memmove(vendor_ramdisk_end, init_boot_ramdisk, init_boot_ramdisk_size);
	params->ramdisk_size += init_boot_ramdisk_size;

	/* Save vendor cmdline for booting */
	params->vendor_cmdline_buffer = (char *)vendor_hdr->cmdline;

	return 0;
}

vb2_error_t vb2_load_android(struct vb2_context *ctx, GptData *gpt, GptEntry *entry,
			     struct vb2_kernel_params *params, vb2ex_disk_handle_t disk_handle)
{
	AvbSlotVerifyData *verify_data = NULL;
	AvbOps *avb_ops;
	AvbSlotVerifyFlags avb_flags;
	AvbSlotVerifyResult result;
	vb2_error_t ret;
	const char *boot_partitions[] = {
		GptPartitionNames[GPT_ANDROID_BOOT],
		GptPartitionNames[GPT_ANDROID_INIT_BOOT],
		GptPartitionNames[GPT_ANDROID_VENDOR_BOOT],
		NULL,
	};
	const char *slot_suffix = NULL;
	bool need_verification = vb2_need_kernel_verification(ctx);

	/* Update flags to mark loaded GKI image */
	params->flags &= ~VB2_KERNEL_TYPE_MASK;
	params->flags |= VB2_KERNEL_TYPE_BOOTIMG;

	const char *vb = GptPartitionNames[GPT_ANDROID_VBMETA];
	if (GptEntryHasName(entry, vb, GPT_ENT_NAME_ANDROID_A_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_A_SUFFIX;
	else if (GptEntryHasName(entry, vb, GPT_ENT_NAME_ANDROID_B_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_B_SUFFIX;

	ret = vboot_android_reserve_buffers(params, gpt, slot_suffix, disk_handle);
	if (ret != VB2_SUCCESS) {
		VB2_DEBUG("Cannot reserve buffers for android partitions\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_ops = vboot_avb_ops_new(ctx, params, gpt, disk_handle, slot_suffix);

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_verification)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops, boot_partitions, slot_suffix, avb_flags,
				 AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
				 &verify_data);

	/* Ignore verification errors in developer mode */
	if (!need_verification) {
		switch (result) {
		case AVB_SLOT_VERIFY_RESULT_OK:
		case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
		case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
		case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
			result = AVB_SLOT_VERIFY_RESULT_OK;
			break;
		default:
			break;
		}
	}

	/* Map AVB return code into VB2 code */
	ret = vb2_map_libavb_errors(result);

	/*
	 * Return from this function early so that caller can try fallback to
	 * other partition in case of error.
	 */
	if (ret != VB2_SUCCESS) {
		if (verify_data != NULL)
			avb_slot_verify_data_free(verify_data);
		return ret;
	}

	/*
	 * Before booting we need to rearrange buffers with partition data, which includes:
	 * - save bootconfig in separate buffer, so depthcharge can modify it
	 * - concatenate ramdisks from vendor_boot & init_boot partitions
	 */
	ret = android_rearrange_partitions(params);
	/* No need for slot data, partitions should be already at correct
	 * locations in memory since we are using "get_preloaded_partitions"
	 * callbacks.
	 */
	avb_slot_verify_data_free(verify_data);

	return ret;
}
