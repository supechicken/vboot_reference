/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Misc functions which need access to vb2_context but are not public APIs
 */

#ifndef VBOOT_REFERENCE_VBOOT_2MISC_H_
#define VBOOT_REFERENCE_VBOOT_2MISC_H_

struct vb2_context;
struct vb2_gbb_header;

/**
 * Read the GBB header.
 *
 * @param ctx		Vboot context
 * @param gbb		Destination for header
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_read_gbb_header(struct vb2_context *ctx, struct vb2_gbb_header *gbb);

/**
 * Handle vboot failure.
 *
 * If the failure occurred after choosing a firmware slot, and the other
 * firmware slot is not known-bad, try the other firmware slot after reboot.
 *
 * If the failure occurred before choosing a firmware slot, or both slots have
 * failed in successive boots, request recovery.
 *
 * @param reason	Recovery reason
 * @param subcode	Recovery subcode
 */
void vb2_fail(struct vb2_context *ctx, uint8_t reason, uint8_t subcode);

/**
 * Set up the verified boot context data.
 *
 * Caller must fill in all of the fields at the top of the structure, then
 * call this function to initialize the context.  This must be done before
 * passing the context to any other vboot function, unless specifically
 * allowed for that function (for example, vb2_secdata_check_crc()).
 *
 * On error, always sets a recovery reason in the context.
 *
 * @param ctx		Vboot context to initialize
 * @return VB2_SUCCESS, or error code on error.
 */
void vb2_init_context(struct vb2_context *ctx);

/**
 * Check for recovery reasons we can determine early in the boot process.
 *
 * On exit, check ctx->flags for VB2_CONTEXT_RECOVERY_MODE; if present, jump to
 * the recovery path instead of continuing with normal boot.  This is the only
 * direct path to recovery mode.  All other errors later in the boot process
 * should induce a reboot instead of jumping to recovery, so that recovery mode
 * starts from a consistent firmware state.
 *
 * @param ctx		Vboot context
 */
void vb2_check_recovery(struct vb2_context *ctx);

/**
 * Parse the GBB header.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_fw_parse_gbb(struct vb2_context *ctx);

/**
 * Check developer switch position.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_check_dev_switch(struct vb2_context *ctx);

/**
 * Check if we need to clear the TPM owner.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_check_tpm_clear(struct vb2_context *ctx);

/**
 * Decide which firmware slot to try this boot.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_select_fw_slot(struct vb2_context *ctx);

/**
 * Verify the firmware keyblock using the root key.
 *
 * After this call, the data key is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_verify_fw_keyblock(struct vb2_context *ctx);

/**
 * Verify the firmware preamble using the data subkey from the keyblock.
 *
 * After this call, the preamble is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_verify_fw_preamble2(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_VBOOT_2MISC_H_ */
