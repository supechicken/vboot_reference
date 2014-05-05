/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Externally-callable APIs
 * (Firmware portion)
 */

#include "2sysincludes.h"
#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sha.h"
#include "2rsa.h"

int vb2api_secdata_check(const struct vb2_context *ctx)
{
	return vb2_secdata_check_crc(ctx);
}

int vb2api_secdata_create(struct vb2_context *ctx)
{
	return vb2_secdata_create(ctx);
}

void vb2api_fail(struct vb2_context *ctx, uint8_t reason, uint8_t subcode)
{
	/* Initialize the vboot context if it hasn't been yet */
	vb2_init_context(ctx);

	vb2_fail(ctx, reason, subcode);
}

int vb2api_fw_phase1(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;
	int rv;

	/* Initialize the vboot context if it hasn't been yet */
	vb2_init_context(ctx);

	/* Initialize NV context */
	vb2_nv_init(ctx);

	/* Initialize secure data */
	rv = vb2_secdata_init(ctx);
	if (rv)
		sd->recovery_reason = VB2_RECOVERY_SECDATA_INIT;

	/*
	 * Check for recovery.  Note that this function returns void, since
	 * any errors result in requesting recovery.
	 */
	vb2_check_recovery(ctx);

	/* Return error if recovery is needed */
	if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE)
		return VB2_ERROR_UNKNOWN;

	return VB2_SUCCESS;
}

int vb2api_fw_phase2(struct vb2_context *ctx)
{
	int rv;

	/* Load and parse the GBB header */
	rv = vb2_fw_parse_gbb(ctx);
	if (rv) {
		vb2_fail(ctx, VB2_RECOVERY_GBB_HEADER, rv);
		return rv;
	}

	/* Check for dev switch */
	rv = vb2_check_dev_switch(ctx);
	if (rv) {
		vb2_fail(ctx, VB2_RECOVERY_DEV_SWITCH, rv);
		return rv;
	}

	/* Check for explicit request to clear TPM */
	rv = vb2_check_tpm_clear(ctx);
	if (rv) {
		vb2_fail(ctx, VB2_RECOVERY_TPM_CLEAR_OWNER, rv);
		return rv;
	}

	/* Decide which firmware slot to try this boot */
	rv = vb2_select_fw_slot(ctx);
	if (rv) {
		vb2_fail(ctx, VB2_RECOVERY_FW_SLOT, rv);
		return rv;
	}

	return VB2_SUCCESS;
}

int vb2api_fw_phase3(struct vb2_context *ctx)
{
	int rv;

	/* Verify firmware keyblock */
	rv = vb2_verify_fw_keyblock(ctx);
	if (rv) {
		vb2_fail(ctx, VBNV_RECOVERY_RO_INVALID_RW, rv);
		return rv;
	}

	/* Verify firmware preamble */
	rv = vb2_verify_fw_preamble2(ctx);
	if (rv) {
		vb2_fail(ctx, VBNV_RECOVERY_RO_INVALID_RW, rv);
		return rv;
	}

	return VB2_SUCCESS;
}

int vb2api_init_hash(struct vb2_context *ctx, uint32_t tag, uint32_t *size)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;
	const struct vb2_fw_preamble *pre;
	struct vb2_digest_context *dc;
	struct vb2_public_key key;
	int rv;

	if (tag == VB2_HASH_TAG_INVALID)
		return VB2_ERROR_BAD_TAG;

	/* Get preamble pointer */
	if (!sd->workbuf_preamble_size)
		return VB2_ERROR_UNKNOWN;
	pre = (const struct vb2_fw_preamble *)
		(ctx->workbuf + sd->workbuf_preamble_offset);

	/* For now, we only support the firmware body tag */
	if (tag != VB2_HASH_TAG_FW_BODY)
		return VB2_ERROR_BAD_TAG;

	/* Allocate workbuf space for the hash */
	if (!sd->workbuf_hash_size) {
		uint32_t dig_size = sizeof(*dc);

		if (ctx->workbuf_used + dig_size > ctx->workbuf_size)
			return VB2_ERROR_WORKBUF_TOO_SMALL;

		sd->workbuf_hash_offset = ctx->workbuf_used;
		sd->workbuf_hash_size = dig_size;
		ctx->workbuf_used += dig_size;
		dc = (struct vb2_digest_context *)
			(ctx->workbuf + sd->workbuf_hash_offset);
	}

	/*
	 * Unpack the firmware data key to see which hashing algorithm we
	 * should use.
	 */
	// TODO: the algorithm should be part of VbSignature
	// TODO: and this should only be a hash, not a full signature
	if (!sd->workbuf_data_key_size)
		return VB2_ERROR_UNKNOWN;

	rv = vb2_unpack_key(&key,
			    ctx->workbuf + sd->workbuf_data_key_offset,
			    sd->workbuf_data_key_size);
	if (rv)
		return rv;

	sd->hash_tag = tag;
	sd->hash_remaining_size = pre->body_signature.data_size;

	if (size)
		*size = pre->body_signature.data_size;

	return vb2_digest_init(dc, key.algorithm);
}

int vb2api_extend_hash(struct vb2_context *ctx,
		       const void *buf,
		       uint32_t size)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;
	struct vb2_digest_context *dc = (struct vb2_digest_context *)
		(ctx->workbuf + sd->workbuf_hash_offset);

	/* Must have initialized hash digest work area */
	if (!sd->workbuf_hash_size)
		return VB2_ERROR_UNKNOWN;

	/* Don't extend past the data we expect to hash */
	if (!size || size > sd->hash_remaining_size)
		return VB2_ERROR_UNKNOWN;

	sd->hash_remaining_size -= size;

	return vb2_digest_extend(dc, buf, size);
}

int vb2api_check_hash(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;
	struct vb2_digest_context *dc = (struct vb2_digest_context *)
		(ctx->workbuf + sd->workbuf_hash_offset);
	uint32_t workbuf_next = ctx->workbuf_used;

	uint8_t *digest;
	uint32_t digest_size = vb2_digest_size(dc->algorithm);

	struct vb2_fw_preamble *pre;
	struct vb2_public_key key;
	int rv;

	/* Get preamble pointer */
	if (!sd->workbuf_preamble_size)
		return VB2_ERROR_UNKNOWN;
	pre = (struct vb2_fw_preamble *)
		(ctx->workbuf + sd->workbuf_preamble_offset);

	/* Must have initialized hash digest work area */
	if (!sd->workbuf_hash_size)
		return VB2_ERROR_UNKNOWN;

	/* Should have hashed the right amount of data */
	if (sd->hash_remaining_size)
		return VB2_ERROR_UNKNOWN;

	/* Make sure there's space in the workbuf for the digest */
	if (workbuf_next + digest_size > ctx->workbuf_size)
		return VB2_ERROR_WORKBUF_TOO_SMALL;
	digest = ctx->workbuf + workbuf_next;
	workbuf_next += digest_size;

	/* Finalize the digest */
	rv = vb2_digest_finalize(dc, digest, digest_size);
	if (rv)
		return rv;

	/* The code below is specific to the body signature */
	if (sd->hash_tag != VB2_HASH_TAG_FW_BODY)
		return VB2_ERROR_BAD_TAG;

	/*
	 * The body signature is currently a *signature* of the body data, not
	 * just its hash.  So we need to verify the signature.
	 */
	// TODO: fix that in the signing process

	/* Unpack the data key */
	if (!sd->workbuf_data_key_size)
		return VB2_ERROR_UNKNOWN;

	rv = vb2_unpack_key(&key,
			    ctx->workbuf + sd->workbuf_data_key_offset,
			    sd->workbuf_data_key_size);
	if (rv)
		return rv;

	/*
	 * Check digest vs. signature.  Note that this destroys the signature.
	 * That's ok, because we only check each signature once per boot.
	 */
	// TODO: should make additional checks on key and signature data like
	// vb2_verify_data() does, or maybe wrap that and this into one func.
	rv = vb2_verify_digest(&key,
			       vb2_signature_data(&pre->body_signature),
			       digest,
			       ctx->workbuf + workbuf_next,
			       ctx->workbuf_size - workbuf_next);
	return rv;
}

int vb2api_get_kernel_subkey(struct vb2_context *ctx,
			     uint8_t *buf,
			     uint32_t *size)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;
	struct vb2_fw_preamble *pre;
	struct vb2_packed_key *kd = (struct vb2_packed_key *)buf;
	uint8_t *src;

	/* Clear size in case there are errors before we determine the size */
	*size = 0;

	/* Get preamble pointer */
	if (!sd->workbuf_preamble_size)
		return VB2_ERROR_UNKNOWN;
	pre = (struct vb2_fw_preamble *)
		(ctx->workbuf + sd->workbuf_preamble_offset);
	src = (uint8_t *)&pre->kernel_subkey;

	/* Update the total destination size */
	*size = pre->kernel_subkey.key_offset + pre->kernel_subkey.key_size;

	/* Make sure the destination is big enough */
	if (*size < sizeof(pre->kernel_subkey) + pre->kernel_subkey.key_size)
		return VB2_ERROR_BUFFER_TOO_SMALL;

	/* Copy the key struct */
	memcpy(kd, src, sizeof(*kd));

	/* Copy the key data */
	memcpy(kd + 1, src + kd->key_offset, kd->key_size);

	/* Update the offset to immediately follow the key */
	kd->key_offset = sizeof(*kd);

	return VB2_SUCCESS;
}
