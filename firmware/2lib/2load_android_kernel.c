/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions to load and verify an Android kernel.
 */

#include "2api.h"
#include "2common.h"
#include "2load_android_kernel.h"
#include "cgptlib.h"
#include "cgptlib_internal.h"
#include "gpt_misc.h"
#include "vb2_android_misc.h"
#include "vboot_api.h"
#include "vboot_avb_ops.h"

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

#define VERIFIED_BOOT_PROPERTY_NAME "androidboot.verifiedbootstate="

static enum vb2_boot_command vb2_bcb_command(AvbOps *ops)
{
	struct vb2_bootloader_message bcb;
	AvbIOResult io_ret;
	size_t num_bytes_read;
	enum vb2_boot_command cmd;

	io_ret = ops->read_from_partition(ops,
					  GptPartitionNames[GPT_ANDROID_MISC],
					  0,
					  sizeof(struct vb2_bootloader_message),
					  &bcb,
					  &num_bytes_read);
	if (io_ret != AVB_IO_RESULT_OK ||
	    num_bytes_read != sizeof(struct vb2_bootloader_message)) {
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

static uint32_t fletcher32(const char *data, size_t len)
{
	uint32_t s0 = 0;
	uint32_t s1 = 0;

	for (; len > 0; len--, data++) {
		s0 = (s0 + *data) % UINT16_MAX;
		s1 = (s1 + s0) % UINT16_MAX;
	}

	return (s1 << 16) | s0;
}

bool vb2_is_fastboot_cmdline_valid(struct vb2_fastboot_cmdline *fb_cmd,
				   enum vb2_fastboot_cmdline_magic magic)
{
	if (fb_cmd->version != 0) {
		VB2_DEBUG("Unknown vb2_fastboot_cmdline version (%d)", fb_cmd->version);
		return false;
	}

	if (fb_cmd->magic != magic) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline magic (got: 0x%x, expected 0x%x)",
			  fb_cmd->magic, magic);
		return false;
	}

	if (fb_cmd->len > sizeof(fb_cmd->cmdline)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline len (%d)", fb_cmd->len);
		return false;
	}

	if (fb_cmd->fletcher != fletcher32((char *)&fb_cmd->len,
					   sizeof(fb_cmd->len) + fb_cmd->len)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline checksum");
		return false;
	}

	return true;
}

bool vb2_update_fastboot_cmdline_checksum(struct vb2_fastboot_cmdline *fb_cmd)
{
	if (fb_cmd->len > sizeof(fb_cmd->cmdline)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline len (%d)", fb_cmd->len);
		return false;
	}

	fb_cmd->fletcher = fletcher32((char *)&fb_cmd->len, sizeof(fb_cmd->len) + fb_cmd->len);

	return true;
}

static struct vb2_fastboot_cmdline *vb2_fastboot_cmdline(
		AvbOps *ops, enum vb2_fastboot_cmdline_magic magic)
{
	struct vb2_fastboot_cmdline *fb_cmd;
	AvbIOResult io_ret;
	size_t num_bytes_read;
	int64_t offset;

	switch (magic) {
	case VB2_FASTBOOT_CMDLINE_MAGIC:
		offset = VB2_MISC_VENDOR_SPACE_FASTBOOT_CMDLINE_OFFSET;
		break;
	case VB2_FASTBOOT_BOOTCONFIG_MAGIC:
		offset = VB2_MISC_VENDOR_SPACE_FASTBOOT_BOOTCONFIG_OFFSET;
		break;
	default:
		VB2_DEBUG("Unknown magic: 0x%x\n", magic);
		return NULL;
	}

	fb_cmd = malloc(sizeof(struct vb2_fastboot_cmdline));
	if (fb_cmd == NULL)
		return NULL;

	io_ret = ops->read_from_partition(ops,
					  GptPartitionNames[GPT_ANDROID_MISC],
					  offset,
					  sizeof(struct vb2_fastboot_cmdline),
					  fb_cmd,
					  &num_bytes_read);
	if (io_ret != AVB_IO_RESULT_OK ||
	    num_bytes_read != sizeof(struct vb2_fastboot_cmdline)) {
		VB2_DEBUG("Cannot read misc partition (magic: 0x%x, offset: %" PRIi64 ").\n",
			   magic, offset);
		free(fb_cmd);
		return NULL;
	}

	if (!vb2_is_fastboot_cmdline_valid(fb_cmd, magic)) {
		free(fb_cmd);
		return NULL;
	}

	return fb_cmd;
}

vb2_error_t vb2_load_android_kernel(
	struct vb2_context *ctx, struct vb2_kernel_params *params,
	VbExStream_t stream, GptData *gpt, vb2ex_disk_handle_t disk_handle,
	int need_keyblock_valid)
{
	AvbSlotVerifyData *verify_data = NULL;
	AvbOps *avb_ops;
	AvbSlotVerifyFlags avb_flags;
	AvbSlotVerifyResult result;
	vb2_error_t ret;
	const char *boot_part;
	char *verified_str;
	struct vb2_fastboot_cmdline *fb_cmd = NULL;
	struct vb2_fastboot_cmdline *fb_bootconfig = NULL;
	GptEntry *entries = (GptEntry *)gpt->primary_entries;
	GptEntry *e;
	const char *boot_partitions[] = {
		GptPartitionNames[GPT_ANDROID_BOOT],
		GptPartitionNames[GPT_ANDROID_INIT_BOOT],
		GptPartitionNames[GPT_ANDROID_VENDOR_BOOT],
		GptPartitionNames[GPT_ANDROID_PVMFW],
		NULL,
	};

	e = &entries[gpt->current_kernel];
	boot_part = GptPartitionNames[GPT_ANDROID_BOOT];
	if (GptEntryHasName(e, boot_part, GPT_ENT_NAME_ANDROID_A_SUFFIX))
		gpt->current_ab_slot = GPT_ENT_NAME_ANDROID_A_SUFFIX;
	else if (GptEntryHasName(e, boot_part, GPT_ENT_NAME_ANDROID_B_SUFFIX))
		gpt->current_ab_slot = GPT_ENT_NAME_ANDROID_B_SUFFIX;
	else
		return VB2_ERROR_LK_NO_KERNEL_FOUND;

	/*
	 * Check if the buffer is zero sized (ie. pvmfw loading is not
	 * requested) or the pvmfw partition does not exist. If so skip
	 * loading and verifying it.
	 */
	e = GptFindEntryByName(gpt, GptPartitionNames[GPT_ANDROID_PVMFW], gpt->current_ab_slot);
	if (params->pvmfw_buffer_size == 0 || !e) {
		if (!e)
			VB2_DEBUG("Couldn't find pvmfw partition. Ignoring.\n");

		boot_partitions[3] = NULL;
		params->pvmfw_size = 0;
	}

	avb_ops = vboot_avb_ops_new(ctx, params, stream, gpt, disk_handle);
	if (avb_ops == NULL) {
		VB2_DEBUG("Cannot allocate memory for AVB ops\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_keyblock_valid)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops,
			boot_partitions,
			gpt->current_ab_slot,
			avb_flags,
			AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			&verify_data);

	/* Ignore verification errors in developer mode */
	if (!need_keyblock_valid && ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) {
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
		vboot_avb_ops_free(avb_ops);
		return ret;
	}

	params->boot_command = vb2_bcb_command(avb_ops);

	/* Load fastboot cmdline and bootconfig only in developer mode */
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) {
		fb_cmd = vb2_fastboot_cmdline(avb_ops, VB2_FASTBOOT_CMDLINE_MAGIC);
		fb_bootconfig = vb2_fastboot_cmdline(avb_ops, VB2_FASTBOOT_BOOTCONFIG_MAGIC);
	}

	vboot_avb_ops_free(avb_ops);

	/* TODO(b/335901799): Add support for marking verifiedbootstate yellow */
	/* Possible values for this property are "yellow", "orange" and "green"
	 * so allocate 6 bytes plus 1 byte for NULL terminator.
	 */
	verified_str = malloc(strlen(VERIFIED_BOOT_PROPERTY_NAME) + 7);
	if (verified_str == NULL)
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	sprintf(verified_str, "%s%s", VERIFIED_BOOT_PROPERTY_NAME,
		(ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ? "orange" : "green");

	if ((strlen(verify_data->cmdline) + 1 + strlen(verified_str) + 1 +
	     (fb_bootconfig ? fb_bootconfig->len + 1 : 0)) > params->vboot_bootconfig_size)
		return VB2_ERROR_LOAD_PARTITION_WORKBUF;

	strcpy(params->vboot_bootconfig_buffer, verify_data->cmdline);

	/* Append verifiedbootstate property to bootconfig */
	strcat(params->vboot_bootconfig_buffer, " ");
	strcat(params->vboot_bootconfig_buffer, verified_str);

	free(verified_str);

	if (fb_bootconfig) {
		/* Append fastboot properties to bootconfig */
		strcat(params->vboot_bootconfig_buffer, " ");
		strncat(params->vboot_bootconfig_buffer, fb_bootconfig->cmdline,
			fb_bootconfig->len);

		free(fb_bootconfig);
	}

	if (fb_cmd) {
		if (fb_cmd->len >= params->vboot_cmdline_size)
			return VB2_ERROR_LOAD_PARTITION_WORKBUF;
		/* Append fastboot properties to cmdline */
		strncpy(params->vboot_cmdline_buffer, fb_cmd->cmdline, fb_cmd->len);
		params->vboot_cmdline_buffer[fb_cmd->len] = '\0';

		free(fb_cmd);
	} else {
		params->vboot_cmdline_buffer[0] = '\0';
	}

	/* No need for slot data, partitions should be already at correct
	 * locations in memory since we are using "get_preloaded_partitions"
	 * callbacks.
	 */
	avb_slot_verify_data_free(verify_data);

	return ret;
}
