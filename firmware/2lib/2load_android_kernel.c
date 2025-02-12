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
#include "vb2_android_bootimg.h"

// Legacy Android boot
#include "vb2_android_misc.h"

/* Bytes to read at start of the boot/init_boot/vendor_boot partitions */
#define BOOT_HDR_GKI_SIZE 4096

/* Possible values of BCB command */
#define BCB_CMD_BOOTONCE_BOOTLOADER "bootonce-bootloader"
#define BCB_CMD_BOOT_RECOVERY "boot-recovery"

#define VERIFIED_BOOT_PROPERTY_NAME "androidboot.verifiedbootstate="

static enum vb2_boot_command vb2_bcb_command(AvbOps *ops)
{
	struct vb2_bootloader_message bcb;
	AvbIOResult io_ret;
	size_t num_bytes_read;
	enum vb2_boot_command cmd;
	bool writeback = false;

	io_ret = ops->read_from_partition(ops,
					  GPT_ENT_NAME_ANDROID_MISC,
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
		/* Recovery image will clear the command by itself. */
		writeback = false;
	} else if (!strncmp(bcb.command, BCB_CMD_BOOTONCE_BOOTLOADER,
			    VB2_MIN(sizeof(BCB_CMD_BOOTONCE_BOOTLOADER) - 1,
				    sizeof(bcb.command)))) {
		cmd = VB2_BOOT_CMD_BOOTLOADER_BOOT;
		writeback = true;
	} else {
		/* If empty or unknown command, just boot normally */
		if (bcb.command[0] != '\0')
			VB2_DEBUG("Unknown boot command \"%.*s\". Use normal boot.\n",
				  (int)sizeof(bcb.command), bcb.command);
		cmd = VB2_BOOT_CMD_NORMAL_BOOT;
		writeback = false;
	}

	if (!writeback)
		return cmd;

	/* Command field is supposed to be a one-shot thing. Clear it. */
	memset(bcb.command, 0, sizeof(bcb.command));
	io_ret = ops->write_to_partition(ops,
					 GPT_ENT_NAME_ANDROID_MISC,
					 0,
					 sizeof(struct vb2_bootloader_message),
					 &bcb);
	if (io_ret != AVB_IO_RESULT_OK)
		VB2_DEBUG("Failed to update misc parition\n");

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
		VB2_DEBUG("Unknown vb2_fastboot_cmdline version (%d)\n", fb_cmd->version);
		return false;
	}

	if (fb_cmd->magic != magic) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline magic (got 0x%x, expected 0x%x)\n",
			  fb_cmd->magic, magic);
		return false;
	}

	if (fb_cmd->len > sizeof(fb_cmd->cmdline)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline len (%d)\n", fb_cmd->len);
		return false;
	}

	if (fb_cmd->fletcher != fletcher32((char *)&fb_cmd->len,
					   sizeof(fb_cmd->len) + fb_cmd->len)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline checksum\n");
		return false;
	}

	return true;
}

bool vb2_update_fastboot_cmdline_checksum(struct vb2_fastboot_cmdline *fb_cmd)
{
	if (fb_cmd->len > sizeof(fb_cmd->cmdline)) {
		VB2_DEBUG("Wrong vb2_fastboot_cmdline len (%d)\n", fb_cmd->len);
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
					  GPT_ENT_NAME_ANDROID_MISC,
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
	char *ab_suffix = NULL;
	AvbSlotVerifyData *verify_data = NULL;
	AvbOps *avb_ops;
	const char *boot_partitions[] = {
		GPT_ENT_NAME_ANDROID_BOOT,
		GPT_ENT_NAME_ANDROID_INIT_BOOT,
		GPT_ENT_NAME_ANDROID_VENDOR_BOOT,
		GPT_ENT_NAME_ANDROID_PVMFW,
		NULL,
	};
	AvbSlotVerifyFlags avb_flags;
	AvbSlotVerifyResult result;
	vb2_error_t ret;
	char *verified_str;
	struct vb2_fastboot_cmdline *fb_cmd = NULL;
	struct vb2_fastboot_cmdline *fb_bootconfig = NULL;

	/*
	 * Check if the buffer is zero sized (ie. pvmfw loading is not
	 * requested) or the pvmfw partition does not exist. If so skip
	 * loading and verifying it.
	 */
	uint64_t pvmfw_start;
	uint64_t pvmfw_size;
	if (params->pvmfw_buffer_size == 0 ||
	    GptFindPvmfw(gpt, &pvmfw_start, &pvmfw_size) != GPT_SUCCESS) {
		if (params->pvmfw_buffer_size != 0)
			VB2_DEBUG("Couldn't find pvmfw partition. Ignoring.\n");

		boot_partitions[3] = NULL;
		params->pvmfw_size = 0;
	}

	ret = GptGetActiveKernelPartitionSuffix(gpt, &ab_suffix);
	if (ret != GPT_SUCCESS) {
		VB2_DEBUG("Unable to get kernel partition suffix\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_ops = vboot_avb_ops_new(ctx, params, stream, gpt, disk_handle, ab_suffix, true);
	if (avb_ops == NULL) {
		free(ab_suffix);
		VB2_DEBUG("Cannot allocate memory for AVB ops\n");
		return VB2_ERROR_LK_NO_KERNEL_FOUND;
	}

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_keyblock_valid)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops,
			boot_partitions,
			ab_suffix,
			avb_flags,
			AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
			&verify_data);
	free(ab_suffix);

	if (result == AVB_SLOT_VERIFY_RESULT_OK) {
		struct vb2_shared_data *sd = vb2_get_sd(ctx);
		sd->flags |= VB2_SD_FLAG_KERNEL_SIGNED;
	}

	/* Ignore verification errors in developer mode */
	if (!need_keyblock_valid) {
		switch (result) {
		case AVB_SLOT_VERIFY_RESULT_OK:
		case AVB_SLOT_VERIFY_RESULT_ERROR_VERIFICATION:
		case AVB_SLOT_VERIFY_RESULT_ERROR_ROLLBACK_INDEX:
		case AVB_SLOT_VERIFY_RESULT_ERROR_PUBLIC_KEY_REJECTED:
			ret = AVB_SLOT_VERIFY_RESULT_OK;
			break;
		default:
			ret = VB2_ERROR_LK_NO_KERNEL_FOUND;
		}
	} else {
		ret = result;
	}

	/*
	 * Return from this function early so that caller can try fallback to
	 * other partition in case of error.
	 */
	if (ret != AVB_SLOT_VERIFY_RESULT_OK) {
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
	     (fb_bootconfig ? fb_bootconfig->len + 1 : 0)) > params->kernel_bootconfig_size)
		return VB2_ERROR_LOAD_PARTITION_WORKBUF;

	strcpy(params->kernel_bootconfig_buffer, verify_data->cmdline);

	/* Append verifiedbootstate property to bootconfig */
	strcat(params->kernel_bootconfig_buffer, " ");
	strcat(params->kernel_bootconfig_buffer, verified_str);

	free(verified_str);

	if (fb_bootconfig) {
		/* Append fastboot properties to bootconfig */
		strcat(params->kernel_bootconfig_buffer, " ");
		strncat(params->kernel_bootconfig_buffer, fb_bootconfig->cmdline,
			fb_bootconfig->len);

		free(fb_bootconfig);
	}

	if (fb_cmd) {
		if (fb_cmd->len >= params->kernel_cmdline_size)
			return VB2_ERROR_LOAD_PARTITION_WORKBUF;
		/* Append fastboot properties to cmdline */
		strncpy(params->kernel_cmdline_buffer, fb_cmd->cmdline, fb_cmd->len);
		params->kernel_cmdline_buffer[fb_cmd->len] = '\0';

		free(fb_cmd);
	} else {
		params->kernel_cmdline_buffer[0] = '\0';
	}

	/* No need for slot data, partitions should be already at correct
	 * locations in memory since we are using "get_preloaded_partitions"
	 * callbacks.
	 */
	avb_slot_verify_data_free(verify_data);

	/*
	 * Bootloader expects kernel image at the very beginning of
	 * kernel_buffer, but verification requires boot header before
	 * kernel. Since the verification is done, we need to move kernel
	 * at proper address.
	 */
	memmove((uint8_t *)params->kernel_buffer,
	       (uint8_t *)params->kernel_buffer + BOOT_HDR_GKI_SIZE,
	       params->vendor_boot_offset - BOOT_HDR_GKI_SIZE);

	return ret;
}

// Android boot
#define GPT_ENT_NAME_ANDROID_A_SUFFIX "_a"
#define GPT_ENT_NAME_ANDROID_B_SUFFIX "_b"

static vb2_error_t vb2_map_libavb_errors(AvbSlotVerifyResult avb_error)
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
static vb2_error_t save_bootconfig(struct vendor_boot_img_hdr_v4 *vendor_hdr,
				   size_t total_size,
				   struct vb2_kernel_params *params)
{
	uint8_t *bootconfig;
	size_t bootconfig_offset;
	uint32_t page_size = vendor_hdr->page_size;

	if (!vendor_hdr->bootconfig_size)
		return VB2_SUCCESS;

	bootconfig_offset = VB2_ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4), page_size) +
			    VB2_ALIGN_UP(vendor_hdr->vendor_ramdisk_size, page_size) +
			    VB2_ALIGN_UP(vendor_hdr->dtb_size, page_size) +
			    VB2_ALIGN_UP(vendor_hdr->vendor_ramdisk_table_size, page_size);
	if (bootconfig_offset > total_size ||
	    total_size - bootconfig_offset < vendor_hdr->bootconfig_size) {
		VB2_DEBUG("Broken 'vendor_boot' image\n");
		return VB2_ERROR_ANDROID_BROKEN_VENDOR_BOOT;
	}

	params->bootconfig = malloc(vendor_hdr->bootconfig_size);
	if (!params->bootconfig) {
		VB2_DEBUG("Cannot malloc %u bytes for bootconfig", vendor_hdr->bootconfig_size);
		return VB2_ERROR_ANDROID_MEMORY_ALLOC;
	}

	bootconfig = (uint8_t *)vendor_hdr + bootconfig_offset;
	memcpy(params->bootconfig, bootconfig, vendor_hdr->bootconfig_size);
	params->bootconfig_size = vendor_hdr->bootconfig_size;
	return VB2_SUCCESS;
}


/*
 * This function validates the partitions magic numbers and move them into place requested
 * from linux.
 */
static vb2_error_t rearrange_partitions(AvbOps *avb_ops,
					struct vb2_kernel_params *params)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	size_t vendor_boot_size, init_boot_size;
	uint8_t *vendor_ramdisk_end = 0;

	if (vb2_android_get_buffer(avb_ops, GPT_ANDROID_VENDOR_BOOT, (void **)&vendor_hdr,
				   &vendor_boot_size) ||
	    vb2_android_get_buffer(avb_ops, GPT_ANDROID_INIT_BOOT, (void **)&init_hdr,
				   &init_boot_size)) {
		VB2_DEBUG("Cannot get information about preloaded paritition\n");
		return VB2_ERROR_ANDROID_RAMDISK_ERROR;
	}

	if (vendor_boot_size < sizeof(*vendor_hdr) ||
	    memcmp(vendor_hdr->magic, VENDOR_BOOT_MAGIC, VENDOR_BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("Incorrect magic or size (%zx) of 'vendor_boot' image\n",
			  vendor_boot_size);
		return VB2_ERROR_ANDROID_BROKEN_VENDOR_BOOT;
	}

	/* Save bootconfig for depthcharge, it can be overwritten when ramdisk are moved */
	VB2_TRY(save_bootconfig(vendor_hdr, vendor_boot_size, params));

	/* Validate init_boot partition */
	if (init_boot_size < BOOT_HEADER_SIZE ||
	    init_boot_size - BOOT_HEADER_SIZE < init_hdr->ramdisk_size ||
	    init_hdr->kernel_size != 0 ||
	    memcmp(init_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE)) {
		VB2_DEBUG("Incorrect 'init_boot' header, total size: %zx\n",
			  init_boot_size);
		return VB2_ERROR_ANDROID_BROKEN_INIT_BOOT;
	}

	/* On init_boot there's no kernel, so ramdisk follows the header */
	uint8_t *init_boot_ramdisk = (uint8_t *)init_hdr + BOOT_HEADER_SIZE;
	size_t init_boot_ramdisk_size = init_hdr->ramdisk_size;

	/*
	 * Move init_boot ramdisk to directly follow the vendor_boot ramdisk.
	 * This is a requirement from Android system. The cpio/gzip/lz4
	 * compression formats support this type of concatenation. After
	 * the kernel decompresses, it extracts concatenated file into
	 * an initramfs, which results in a file structure that's a generic
	 * ramdisk (from init_boot) overlaid on the vendor ramdisk (from
	 * vendor_boot) file structure.
	 */
	vendor_ramdisk_end = (uint8_t *)vendor_hdr +
		VB2_ALIGN_UP(sizeof(*vendor_hdr), vendor_hdr->page_size) +
		vendor_hdr->vendor_ramdisk_size;
	VB2_ASSERT(vendor_ramdisk_end < init_boot_ramdisk);
	memmove(vendor_ramdisk_end, init_boot_ramdisk, init_boot_ramdisk_size);
	params->ramdisk_size += init_boot_ramdisk_size;

	/* Save vendor cmdline for booting */
	vendor_hdr->cmdline[sizeof(vendor_hdr->cmdline) - 1] = '\0';
	params->vendor_cmdline_buffer = (char *)vendor_hdr->cmdline;

	return VB2_SUCCESS;
}

vb2_error_t vb2_load_android(struct vb2_context *ctx, GptData *gpt, GptEntry *entry,
			     struct vb2_kernel_params *params, vb2ex_disk_handle_t disk_handle)
{
	AvbSlotVerifyData *verify_data = NULL;
	AvbOps *avb_ops;
	AvbSlotVerifyFlags avb_flags;
	AvbSlotVerifyResult result;
	vb2_error_t rv;
	const char *boot_partitions[] = {
		GptPartitionNames[GPT_ANDROID_BOOT],
		GptPartitionNames[GPT_ANDROID_INIT_BOOT],
		GptPartitionNames[GPT_ANDROID_VENDOR_BOOT],
		NULL,
	};
	const char *slot_suffix = NULL;
	bool need_verification = vb2_need_kernel_verification(ctx);

	/* Update flags to mark loaded GKI image */
	params->flags = VB2_KERNEL_TYPE_BOOTIMG;

	const char *vbmeta = GptPartitionNames[GPT_ANDROID_VBMETA];
	if (GptEntryHasName(entry, vbmeta, GPT_ENT_NAME_ANDROID_A_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_A_SUFFIX;
	else if (GptEntryHasName(entry, vbmeta, GPT_ENT_NAME_ANDROID_B_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_B_SUFFIX;
	else
		return VB2_ERROR_ANDROID_INVALID_SLOT_SUFFIX;

	avb_ops = vboot_avb_ops_new(ctx, params, gpt, disk_handle, slot_suffix);
	if (!avb_ops)
		return VB2_ERROR_ANDROID_MEMORY_ALLOC;

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_verification)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops, boot_partitions, slot_suffix, avb_flags,
				 AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
				 &verify_data);

	/* Ignore verification errors in developer mode */
	if (!need_verification) {
		switch (result) {
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
	rv = vb2_map_libavb_errors(result);
	if (rv != VB2_SUCCESS)
		goto out;

	/*
	 * Before booting we need to rearrange buffers with partition data, which includes:
	 * - save bootconfig in separate buffer, so depthcharge can modify it
	 * - concatenate ramdisks from vendor_boot & init_boot partitions
	 */
	rv = rearrange_partitions(avb_ops, params);

out:
	/* No need for slot data */
	if (verify_data != NULL)
		avb_slot_verify_data_free(verify_data);

	vboot_avb_ops_free(avb_ops);

	return rv;
}
