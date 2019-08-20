/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#include "2common.h"
#include "2crc8.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sysincludes.h"
#include "rollback_index.h"
#include "tlcl.h"
#include "tss_constants.h"
#include "vboot_api.h"

#define RETURN_ON_FAILURE(tpm_command) do { \
		uint32_t result_; \
		if (TPM_SUCCESS != (result_ = (tpm_command))) { \
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

uint32_t TPMClearAndReenable(void)
{
	VB2_DEBUG("TPM: clear and re-enable\n");
	RETURN_ON_FAILURE(TlclForceClear());
	RETURN_ON_FAILURE(TlclSetEnable());
	RETURN_ON_FAILURE(TlclSetDeactivated(0));

	return TPM_SUCCESS;
}

uint32_t SafeWrite(uint32_t index, const void *data, uint32_t length)
{
	uint32_t result = TlclWrite(index, data, length);
	if (result == TPM_E_MAXNVWRITES) {
		RETURN_ON_FAILURE(TPMClearAndReenable());
		return TlclWrite(index, data, length);
	} else {
		return result;
	}
}

/* Functions to read and write firmware and kernel spaces. */

uint32_t ReadSpaceFirmware(struct vb2_context *ctx)
{
	uint32_t r;

	PRINT_BYTES("TPM: read secdata", ctx->secdata);
	r = TlclRead(FIRMWARE_NV_INDEX, ctx->secdata, VB2_SECDATA_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdata returned 0x%x\n", r);
		return r;
	}

	return TPM_SUCCESS;
}

uint32_t WriteSpaceFirmware(struct vb2_context *ctx)
{
	uint32_t r;

	PRINT_BYTES("TPM: write secdata", ctx->secdata);
	r = SafeWrite(FIRMWARE_NV_INDEX, ctx->secdata, VB2_SECDATA_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdata returned 0x%x\n", r);
		return r;
	}

	return TPM_SUCCESS;
}

uint32_t ReadSpaceKernel(struct vb2_context *ctx)
{
	uint32_t r;

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
		VB2_DEBUG("TPM: get secdatak permissions returned 0x%x\n", r);
		return r;
	}

	if (perms != TPM_NV_PER_PPWRITE)
		return TPM_E_CORRUPTED_STATE;
#endif

	PRINT_BYTES("TPM: read secdatak", ctx->secdatak);
	r = TlclRead(KERNEL_NV_INDEX, ctx->secdatak, VB2_SECDATAK_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdatak returned 0x%x\n", r);
		return r;
	}

	return TPM_SUCCESS;
}

uint32_t WriteSpaceKernel(struct vb2_context *ctx)
{
	uint32_t r;

	PRINT_BYTES("TPM: write secdatak", ctx->secdatak);
	r = SafeWrite(KERNEL_NV_INDEX, ctx->secdatak, VB2_SECDATAK_SIZE);
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdatak returned 0x%x\n", r);
		return r;
	}

	return TPM_SUCCESS;
}

uint32_t LockSpaceKernel(void)
{
	static int kernel_locked = 0;
	uint32_t r;

	if (kernel_locked)
		return TPM_SUCCESS;

	r = TlclLockPhysicalPresence();
	if (TPM_SUCCESS == r)
		kernel_locked = 1;

	VB2_DEBUG("TPM: lock secdatak returned 0x%x\n", r);
	return r;
}

uint32_t ReadSpaceFwmp(struct vb2_context *ctx)
{
	uint32_t size = VB2_SECDATA_FWMP_MIN_SIZE;
	uint32_t r;

	/* Try to read entire 1.0 struct */
	r = TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp, size);
	if (TPM_E_BADINDEX == r) {
		memset(ctx->secdata_fwmp, 0, VB2_SECDATA_FWMP_MAX_SIZE);
		VB2_DEBUG("TPM: FWMP space does not exist\n");
		return TPM_SUCCESS;
	} else if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read FWMP returned 0x%x\n", r);
		return r;
	}

	/* Re-read more data if necessary */
	if (vb2api_secdata_fwmp_check(ctx, &size) ==
	    VB2_ERROR_SECDATA_FWMP_INCOMPLETE) {
		r = TlclRead(FWMP_NV_INDEX, ctx->secdata_fwmp, size);
		if (TPM_SUCCESS != r) {
			VB2_DEBUG("TPM: re-read FWMP returned 0x%x\n", r);
			return r;
		}
	}

	return TPM_SUCCESS;
}

vb2_error_t vb2_secdata_load(struct vb2_context *ctx)
{
	if (ReadSpaceFirmware(ctx)) {
		VB2_DEBUG("Error reading secdata\n");
		return VB2_RECOVERY_RW_TPM_R_ERROR;
	}

	return vb2_secdata_init(ctx);
}

vb2_error_t vb2_secdata_commit(struct vb2_context *ctx)
{
	if (!(ctx->flags & VB2_CONTEXT_SECDATA_CHANGED))
		return VB2_SUCCESS;

	if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) {
		VB2_DEBUG("Error: secdata modified in non-recovery mode?\n");
		return VB2_ERROR_UNKNOWN;
	}

	VB2_DEBUG("Saving secdata\n");
	if (WriteSpaceFirmware(ctx)) {
		VB2_DEBUG("Error writing secdata\n");
		return VB2_ERROR_UNKNOWN;
	}
	ctx->flags &= ~VB2_CONTEXT_SECDATA_CHANGED;

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdatak_load(struct vb2_context *ctx)
{
	if (ReadSpaceKernel(ctx)) {
		VB2_DEBUG("Error reading secdatak\n");
		return VB2_RECOVERY_RW_TPM_R_ERROR;
	}

	return vb2_secdatak_init(ctx);
}

vb2_error_t vb2_secdatak_commit(struct vb2_context *ctx)
{
	vb2_error_t rv = VB2_SUCCESS;

	if (ctx->flags & VB2_CONTEXT_SECDATAK_CHANGED) {
		VB2_DEBUG("Saving secdatak\n");
		if (WriteSpaceKernel(ctx)) {
			VB2_DEBUG("Error writing secdatak\n");
			rv = VB2_ERROR_UNKNOWN;
		}
		ctx->flags &= ~VB2_CONTEXT_SECDATAK_CHANGED;
	}

	/* Lock secdatak if not in recovery mode */
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE)) {
		VB2_DEBUG("Locking secdatak\n");
		if (LockSpaceKernel()) {
			VB2_DEBUG("Error locking secdatak\n");
			vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST,
				   VB2_RECOVERY_RW_TPM_L_ERROR);
			rv = VB2_ERROR_UNKNOWN;
		}
	}

	return rv;
}

/* NOTE: vb2_set_developer_mode doesn't update the LAST_BOOT_DEVELOPER secdata
   flag.  That will be done on the next boot. */
vb2_error_t vb2_set_developer_mode(struct vb2_context *ctx, int value)
{
	uint32_t flags;

	VB2_DEBUG("Enabling developer mode...\n");

	if (vb2_secdata_get(ctx, VB2_SECDATA_FLAGS, &flags))
		return VBERROR_TPM_FIRMWARE_SETUP;

	if (value)
		flags |= VB2_SECDATA_FLAG_DEV_MODE;
	else
		flags &= ~VB2_SECDATA_FLAG_DEV_MODE;

	if (vb2_secdata_set(ctx, VB2_SECDATA_FLAGS, flags))
		return VBERROR_TPM_SET_BOOT_MODE_STATE;

	VB2_DEBUG("Mode change will take effect on next reboot\n");

	return VB2_SUCCESS;
}
