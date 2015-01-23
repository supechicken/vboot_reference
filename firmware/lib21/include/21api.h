/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_21_API_H_
#define VBOOT_REFERENCE_21_API_H_

/******************************************************************************
 * Exported APIs provided by verified boot.
 *
 * At a high level, call functions in the order described below.  After each
 * call, examine vb2_context.flags to determine whether nvdata or secdata
 * needs to be written.
 *
 * If you need to cause the boot process to fail at any point, call
 * vb21api_fail().  Then check vb2_context.flags to see what data needs to be
 * written.  Then reboot.
 *
 *	Load nvdata from wherever you keep it.
 *
 *	Load secdata from wherever you keep it.
 *
 *		If it wasn't there at all (for example, this is the first boot
 *		of a new system in the factory), call vb21api_secdata_create()
 *		to initialize the data.
 *
 *		If access to your storage is unreliable (reads/writes may
 *		contain corrupt data), you may call vb21api_secdata_check() to
 *		determine if the data was valid, and retry reading if it
 *		wasn't.  (In that case, you should also read back and check the
 *		data after any time you write it, to make sure it was written
 *		correctly.)
 *
 *	Call vb21api_fw_phase1().  At present, this nominally decides whether
 *	recovery mode is needed this boot.
 *
 *	Call vb21api_fw_phase2().  At present, this nominally decides which
 *	firmware slot will be attempted (A or B).
 *
 *	Call vb21api_fw_phase3().  At present, this nominally verifies the
 *	firmware keyblock and preamble.
 *
 *	Lock down wherever you keep secdata.  It should no longer be writable
 *	this boot.
 *
 *	Verify the hash of each section of code/data you need to boot the RW
 *	firmware.  For each section:
 *
 *		Call vb2_init_hash() to see if the hash exists.
 *
 *		Load the data for the section.  Call vb2_extend_hash() on the
 *		data as you load it.  You can load it all at once and make one
 *		call, or load and hash-extend a block at a time.
 *
 *		Call vb2_check_hash() to see if the hash is valid.
 *
 *			If it is valid, you may use the data and/or execute
 *			code from that section.
 *
 *			If the hash was invalid, you must reboot.
 *
 * At this point, firmware verification is done, and vb2_context contains the
 * kernel key needed to verify the kernel.  That context should be preserved
 * and passed on to kernel selection.  For now, that requires translating it
 * into the old VbSharedData format (via a func which does not yet exist...)
 */

/**
 * Sanity-check the contents of the secure storage context.
 *
 * Use this if reading from secure storage may be flaky, and you want to retry
 * reading it several times.
 *
 * This may be called before vb21api_phase1().
 *
 * @param ctx		Context pointer
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static inline int vb21api_secdata_check(const struct vb2_context *ctx)
{
	return vb2x_secdata_check(ctx);
}

/**
 * Create fresh data in the secure storage context.
 *
 * Use this only when initializing the secure storage context on a new machine
 * the first time it boots.  Do NOT simply use this if vb21api_secdata_check()
 * (or any other API in this library) fails; that could allow the secure data
 * to be rolled back to an insecure state.
 *
 * This may be called before vb21api_phase1().
 *
 * @param ctx		Context pointer
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static inline int vb21api_secdata_create(struct vb2_context *ctx)
{
	return vb2x_secdata_create(ctx);
}

/**
 * Report firmware failure to vboot.
 *
 * This may be called before vb21api_phase1() to indicate errors in the boot
 * process prior to the start of vboot.
 *
 * If this is called after vb21api_phase1(), on return, the calling firmware
 * should check for updates to secdata and/or nvdata, then reboot.
 *
 * @param reason	Recovery reason
 * @param subcode	Recovery subcode
 */
static inline void vb21api_fail(struct vb2_context *ctx,
				uint8_t reason, uint8_t subcode)
{
	vb2x_fail(ctx, reason, subcode);
}

/**
 * Firmware selection, phase 1.
 *
 * On error, the calling firmware should jump directly to recovery-mode
 * firmware without rebooting.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
static inline int vb21api_fw_phase1(struct vb2_context *ctx)
{
	return vb2x_fw_phase1(ctx);
}

/**
 * Firmware selection, phase 2.
 *
 * On error, the calling firmware should check for updates to secdata and/or
 * nvdata, then reboot.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
static inline int vb21api_fw_phase2(struct vb2_context *ctx)
{
	return vb2x_fw_phase2(ctx);
}

/**
 * Firmware selection, phase 3.
 *
 * On error, the calling firmware should check for updates to secdata and/or
 * nvdata, then reboot.
 *
 * On success, the calling firmware should lock down secdata before continuing
 * with the boot process.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb21api_fw_phase3(struct vb2_context *ctx);

/**
 * Initialize hashing data for the specified guid.
 *
 * @param ctx		Vboot context
 * @param guid		guid to start hashing
 * @param size		If non-null, expected size of data for guid will be
 *			stored here on output.
 * @return VB2_SUCCESS, or error code on error.
 */
int vb21api_init_hash(struct vb2_context *ctx,
		      const struct vb2_guid *guid, uint32_t *size);

/**
 * Extend the hash started by vb21api_init_hash() with additional data.
 *
 * (This is the same for both old and new style structs.)
 *
 * @param ctx		Vboot context
 * @param buf		Data to hash
 * @param size		Size of data in bytes
 * @return VB2_SUCCESS, or error code on error.
 */
static inline int vb21api_extend_hash(struct vb2_context *ctx,
				      const void *buf, uint32_t size)
{
	return vb2x_extend_hash(ctx, buf, size);
}

/**
 * Check the hash value started by vb21api_init_hash().
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb21api_check_hash(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_21_API_H_ */
