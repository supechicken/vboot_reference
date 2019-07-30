/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking secure data spaces
 * stored in the TPM NVRAM.
 */

#include "2common.h"
#include "2crc8.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sysincludes.h"
#include "secdata_tpm.h"
#include "tlcl.h"
#include "tss_constants.h"
#include "vboot_api.h"

#define RETURN_ON_FAILURE(tpm_command) do { \
		uint32_t result_; \
		if ((result_ = (tpm_command)) != TPM_SUCCESS) { \
			VB2_DEBUG("TPM: 0x%x returned by " #tpm_command \
				  "\n", (int)result_); \
			return result_; \
		} \
	} while (0)

#define PRINT_BYTES(title, value) do { \
		int i; \
		VB2_DEBUG(title); \
		VB2_DEBUG_RAW(":"); \
		for (i = 0; i < sizeof(*(value)); i++) \
			VB2_DEBUG_RAW(" %02x", *((uint8_t *)(value) + i)); \
		VB2_DEBUG_RAW("\n"); \
	} while (0)

/**
 * Issue a TPM_Clear and reenable/reactivate the TPM.
 */
/* Extern function for testing */
extern uint32_t tpm_clear_and_reenable(void);
uint32_t tpm_clear_and_reenable(void)
{
	VB2_DEBUG("TPM: clear and re-enable\n");
	RETURN_ON_FAILURE(TlclForceClear());
	RETURN_ON_FAILURE(TlclSetEnable());
	RETURN_ON_FAILURE(TlclSetDeactivated(0));

	return TPM_SUCCESS;
}

/**
 * Like TlclWrite(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */
/* Extern function for testing */
extern uint32_t tlcl_safe_write(uint32_t index, const void *data,
				uint32_t length);
uint32_t tlcl_safe_write(uint32_t index, const void *data, uint32_t length)
{
	uint32_t result = TlclWrite(index, data, length);
	if (result == TPM_E_MAXNVWRITES) {
		RETURN_ON_FAILURE(tpm_clear_and_reenable());
		return TlclWrite(index, data, length);
	} else {
		return result;
	}
}

/* Functions to read and write firmware and kernel spaces. */

uint32_t secdata_firmware_read(struct vb2_context *ctx)
{
	uint32_t r;

	VB2_DEBUG("TPM: secdata_firmware_read\n");

	r = TlclRead(FIRMWARE_NV_INDEX, ctx->secdata_firmware,
		     VB2_SECDATA_FIRMWARE_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdata_firmware returned 0x%x\n", r);
		return r;
	}
	PRINT_BYTES("TPM: read secdata_firmware", &ctx->secdata_firmware);

	if (vb2api_secdata_firmware_check(ctx))
		return TPM_E_CORRUPTED_STATE;

	return TPM_SUCCESS;
}

uint32_t secdata_firmware_write(struct vb2_context *ctx)
{
	uint32_t r;

	VB2_DEBUG("TPM: secdata_firmware_write\n");

	if (!(ctx->flags & VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED))
		return TPM_SUCCESS;

	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		VB2_DEBUG("Error: secdata_firmware modified "
			  "in non-recovery mode?\n");
		return TPM_E_AREA_LOCKED;
	}

	PRINT_BYTES("TPM: write secdata_firmware", &ctx->secdata_firmware);
	r = tlcl_safe_write(FIRMWARE_NV_INDEX, ctx->secdata_firmware,
			    VB2_SECDATA_FIRMWARE_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdata_firmware returned 0x%x\n", r);
		return r;
	}

	ctx->flags &= ~VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_read(struct vb2_context *ctx)
{
	uint32_t r;

	VB2_DEBUG("TPM: secdata_kernel_read\n");

#ifndef TPM2_MODE
	/*
	 * Before reading the kernel space, verify its permissions.  If the
	 * kernel space has the wrong permission, we give up.  This will need
	 * to be fixed by the recovery kernel.  We will have to worry about
	 * this because at any time (even with PP turned off) the TPM owner can
	 * remove and redefine a PP-protected space (but not write to it).
	 */
	uint32_t perms;

	r = TlclGetPermissions(KERNEL_NV_INDEX, &perms);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: get secdata_kernel permissions returned 0x%x\n",
			  r);
		return r;
	}

	if (perms != TPM_NV_PER_PPWRITE)
		return TPM_E_CORRUPTED_STATE;
#endif

	r = TlclRead(KERNEL_NV_INDEX, ctx->secdata_kernel,
		     VB2_SECDATA_KERNEL_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdata_kernel returned 0x%x\n", r);
		return r;
	}
	PRINT_BYTES("TPM: read secdata_kernel", &ctx->secdata_kernel);

	if (vb2api_secdata_kernel_check(ctx))
		return TPM_E_CORRUPTED_STATE;

	return TPM_SUCCESS;
}

uint32_t secdata_kernel_write(struct vb2_context *ctx)
{
	uint32_t r;

	VB2_DEBUG("TPM: secdata_kernel_write\n");

	if (!(ctx->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED))
		return TPM_SUCCESS;

	PRINT_BYTES("TPM: write secdata_kernel", &ctx->secdata_kernel);
	r = tlcl_safe_write(KERNEL_NV_INDEX, ctx->secdata_kernel,
			    VB2_SECDATA_KERNEL_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdata_kernel returned 0x%x\n", r);
		return r;
	}

	ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	return TPM_SUCCESS;
}

uint32_t secdata_kernel_lock(struct vb2_context *ctx)
{
	uint32_t r;

	VB2_DEBUG("TPM: secdata_kernel_lock\n");

	/* Skip if in recovery mode */
	if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) {
		VB2_DEBUG("TPM: skip locking secdata_kernel "
			  "in recovery mode\n");
		return TPM_SUCCESS;
	}

	r = TlclLockPhysicalPresence();
	if (r) {
		VB2_DEBUG("TPM: lock secdata_kernel returned 0x%x\n", r);
		vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST,
			   VB2_RECOVERY_RW_TPM_L_ERROR);
	}

	return r;
}

uint32_t secdata_fwmp_read(struct vb2_context *ctx)
{
	vb2_error_t rv;
	uint8_t size = VB2_SECDATA_FWMP_MIN_SIZE;
	uint32_t r;

	VB2_DEBUG("TPM: secdata_fwmp_read\n");

	/* Try to read entire 1.0 struct */
	r = TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp, size);
	if (TPM_E_BADINDEX == r) {
		vb2api_secdata_fwmp_create(ctx);
		VB2_DEBUG("TPM: FWMP space does not exist\n");
		return TPM_SUCCESS;
	} else if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read FWMP returned 0x%x\n", r);
		return r;
	}

	/* Re-read more data if necessary */
	rv = vb2api_secdata_fwmp_check(ctx, &size);
	if (rv == VB2_ERROR_SECDATA_FWMP_INCOMPLETE) {
		r = TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp, size);
		if (TPM_SUCCESS != r) {
			VB2_DEBUG("TPM: re-read FWMP returned 0x%x\n", r);
			return r;
		}

		/* Check one more time */
		if (vb2api_secdata_fwmp_check(ctx, &size))
			return TPM_E_CORRUPTED_STATE;
		return TPM_SUCCESS;
	} else if (rv) {
		return TPM_E_CORRUPTED_STATE;
	}

	return TPM_SUCCESS;
}
