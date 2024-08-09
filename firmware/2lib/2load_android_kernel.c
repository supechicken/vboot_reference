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

/* Android BCB commands */
enum vb2_boot_command {
	VB2_BOOT_CMD_NORMAL_BOOT,
	VB2_BOOT_CMD_RECOVERY_BOOT,
	VB2_BOOT_CMD_BOOTLOADER_BOOT,
};

/* BCB structure from Android recovery bootloader_message.h */
struct bootloader_message {
	char command[32];
	char status[32];
	char recovery[768];
	char stage[32];
	char reserved[1184];
};
_Static_assert(sizeof(struct bootloader_message) == 2048,
	       "bootloader_message size is incorrect");

/* Possible values of BCB command */
#define BCB_CMD_BOOTONCE_BOOTLOADER "bootonce-bootloader"
#define BCB_CMD_BOOT_RECOVERY "boot-recovery"

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

static enum vb2_boot_command vb2_bcb_command(AvbOps *ops)
{
	struct bootloader_message bcb;
	AvbIOResult io_ret;
	size_t num_bytes_read;
	enum vb2_boot_command cmd;

	io_ret = ops->read_from_partition(ops,
					  GptPartitionNames[GPT_ANDROID_MISC],
					  0,
					  sizeof(bcb),
					  &bcb,
					  &num_bytes_read);
	if (io_ret != AVB_IO_RESULT_OK ||
	    num_bytes_read != sizeof(struct bootloader_message)) {
		/*
		 * TODO(b/349304841): Handle IO errors, for now just try to boot
		 *                    normally
		 */
		VB2_DEBUG("Cannot read misc partition.\n");
		return VB2_BOOT_CMD_NORMAL_BOOT;
	}

	/* BCB command field is for the bootloader */
	if (!strcmp(bcb.command, BCB_CMD_BOOT_RECOVERY)) {
		cmd = VB2_BOOT_CMD_RECOVERY_BOOT;
	} else if (!strcmp(bcb.command, BCB_CMD_BOOTONCE_BOOTLOADER)) {
		cmd = VB2_BOOT_CMD_BOOTLOADER_BOOT;
	} else {
		/* If empty or unknown command, just boot normally */
		if (bcb.command[0] != '\0')
			VB2_DEBUG("Unknown boot command \"%.*s\". Use normal boot.\n",
				  (int)sizeof(bcb.command), bcb.command);
		cmd = VB2_BOOT_CMD_NORMAL_BOOT;
	}

	return cmd;
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

static bool gki_is_recovery_boot(enum vb2_boot_command boot_command)
{
	switch (boot_command) {
	case VB2_BOOT_CMD_NORMAL_BOOT:
		return false;

	case VB2_BOOT_CMD_BOOTLOADER_BOOT:
		/*
		 * TODO(b/358088653): We should enter fastboot mode and clear
		 * BCB command in misc partition. For now ignore that and boot
		 * to recovery where fastbootd should be available.
		 */
		return true;

	case VB2_BOOT_CMD_RECOVERY_BOOT:
		return true;

	default:
		printf("Unknown boot command, assume recovery boot is required\n");
		return true;
	}
}

static bool gki_ramdisk_fragment_needed(struct vendor_ramdisk_table_entry_v4 *fragment,
					bool recovery_boot)
{
	/* Ignore all other properties except ramdisk type */
	switch (fragment->ramdisk_type) {
	case VENDOR_RAMDISK_TYPE_PLATFORM:
	case VENDOR_RAMDISK_TYPE_DLKM:
		return true;

	case VENDOR_RAMDISK_TYPE_RECOVERY:
		return recovery_boot;

	default:
		printf("Unknown ramdisk type 0x%x\n", fragment->ramdisk_type);

		return false;
	}
}

/*
 * This function removes unnecessary ramdisks from ramdisk table, concanetates rest of
 * them and returns start and end of new ramdisk.
 */
static int prepare_vendor_ramdisks(struct vendor_boot_img_hdr_v4 *vendor_hdr,
				   bool recovery_boot,
				   uint8_t **vendor_ramdisk,
				   uint8_t **vendor_ramdisk_end)
{
	uint32_t vendor_ramdisk_offset;
	uint32_t vendor_ramdisk_table_offset;
	uint32_t page_size = vendor_hdr->page_size;

	/* Calculate address offset of vendor_ramdisk section on vendor_boot partition */
	vendor_ramdisk_offset = ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4), page_size);
	vendor_ramdisk_table_offset = vendor_ramdisk_offset +
		ALIGN_UP(vendor_hdr->vendor_ramdisk_size, page_size) +
		ALIGN_UP(vendor_hdr->dtb_size, page_size);

	/* Check if vendor ramdisk table is correct */
	if (vendor_hdr->vendor_ramdisk_table_size <
	    vendor_hdr->vendor_ramdisk_table_entry_num *
	    vendor_hdr->vendor_ramdisk_table_entry_size) {
		printf("GKI: Too small vendor ramdisk table\n");
		return -1;
	}

	*vendor_ramdisk = (uint8_t *)vendor_hdr + vendor_ramdisk_offset;
	*vendor_ramdisk_end = *vendor_ramdisk;
	/* Go through all ramdisk fragments and keep only the required ones */
	for (uintptr_t i = 0,
	     fragment_ptr = (uintptr_t)vendor_hdr + vendor_ramdisk_table_offset;
	     i < vendor_hdr->vendor_ramdisk_table_entry_num;
	     fragment_ptr += vendor_hdr->vendor_ramdisk_table_entry_size, i++) {

		struct vendor_ramdisk_table_entry_v4 *fragment;
		uint8_t *fragment_src;

		fragment = (struct vendor_ramdisk_table_entry_v4 *)fragment_ptr;
		if (!gki_ramdisk_fragment_needed(fragment, recovery_boot))
			/* Fragment not needed, skip it */
			continue;

		fragment_src = *vendor_ramdisk + fragment->ramdisk_offset;
		if (*vendor_ramdisk_end != fragment_src)
			/*
			 * A fragment was skipped before, we need to move current one
			 * at the correct place.
			 */
			memmove(*vendor_ramdisk_end, fragment_src, fragment->ramdisk_size);

		/* Update location of the end of vendor ramdisk */
		*vendor_ramdisk_end += fragment->ramdisk_size;
	}
	return 0;
}

/*
 * This function validates the partitions magic numbers and move them into place requested
 * from linux.
 */
static int android_rearrange_partitions(struct vb2_kernel_params *params, bool recovery_boot)
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

	/* Remove unused ramdisks */
	ret = prepare_vendor_ramdisks(vendor_hdr, recovery_boot, &params->ramdisk,
				      &vendor_ramdisk_end);
	if (ret)
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	params->ramdisk_size = vendor_ramdisk_end - params->ramdisk;

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

	/* Check "misc" partition for boot type */
	enum vb2_boot_command boot_command = vb2_bcb_command(avb_ops);
	bool recovery_boot = gki_is_recovery_boot(boot_command);

	/*
	 * Before booting we need to rearrange buffers with partition data, which includes:
	 * - save bootconfig in separate buffer, so depthcharge can modify it
	 * - removing unused ramdisks depends on boot type (normal/recovery)
	 * - concatenate ramdisks from vendor_boot & init_boot partitions
	 */
	ret = android_rearrange_partitions(params, recovery_boot);

	/* No need for slot data, partitions should be already at correct
	 * locations in memory since we are using "get_preloaded_partitions"
	 * callbacks.
	 */
	avb_slot_verify_data_free(verify_data);

	return ret;
}
