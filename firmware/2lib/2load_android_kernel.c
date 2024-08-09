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
#include "vb2_android_bootimg.h"
#include "vboot_api.h"

#define VERIFIED_BOOT_PROPERTY_NAME "androidboot.verifiedbootstate="

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
					  sizeof(struct bootloader_message),
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
	if (!strncmp(bcb.command, BCB_CMD_BOOT_RECOVERY,
		     VB2_MIN(sizeof(BCB_CMD_BOOT_RECOVERY) - 1, sizeof(bcb.command)))) {
		cmd = VB2_BOOT_CMD_RECOVERY_BOOT;
	} else if (!strncmp(bcb.command, BCB_CMD_BOOTONCE_BOOTLOADER,
			    VB2_MIN(sizeof(BCB_CMD_BOOTONCE_BOOTLOADER) - 1,
				    sizeof(bcb.command)))) {
		cmd = VB2_BOOT_CMD_BOOTLOADER_BOOT;
	} else {
		/* If empty or unknown command, just boot normally */
		if (bcb.command[0] != '\0')
			VB2_DEBUG("Unknown boot command \"%.*s\". Use normal boot.",
				  (int)sizeof(bcb.command), bcb.command);
		cmd = VB2_BOOT_CMD_NORMAL_BOOT;
	}

	return cmd;
}

static vb2_error_t vb2_load_android_partition(GptData *gpt, enum GptPartition part,
					      const char *suffix, uint8_t *buffer,
					      size_t buffer_size, vb2ex_disk_handle_t dh,
					      uint32_t *loaded)
{
	VbExStream_t stream;
	uint32_t read_ms = 0, start_ts;
	uint64_t part_start, part_size, part_bytes;
	vb2_error_t res = VB2_ERROR_LOAD_PARTITION_READ_BODY;
	GptEntry *e;

	e = GptFindEntryByName(gpt, GptPartitionNames[part], suffix);
	if (!e) {
		VB2_DEBUG("Unable to find %s%s\n", GptPartitionNames[part], suffix);
		return VB2_ERROR_LOAD_PARTITION_READ_BODY;
	}

	part_start = e->starting_lba;
	part_size = GptGetEntrySizeLba(e);

	if (VbExStreamOpen(dh, part_start, part_size, &stream)) {
		VB2_DEBUG("Unable to open disk handle.\n");
		return res;
	}

	part_bytes = gpt->sector_bytes * part_size;
	if (part_bytes > buffer_size) {
		VB2_DEBUG("Buffer too small for partition %s%s (has %lu needs: %lld)\n",
			  GptPartitionNames[part], suffix, buffer_size, part_bytes);
		goto out;
	}

	/* Load partition to memory */
	start_ts = vb2ex_mtime();
	if (VbExStreamRead(stream, part_bytes, buffer)) {
		VB2_DEBUG("Unable to read ramdisk partition\n");
		goto out;
	}
	read_ms = vb2ex_mtime() - start_ts;

	if (read_ms == 0)  /* Avoid division by 0 in speed calculation */
		read_ms = 1;
	VB2_DEBUG("read %u KB in %u ms at %u KB/s.\n",
		  (uint32_t)(part_bytes) / 1024, read_ms,
		  (uint32_t)(((part_bytes) * VB2_MSEC_PER_SEC) /
			  (read_ms * 1024)));

	if (loaded != NULL)
		*loaded = part_bytes;

	res = VB2_SUCCESS;
out:
	VbExStreamClose(stream);
	return res;
}


static int vboot_preload_partition(struct vb2_kernel_params *params, GptData *gpt,
				   vb2ex_disk_handle_t disk_handle)
{
	uint8_t *buf;
	uint32_t buf_size;
	uint32_t bytes_used = 0;
	struct boot_img_hdr_v4 *hdr;
	struct vendor_boot_img_hdr_v4 *vendor_hdr;

	buf = params->kernel_buffer;
	buf_size = params->kernel_buffer_size;

	if (!buf || !buf_size) {
		VB2_DEBUG("Caller have not defined kernel_buffer and its size\n");
		return VB2_ERROR_LOAD_PARTITION_BODY_SIZE;
	}

	vb2_load_android_partition(gpt, GPT_ANDROID_BOOT, params->current_android_slot_suffix,
				   buf, buf_size, disk_handle, &bytes_used);

	/* Validate read partition */
	hdr = (struct boot_img_hdr_v4 *)buf;
	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("BOOT_MAGIC mismatch!\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}
	if (hdr->header_version != 4) {
		VB2_DEBUG("Unsupported header version %d\n", hdr->header_version);
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	params->android_boot_size = bytes_used;
	buf_size -= bytes_used;
	buf += bytes_used;

	vb2_load_android_partition(gpt, GPT_ANDROID_VENDOR_BOOT,
				   params->current_android_slot_suffix,
				   buf, buf_size, disk_handle, &bytes_used);
	params->vendor_boot_buffer = buf;
	params->vendor_boot_size = bytes_used;

	/* Validate read partition */
	vendor_hdr = (struct vendor_boot_img_hdr_v4 *)buf;
	if (memcmp(vendor_hdr->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("VENDOR_BOOT_MAGIC mismatch!\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	buf_size -= bytes_used;
	buf += bytes_used;

	vb2_load_android_partition(gpt, GPT_ANDROID_INIT_BOOT,
				   params->current_android_slot_suffix,
				   buf, buf_size, disk_handle, &bytes_used);
	params->init_boot_buffer = buf;
	params->init_boot_size = bytes_used;

	/* Validate read partition */
	hdr = (struct boot_img_hdr_v4 *)buf;
	if (memcmp(hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("INIT_BOOT_MAGIC mismatch!\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	buf_size -= bytes_used;
	buf += bytes_used;

	return 0;
}

static vb2_error_t vb2_load_pvmfw(struct vb2_kernel_params *params, GptData *gpt,
				  vb2ex_disk_handle_t disk_handle)
{
	return vb2_load_android_partition(gpt, GPT_ANDROID_PVMFW,
					  params->current_android_slot_suffix,
					  params->pvmfw_buffer,
					  params->pvmfw_buffer_size,
					  disk_handle, &params->pvmfw_size);

}

vb2_error_t vb2_load_android(struct vb2_context *ctx, struct vb2_kernel_params *params,
			     GptData *gpt, vb2ex_disk_handle_t disk_handle)
{
	AvbSlotVerifyData *verify_data = NULL;
	AvbOps *avb_ops;
	AvbSlotVerifyFlags avb_flags;
	AvbSlotVerifyResult result;
	vb2_error_t ret;
	char *verified_str;
	const char *boot_partitions[] = {
		GptPartitionNames[GPT_ANDROID_BOOT],
		GptPartitionNames[GPT_ANDROID_INIT_BOOT],
		GptPartitionNames[GPT_ANDROID_VENDOR_BOOT],
		NULL,
	};
	bool need_verification = vb2_need_kernel_verification(ctx);

	/* Update flags to mark loaded GKI image */
	params->flags &= ~VB2_KERNEL_TYPE_MASK;
	params->flags |= VB2_KERNEL_TYPE_BOOTIMG;

	ret = vboot_preload_partition(params, gpt, disk_handle);
	if (ret != VB2_SUCCESS) {
		VB2_DEBUG("Cannot preload android partitions\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_ops = vboot_avb_ops_new(ctx, params, gpt, disk_handle);
	if (avb_ops == NULL) {
		VB2_DEBUG("Cannot allocate memory for AVB ops\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_verification)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops,
			boot_partitions,
			params->current_android_slot_suffix,
			avb_flags,
			AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			&verify_data);

	/* Ignore verification errors in developer mode */
	if (!need_verification && ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) {
		switch (result) {
		case AVB_SLOT_VERIFY_RESULT_OK:
		case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
		case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
		case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
			result = AVB_SLOT_VERIFY_RESULT_OK;
			break;
		default:
			result = AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION;
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

	params->boot_command = vb2_bcb_command(avb_ops);

	/* TODO(b/335901799): Add support for marking verifiedbootstate yellow */
	/* Possible values for this property are "yellow", "orange" and "green"
	 * so allocate 6 bytes plus 1 byte for NULL terminator.
	 */
	verified_str = malloc(strlen(VERIFIED_BOOT_PROPERTY_NAME) + 7);
	if (verified_str == NULL)
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	sprintf(verified_str, "%s%s", VERIFIED_BOOT_PROPERTY_NAME,
		(ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ? "orange" : "green");

	if ((strlen(verify_data->cmdline) + strlen(verified_str) + 1) >=
	    params->vboot_cmdline_size)
		return VB2_ERROR_LOAD_PARTITION_WORKBUF;

	strcpy(params->vboot_cmdline_buffer, verify_data->cmdline);

	/* Append verifiedbootstate property to cmdline */
	strcat(params->vboot_cmdline_buffer, " ");
	strcat(params->vboot_cmdline_buffer, verified_str);

	free(verified_str);

	/* No need for slot data, partitions should be already at correct
	 * locations in memory since we are using "get_preloaded_partitions"
	 * callbacks.
	 */
	avb_slot_verify_data_free(verify_data);

	return ret;
}
