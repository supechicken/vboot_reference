/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Externally-callable APIs
 * (Firmware portion)
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2rsa.h"
#include "2secdata.h"
#include "2sha.h"
#include "2sysincludes.h"
#include "2tpm_bootmode.h"
#include "vb2_common.h"

vb2_error_t vb2api_fw_phase1(struct vb2_context *ctx)
{
	vb2_error_t rv;
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	/* Initialize NV context */
	vb2_nv_init(ctx);

	/*
	 * Handle caller-requested reboot due to secdata.  Do this before we
	 * even look at secdata.  If we fail because of a reboot loop we'll be
	 * the first failure so will get to set the recovery reason.
	 */
	if (!(ctx->flags & VB2_CONTEXT_SECDATA_WANTS_REBOOT)) {
		/* No reboot requested */
		vb2_nv_set(ctx, VB2_NV_TPM_REQUESTED_REBOOT, 0);
	} else if (vb2_nv_get(ctx, VB2_NV_TPM_REQUESTED_REBOOT)) {
		/*
		 * Reboot requested... again.  Fool me once, shame on you.
		 * Fool me twice, shame on me.  Fail into recovery to avoid
		 * a reboot loop.
		 */
		vb2api_fail(ctx, VB2_RECOVERY_RO_TPM_REBOOT, 0);
	} else {
		/* Reboot requested for the first time */
		vb2_nv_set(ctx, VB2_NV_TPM_REQUESTED_REBOOT, 1);
		return VB2_ERROR_API_PHASE1_SECDATA_REBOOT;
	}

	/* Initialize firmware & kernel secure data */
	rv = vb2_secdata_firmware_init(ctx);
	if (rv)
		vb2api_fail(ctx, VB2_RECOVERY_SECDATA_FIRMWARE_INIT, rv);

	rv = vb2_secdata_kernel_init(ctx);
	if (rv)
		vb2api_fail(ctx, VB2_RECOVERY_SECDATA_KERNEL_INIT, rv);

	/* Load and parse the GBB header */
	rv = vb2_fw_init_gbb(ctx);
	if (rv)
		vb2api_fail(ctx, VB2_RECOVERY_GBB_HEADER, rv);

	/*
	 * Check for recovery.  Note that this function returns void, since any
	 * errors result in requesting recovery.  That's also why we don't
	 * return error from failures in the preceding two steps; those
	 * failures simply cause us to detect recovery mode here.
	 */
	vb2_check_recovery(ctx);

	/* Check for dev switch */
	rv = vb2_check_dev_switch(ctx);
	if (rv && !(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		/*
		 * Error in dev switch processing, and we weren't already
		 * headed for recovery mode.  Reboot into recovery mode, since
		 * it's too late to handle those errors this boot, and we need
		 * to take a different path through the dev switch checking
		 * code in that case.
		 */
		vb2api_fail(ctx, VB2_RECOVERY_DEV_SWITCH, rv);
		return rv;
	}

	/*
	 * Check for possible reasons to ask the firmware to make display
	 * available.  VB2_CONTEXT_RECOVERY_MODE may have been set above by
	 * vb2_check_recovery.  VB2_SD_FLAG_DEV_MODE_ENABLED may have been set
	 * above by vb2_check_dev_switch.  VB2_NV_DIAG_REQUEST may have been
	 * set during the last boot in recovery mode.
	 */
	if (!(ctx->flags & VB2_CONTEXT_DISPLAY_INIT) &&
	    (vb2_nv_get(ctx, VB2_NV_DISPLAY_REQUEST) ||
	     sd->flags & VB2_SD_FLAG_DEV_MODE_ENABLED ||
	     ctx->flags & VB2_CONTEXT_RECOVERY_MODE ||
	     vb2_nv_get(ctx, VB2_NV_DIAG_REQUEST)))
		ctx->flags |= VB2_CONTEXT_DISPLAY_INIT;
	/* Mark display as available for downstream vboot and vboot callers. */
	if (ctx->flags & VB2_CONTEXT_DISPLAY_INIT)
		sd->flags |= VB2_SD_FLAG_DISPLAY_AVAILABLE;

	/* Return error if recovery is needed */
	if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) {
		/* Always clear RAM when entering recovery mode */
		ctx->flags |= VB2_CONTEXT_CLEAR_RAM;
		return VB2_ERROR_API_PHASE1_RECOVERY;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_fw_phase2(struct vb2_context *ctx)
{
	/*
	 * Use the slot from the last boot if this is a resume.  Do not set
	 * VB2_SD_STATUS_CHOSE_SLOT so the try counter is not decremented on
	 * failure as we are explicitly not attempting to boot from a new slot.
	 */
	if (ctx->flags & VB2_CONTEXT_S3_RESUME) {
		struct vb2_shared_data *sd = vb2_get_sd(ctx);

		/* Set the current slot to the last booted slot */
		sd->fw_slot = vb2_nv_get(ctx, VB2_NV_FW_TRIED);

		/* Set context flag if we're using slot B */
		if (sd->fw_slot)
			ctx->flags |= VB2_CONTEXT_FW_SLOT_B;

		return VB2_SUCCESS;
	}

	/* Always clear RAM when entering developer mode */
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE)
		ctx->flags |= VB2_CONTEXT_CLEAR_RAM;

	/* Check for explicit request to clear TPM */
	VB2_TRY(vb2_check_tpm_clear(ctx), ctx, VB2_RECOVERY_TPM_CLEAR_OWNER);

	/* Decide which firmware slot to try this boot */
	VB2_TRY(vb2_select_fw_slot(ctx), ctx, VB2_RECOVERY_FW_SLOT);

	return VB2_SUCCESS;
}

vb2_error_t vb2api_extend_hash(struct vb2_context *ctx,
		       const void *buf,
		       uint32_t size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_digest_context *dc = (struct vb2_digest_context *)
		vb2_member_of(sd, sd->hash_offset);

	/* Must have initialized hash digest work area */
	if (!sd->hash_size)
		return VB2_ERROR_API_EXTEND_HASH_WORKBUF;

	/* Don't extend past the data we expect to hash */
	if (!size || size > sd->hash_remaining_size)
		return VB2_ERROR_API_EXTEND_HASH_SIZE;

	sd->hash_remaining_size -= size;

	if (dc->using_hwcrypto)
		return vb2ex_hwcrypto_digest_extend(buf, size);
	else
		return vb2_digest_extend(dc, buf, size);
}

vb2_error_t vb2api_get_pcr_digest(struct vb2_context *ctx,
			  enum vb2_pcr_digest which_digest,
			  uint8_t *dest,
			  uint32_t *dest_size)
{
	const uint8_t *digest;
	uint32_t digest_size;

	switch (which_digest) {
	case BOOT_MODE_PCR:
		digest = vb2_get_boot_state_digest(ctx);
		digest_size = VB2_SHA1_DIGEST_SIZE;
		break;
	case HWID_DIGEST_PCR:
		digest = vb2_get_gbb(ctx)->hwid_digest;
		digest_size = VB2_GBB_HWID_DIGEST_SIZE;
		break;
	default:
		return VB2_ERROR_API_PCR_DIGEST;
	}

	if (digest == NULL || *dest_size < digest_size)
		return VB2_ERROR_API_PCR_DIGEST_BUF;

	memcpy(dest, digest, digest_size);
	if (digest_size < *dest_size)
		memset(dest + digest_size, 0, *dest_size - digest_size);

	*dest_size = digest_size;

	return VB2_SUCCESS;
}

vb2_error_t vb2api_fw_phase3(struct vb2_context *ctx)
{
	/* Verify firmware keyblock */
	VB2_TRY(vb2_load_fw_keyblock(ctx), ctx, VB2_RECOVERY_RO_INVALID_RW);

	/* Verify firmware preamble */
	VB2_TRY(vb2_load_fw_preamble(ctx), ctx, VB2_RECOVERY_RO_INVALID_RW);

	return VB2_SUCCESS;
}

vb2_error_t vb2api_init_hash(struct vb2_context *ctx, uint32_t tag)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	const struct vb2_fw_preamble *pre;
	struct vb2_digest_context *dc;
	struct vb2_public_key key;
	struct vb2_workbuf wb;

	vb2_workbuf_from_ctx(ctx, &wb);

	if (tag == VB2_HASH_TAG_INVALID)
		return VB2_ERROR_API_INIT_HASH_TAG;

	/* Get preamble pointer */
	if (!sd->preamble_size)
		return VB2_ERROR_API_INIT_HASH_PREAMBLE;
	pre = (const struct vb2_fw_preamble *)
		vb2_member_of(sd, sd->preamble_offset);

	/* For now, we only support the firmware body tag */
	if (tag != VB2_HASH_TAG_FW_BODY)
		return VB2_ERROR_API_INIT_HASH_TAG;

	/* Allocate workbuf space for the hash */
	if (sd->hash_size) {
		dc = (struct vb2_digest_context *)
			vb2_member_of(sd, sd->hash_offset);
	} else {
		uint32_t dig_size = sizeof(*dc);

		dc = vb2_workbuf_alloc(&wb, dig_size);
		if (!dc)
			return VB2_ERROR_API_INIT_HASH_WORKBUF;

		sd->hash_offset = vb2_offset_of(sd, dc);
		sd->hash_size = dig_size;
		vb2_set_workbuf_used(ctx, sd->hash_offset + dig_size);
	}

	/*
	 * Work buffer now contains:
	 *   - vb2_shared_data
	 *   - packed firmware data key
	 *   - firmware preamble
	 *   - hash data
	 */

	/*
	 * Unpack the firmware data key to see which hashing algorithm we
	 * should use.
	 *
	 * TODO: really, the firmware body should be hashed, and not signed,
	 * because the signature we're checking is already signed as part of
	 * the firmware preamble.  But until we can change the signing scripts,
	 * we're stuck with a signature here instead of a hash.
	 */
	if (!sd->data_key_size)
		return VB2_ERROR_API_INIT_HASH_DATA_KEY;

	VB2_TRY(vb2_unpack_key_buffer(&key,
				      vb2_member_of(sd, sd->data_key_offset),
				      sd->data_key_size));

	sd->hash_tag = tag;
	sd->hash_remaining_size = pre->body_signature.data_size;

	if (!(pre->flags & VB2_FIRMWARE_PREAMBLE_DISALLOW_HWCRYPTO)) {
		vb2_error_t rv = vb2ex_hwcrypto_digest_init(
			key.hash_alg, pre->body_signature.data_size);
		if (!rv) {
			VB2_DEBUG("Using HW crypto engine for hash_alg %d\n",
				  key.hash_alg);
			dc->hash_alg = key.hash_alg;
			dc->using_hwcrypto = 1;
			return VB2_SUCCESS;
		}
		if (rv != VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED)
			return rv;
		VB2_DEBUG("HW crypto for hash_alg %d not supported, using SW\n",
			  key.hash_alg);
	} else {
		VB2_DEBUG("HW crypto forbidden by preamble, using SW\n");
	}

	return vb2_digest_init(dc, key.hash_alg);
}

vb2_error_t vb2api_check_hash_get_digest(struct vb2_context *ctx,
					 void *digest_out,
					 uint32_t digest_out_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_digest_context *dc = (struct vb2_digest_context *)
		vb2_member_of(sd, sd->hash_offset);
	struct vb2_workbuf wb;

	uint8_t *digest;
	uint32_t digest_size = vb2_digest_size(dc->hash_alg);

	struct vb2_fw_preamble *pre;
	struct vb2_public_key key;

	vb2_workbuf_from_ctx(ctx, &wb);

	/* Get preamble pointer */
	if (!sd->preamble_size)
		return VB2_ERROR_API_CHECK_HASH_PREAMBLE;
	pre = vb2_member_of(sd, sd->preamble_offset);

	/* Must have initialized hash digest work area */
	if (!sd->hash_size)
		return VB2_ERROR_API_CHECK_HASH_WORKBUF;

	/* Should have hashed the right amount of data */
	if (sd->hash_remaining_size)
		return VB2_ERROR_API_CHECK_HASH_SIZE;

	/* Allocate the digest */
	digest = vb2_workbuf_alloc(&wb, digest_size);
	if (!digest)
		return VB2_ERROR_API_CHECK_HASH_WORKBUF_DIGEST;

	/* Finalize the digest */
	if (dc->using_hwcrypto)
		VB2_TRY(vb2ex_hwcrypto_digest_finalize(digest, digest_size));
	else
		VB2_TRY(vb2_digest_finalize(dc, digest, digest_size));

	/* The code below is specific to the body signature */
	if (sd->hash_tag != VB2_HASH_TAG_FW_BODY)
		return VB2_ERROR_API_CHECK_HASH_TAG;

	/*
	 * The body signature is currently a *signature* of the body data, not
	 * just its hash.  So we need to verify the signature.
	 */

	/* Unpack the data key */
	if (!sd->data_key_size)
		return VB2_ERROR_API_CHECK_HASH_DATA_KEY;

	VB2_TRY(vb2_unpack_key_buffer(&key,
				      vb2_member_of(sd, sd->data_key_offset),
				      sd->data_key_size));

	/*
	 * Check digest vs. signature.  Note that this destroys the signature.
	 * That's ok, because we only check each signature once per boot.
	 */
	VB2_TRY(vb2_verify_digest(&key, &pre->body_signature, digest, &wb),
		ctx, VB2_RECOVERY_FW_BODY);

	if (digest_out != NULL) {
		if (digest_out_size < digest_size)
			return VB2_ERROR_API_CHECK_DIGEST_SIZE;
		memcpy(digest_out, digest, digest_size);
	}

	return VB2_SUCCESS;
}

int vb2api_check_hash(struct vb2_context *ctx)
{
	return vb2api_check_hash_get_digest(ctx, NULL, 0);
}

static void uint8_to_string(char *buf, uint8_t val)
{
	const char *trans = "0123456789abcdef";
	*buf++ = trans[val >> 4];
	*buf = trans[val & 0xf];
}

static void fill_in_sha1_sum(char *outbuf, struct vb2_packed_key *key)
{
	uint8_t *buf = ((uint8_t *)key) + key->key_offset;
	uint64_t buflen = key->key_size;
	uint8_t digest[VB2_SHA1_DIGEST_SIZE];
	int i;

	vb2_digest_buffer(buf, buflen, VB2_HASH_SHA1, digest, sizeof(digest));
	for (i = 0; i < sizeof(digest); i++) {
		uint8_to_string(outbuf, digest[i]);
		outbuf += 2;
	}
	*outbuf = '\0';
}

size_t vb2api_get_debug_info(struct vb2_context *ctx,
			     char *dest, size_t dest_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	struct vb2_workbuf wb;
	char sha1sum[VB2_SHA1_DIGEST_SIZE * 2 + 1];
	int32_t used = 0;
	vb2_error_t rv;
	uint32_t i;

	if (dest == NULL || dest_size == 0)
		return 0;

	vb2_workbuf_from_ctx(ctx, &wb);

#define DEBUG_INFO_APPEND(format, args...) do { \
	if (used < dest_size) \
		used += snprintf(dest + used, dest_size - used, format, \
				 ## args); \
} while (0)

	/* Add hardware ID */
	{
		char hwid[VB2_GBB_HWID_MAX_SIZE];
		uint32_t size = sizeof(hwid);
		rv = vb2api_gbb_read_hwid(ctx, hwid, &size);
		if (rv)
			strcpy(hwid, "{INVALID}");
		DEBUG_INFO_APPEND("HWID: %s", hwid);
	}

	/* Add recovery reason and subcode */
	i = vb2_nv_get(ctx, VB2_NV_RECOVERY_SUBCODE);
	DEBUG_INFO_APPEND("\nrecovery_reason: %#.2x / %#.2x  %s",
			  sd->recovery_reason, i,
			  vb2_get_recovery_reason_string(sd->recovery_reason));

	/* Add vb2_context and vb2_shared_data flags */
	DEBUG_INFO_APPEND("\ncontext.flags: %#.16" PRIx64, ctx->flags);
	DEBUG_INFO_APPEND("\nshared_data.flags: %#.8x", sd->flags);
	DEBUG_INFO_APPEND("\nshared_data.status: %#.8x", sd->status);

	/* Add raw contents of nvdata */
	DEBUG_INFO_APPEND("\nnvdata:");
	if (vb2_nv_get_size(ctx) > 16)  /* Multi-line starts on next line */
		DEBUG_INFO_APPEND("\n  ");
	for (i = 0; i < vb2_nv_get_size(ctx); i++) {
		/* Split into 16-byte blocks */
		if (i > 0 && i % 16 == 0)
			DEBUG_INFO_APPEND("\n  ");
		DEBUG_INFO_APPEND(" %02x", ctx->nvdata[i]);
	}

	/* Add dev_boot_usb flag */
	i = vb2_nv_get(ctx, VB2_NV_DEV_BOOT_EXTERNAL);
	DEBUG_INFO_APPEND("\ndev_boot_usb: %d", i);

	/* Add dev_boot_legacy flag */
	i = vb2_nv_get(ctx, VB2_NV_DEV_BOOT_LEGACY);
	DEBUG_INFO_APPEND("\ndev_boot_legacy: %d", i);

	/* Add dev_default_boot flag */
	i = vb2_nv_get(ctx, VB2_NV_DEV_DEFAULT_BOOT);
	DEBUG_INFO_APPEND("\ndev_default_boot: %d", i);

	/* Add dev_boot_signed_only flag */
	i = vb2_nv_get(ctx, VB2_NV_DEV_BOOT_SIGNED_ONLY);
	DEBUG_INFO_APPEND("\ndev_boot_signed_only: %d", i);

	/* Add TPM versions */
	DEBUG_INFO_APPEND("\nTPM: fwver=%#.8x kernver=%#.8x",
			  sd->fw_version_secdata, sd->kernel_version_secdata);

	/* Add GBB flags */
	DEBUG_INFO_APPEND("\ngbb.flags: %#.8x", gbb->flags);

	/* Add sha1sum for Root & Recovery keys */
	{
		struct vb2_packed_key *key;
		struct vb2_workbuf wblocal = wb;
		rv = vb2_gbb_read_root_key(ctx, &key, NULL, &wblocal);
		if (!rv) {
			fill_in_sha1_sum(sha1sum, key);
			DEBUG_INFO_APPEND("\ngbb.rootkey: %s", sha1sum);
		}
	}

	{
		struct vb2_packed_key *key;
		struct vb2_workbuf wblocal = wb;
		rv = vb2_gbb_read_recovery_key(ctx, &key, NULL, &wblocal);
		if (!rv) {
			fill_in_sha1_sum(sha1sum, key);
			DEBUG_INFO_APPEND("\ngbb.recovery_key: %s", sha1sum);
		}
	}

	/* If we're in dev-mode, show the kernel subkey that we expect, too. */
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE) &&
	    sd->kernel_key_offset) {
		struct vb2_packed_key *key =
			vb2_member_of(sd, sd->kernel_key_offset);
		fill_in_sha1_sum(sha1sum, key);
		DEBUG_INFO_APPEND("\nkernel_subkey: %s", sha1sum);
	}

	/* Make sure we finish with a newline */
	DEBUG_INFO_APPEND("\n");

#undef DEBUG_INFO_APPEND

	if (used == dest_size) {
		dest[dest_size - 1] = '\0';
		used--;
	}
	VB2_DEBUG("vboot debug info:\n%s", dest);

	return used;
}
