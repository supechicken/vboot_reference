/* Copyright 2024 The Chromium OS Authors. All rights reserved.
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

/* Bytes to read at start of the boot/init_boot/vendor_boot partitions */
#define BOOT_HDR_GKI_SIZE 4096

/* Possible values of BCB command */
#define BCB_CMD_BOOTONCE_BOOTLOADER "bootonce-bootloader"
#define BCB_CMD_BOOT_RECOVERY "boot-recovery"

#define VERIFIED_BOOT_PROPERTY_NAME "androidboot.verifiedbootstate"

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
	struct vb2_context *ctx, VbExStream_t stream,
	VbSelectAndLoadKernelParams *params, GptData *gpt,
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

	avb_ops = vboot_avb_ops_new(ctx, params, stream, gpt,
				    params->disk_handle);
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

	/*
	 * Load fastboot cmdline and bootconfig if fastboot is enabled by GBB flag or
	 * FW is in developer mode
	 */
	if (ctx->flags & (VB2_CONTEXT_DEVELOPER_MODE |
			  VB2_GBB_FLAG_FORCE_UNLOCK_FASTBOOT)) {
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
	/*
	 * When booting to recovery with GBB enabled fastboot, always set
	 * verifiedbootstate to orange to unlock all commands of fastbootd.
	 */
	if (ctx->flags & VB2_GBB_FLAG_FORCE_UNLOCK_FASTBOOT &&
	    params->boot_command == VB2_BOOT_CMD_RECOVERY_BOOT)
		sprintf(verified_str, "%s=orange", VERIFIED_BOOT_PROPERTY_NAME);
	else
		sprintf(verified_str, "%s=%s", VERIFIED_BOOT_PROPERTY_NAME,
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

	/* Rollback protection hasn't been implemented yet. */
	return ret;
}

// Android boot
#define GPT_ENT_NAME_ANDROID_A_SUFFIX "_a"
#define GPT_ENT_NAME_ANDROID_B_SUFFIX "_b"

#ifdef NO_LEGACY_ANDROID
/* Android BCB (boot control block) commands */
enum vb2_boot_command {
	VB2_BOOT_CMD_NORMAL_BOOT,
	VB2_BOOT_CMD_RECOVERY_BOOT,
	VB2_BOOT_CMD_BOOTLOADER_BOOT,
};
#endif

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

static enum vb2_boot_command bcb_command(AvbOps *ops)
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
	if (io_ret != AVB_IO_RESULT_OK || num_bytes_read != sizeof(bcb)) {
		/*
		 * TODO(b/349304841): Handle IO errors, for now just try to boot
		 *                    normally
		 */
		VB2_DEBUG("Cannot read misc partition, err: %d\n", io_ret);
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
	}

	return false;
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

/* Function for finding a loaded partition in AvbSlotVerifyData */
static AvbPartitionData *avb_find_part(AvbSlotVerifyData *verify_data, enum GptPartition name)
{
	size_t i;
	AvbPartitionData *part;

	for (i = 0; i < verify_data->num_loaded_partitions; i++) {
		part = &verify_data->loaded_partitions[i];

		if (!strcmp(part->partition_name, GptPartitionNames[name]))
			return part;
	}

	return NULL;
}

/*
 * This function removes unnecessary ramdisks from ramdisk table, concatenates rest of
 * them and returns start and end of new ramdisk.
 */
static vb2_error_t prepare_vendor_ramdisks(struct vendor_boot_img_hdr_v4 *vendor_hdr,
					   size_t total_size,
					   bool recovery_boot,
					   uint8_t **vendor_ramdisk,
					   uint8_t **vendor_ramdisk_end)
{
	uint32_t ramdisk_offset;
	uint32_t ramdisk_table_offset;
	uint32_t ramdisk_table_size = vendor_hdr->vendor_ramdisk_table_size;
	uint32_t ramdisk_table_entry_size = vendor_hdr->vendor_ramdisk_table_entry_size;
	uint32_t ramdisk_table_entry_num = vendor_hdr->vendor_ramdisk_table_entry_num;
	uint32_t page_size = vendor_hdr->page_size;
	uintptr_t fragment_ptr;

	/* Calculate address offset of vendor_ramdisk section on vendor_boot partition */
	ramdisk_offset = VB2_ALIGN_UP(sizeof(struct vendor_boot_img_hdr_v4), page_size);
	ramdisk_table_offset = ramdisk_offset +
		VB2_ALIGN_UP(vendor_hdr->vendor_ramdisk_size, page_size) +
		VB2_ALIGN_UP(vendor_hdr->dtb_size, page_size);

	/* Check if vendor ramdisk table is correct */
	if (ramdisk_offset > total_size ||
	    ramdisk_table_offset > total_size ||
	    ramdisk_table_entry_size < sizeof(struct vendor_ramdisk_table_entry_v4) ||
	    total_size - ramdisk_offset < vendor_hdr->vendor_ramdisk_size ||
	    total_size - ramdisk_table_offset < ramdisk_table_size ||
	    ramdisk_table_size < (ramdisk_table_entry_num * ramdisk_table_entry_size)) {
		VB2_DEBUG("Broken 'vendor_boot' image\n");
		return VB2_ERROR_ANDROID_BROKEN_VENDOR_BOOT;
	}

	*vendor_ramdisk = (uint8_t *)vendor_hdr + ramdisk_offset;
	*vendor_ramdisk_end = *vendor_ramdisk;
	fragment_ptr = (uintptr_t)vendor_hdr + ramdisk_table_offset;
	/* Go through all ramdisk fragments and keep only the required ones */
	for (int i = 0; i < ramdisk_table_entry_num;
	    fragment_ptr += ramdisk_table_entry_size, i++) {
		struct vendor_ramdisk_table_entry_v4 *fragment;
		uint8_t *fragment_src;

		fragment = (struct vendor_ramdisk_table_entry_v4 *)fragment_ptr;
		if (!gki_ramdisk_fragment_needed(fragment, recovery_boot))
			continue;

		uint32_t fragment_size = fragment->ramdisk_size;
		uint32_t fragment_offset = fragment->ramdisk_offset;

		if (fragment_offset > vendor_hdr->vendor_ramdisk_size ||
		    vendor_hdr->vendor_ramdisk_size - fragment_offset < fragment_size) {
			VB2_DEBUG("Incorrect fragment - offset:%x size:%x, ramdisk_size: %x\n",
				  fragment_offset, fragment_size,
				  vendor_hdr->vendor_ramdisk_size);
		}
		fragment_src = *vendor_ramdisk + fragment_offset;
		if (*vendor_ramdisk_end != fragment_src)
			/*
			 * A fragment was skipped before, we need to move current one
			 * at the correct place.
			 */
			memmove(*vendor_ramdisk_end, fragment_src, fragment_size);

		/* Update location of the end of vendor ramdisk */
		*vendor_ramdisk_end += fragment_size;
	}

	return VB2_SUCCESS;
}

static vb2_error_t prepare_pvmfw(AvbSlotVerifyData *verify_data,
				 struct vb2_kernel_params *params)
{
	AvbPartitionData *part;
	struct boot_img_hdr_v4 *pvmfw_hdr;

	part = avb_find_part(verify_data, GPT_ANDROID_PVMFW);
	if (!part) {
		VB2_DEBUG("Ignoring lack of pvmfw partition\n");
		params->pvmfw_out_size = 0;
		return VB2_SUCCESS;
	}

	pvmfw_hdr = (void *)part->data;

	/* If loaded pvmfw is smaller then boot header or the boot header magic is invalid
	 * or the header kernel size exceeds buffer size, then fail */
	if (part->data_size < BOOT_HEADER_SIZE ||
	    memcmp(pvmfw_hdr->magic, BOOT_MAGIC, BOOT_MAGIC_SIZE) ||
	    part->data_size - BOOT_HEADER_SIZE < pvmfw_hdr->kernel_size) {
		VB2_DEBUG("Incorrect magic or size (%zx) of 'pvmfw' image\n", part->data_size);
		return VB2_ERROR_ANDROID_BROKEN_PVMFW;
	}

	/* Get pvmfw code size */
	params->pvmfw_out_size = pvmfw_hdr->kernel_size;

	/* pvmfw code starts after the boot header. Discard the boot header, by
	 * moving the buffer start and trimming its size. */
	params->pvmfw_buffer = ((void *)pvmfw_hdr) + BOOT_HEADER_SIZE;
	params->pvmfw_buffer_size -= BOOT_HEADER_SIZE;

	return VB2_SUCCESS;
}

/*
 * This function validates the partitions magic numbers and move them into place requested
 * from linux.
 */
static vb2_error_t rearrange_partitions(AvbOps *avb_ops,
					struct vb2_kernel_params *params,
					bool recovery_boot)
{
	struct vendor_boot_img_hdr_v4 *vendor_hdr;
	struct boot_img_hdr_v4 *init_hdr;
	size_t vendor_boot_size, init_boot_size;
	uint8_t *vendor_ramdisk_end = 0;

	if (vb2_android_get_buffer(avb_ops, GPT_ANDROID_VENDOR_BOOT, (void **)&vendor_hdr,
				   &vendor_boot_size) ||
	    vb2_android_get_buffer(avb_ops, GPT_ANDROID_INIT_BOOT, (void **)&init_hdr,
				   &init_boot_size)) {
		VB2_DEBUG("Cannot get information about preloaded partition\n");
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

	/* Remove unused ramdisks */
	VB2_TRY(prepare_vendor_ramdisks(vendor_hdr, vendor_boot_size, recovery_boot,
					&params->ramdisk, &vendor_ramdisk_end));
	params->ramdisk_size = vendor_ramdisk_end - params->ramdisk;

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
		GptPartitionNames[GPT_ANDROID_PVMFW],
		NULL,
	};
	const char *slot_suffix = NULL;
	bool need_verification = vb2_need_kernel_verification(ctx);

	/*
	 * Check if the pvmfw buffer is zero sized
	 * (ie. pvmfw loading is not requested)
	 */
	if (params->pvmfw_buffer_size == 0) {
		VB2_DEBUG("Not loading pvmfw: not requested.\n");
		boot_partitions[3] = NULL;
		params->pvmfw_out_size = 0;
	}

	/* Update flags to mark loaded GKI image */
	params->flags = VB2_KERNEL_TYPE_BOOTIMG;

	const char *vbmeta = GptPartitionNames[GPT_ANDROID_VBMETA];
	if (GptEntryHasName(entry, vbmeta, GPT_ENT_NAME_ANDROID_A_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_A_SUFFIX;
	else if (GptEntryHasName(entry, vbmeta, GPT_ENT_NAME_ANDROID_B_SUFFIX))
		slot_suffix = GPT_ENT_NAME_ANDROID_B_SUFFIX;
	else
		return VB2_ERROR_ANDROID_INVALID_SLOT_SUFFIX;

	avb_ops = vboot_avb_ops_new(ctx, params, NULL, gpt, disk_handle, slot_suffix, false);
	if (!avb_ops)
		return VB2_ERROR_ANDROID_MEMORY_ALLOC;

	avb_flags = AVB_SLOT_VERIFY_FLAGS_NONE;
	if (!need_verification)
		avb_flags |= AVB_SLOT_VERIFY_FLAGS_ALLOW_VERIFICATION_ERROR;

	result = avb_slot_verify(avb_ops, boot_partitions, slot_suffix, avb_flags,
				 AVB_HASHTREE_ERROR_MODE_RESTART_AND_INVALIDATE,
				 &verify_data);

	if (result == AVB_SLOT_VERIFY_RESULT_OK) {
		struct vb2_shared_data *sd = vb2_get_sd(ctx);
		sd->flags |= VB2_SD_FLAG_KERNEL_SIGNED;
	}

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

	/* Check "misc" partition for boot type */
	enum vb2_boot_command boot_command = bcb_command(avb_ops);
	bool recovery_boot = gki_is_recovery_boot(boot_command);

	/*
	 * Before booting we need to rearrange buffers with partition data, which includes:
	 * - save bootconfig in separate buffer, so depthcharge can modify it
	 * - remove unused ramdisks depending on boot type (normal/recovery)
	 * - concatenate ramdisks from vendor_boot & init_boot partitions
	 */
	rv = rearrange_partitions(avb_ops, params, recovery_boot);
	if (rv)
		goto out;

	/*
	 * TODO(b/335901799): Add support for marking verifiedbootstate yellow
	 */
	int chars = snprintf(params->vboot_cmdline_buffer, params->vboot_cmdline_size,
			     "%s %s=%s %s=%s %s=%s", verify_data->cmdline,
			     VERIFIED_BOOT_PROPERTY_NAME,
			     need_verification ? "green" : "orange",
			     SLOT_SUFFIX_BOOT_PROPERTY_NAME, slot_suffix,
			     ANDROID_FORCE_NORMAL_BOOT_PROPERTY_NAME, recovery_boot ? "0" : "1"
			     );
	if (chars < 0 || chars >= params->vboot_cmdline_size) {
		VB2_DEBUG("ERROR: Command line doesn't fit provided buffer: %s\n",
			  verify_data->cmdline);
		rv = VB2_ERROR_ANDROID_CMDLINE_BUF_TOO_SMALL;
		goto out;
	}

	rv = prepare_pvmfw(verify_data, params);

out:
	/* No need for slot data */
	if (verify_data != NULL)
		avb_slot_verify_data_free(verify_data);

	vboot_avb_ops_free(avb_ops);

	return rv;
}
