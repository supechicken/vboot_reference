/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2crc8.h"
#include "2secdata.h"
#include "rollback_index.h"
#include "tlcl.h"
#include "tss_constants.h"
#include "vboot_api.h"

#ifdef FOR_TEST
/*
 * Compiling for unit test, so we need the real implementations of
 * rollback functions.  The unit test mocks the underlying tlcl
 * functions, so this is ok to run on the host.
 */
#undef CHROMEOS_ENVIRONMENT
#undef DISABLE_ROLLBACK_TPM
#endif

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
uint32_t ReadSpaceFirmware(RollbackSpaceFirmware *rsf)
{
	uint32_t r;

	r = TlclRead(FIRMWARE_NV_INDEX, rsf, sizeof(RollbackSpaceFirmware));
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdata returned 0x%x\n", r);
		return r;
	}
	PRINT_BYTES("TPM: read secdata", rsf);

	return TPM_SUCCESS;
}

uint32_t WriteSpaceFirmware(RollbackSpaceFirmware *rsf)
{
	uint32_t r;

	PRINT_BYTES("TPM: write secdata", rsf);
	r = SafeWrite(FIRMWARE_NV_INDEX, rsf, sizeof(RollbackSpaceFirmware));
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdata failure\n");
		return r;
	}

	return TPM_SUCCESS;
}

/* NOTE: SetVirtualDevMode doesn't update the SECDATA_FLAG_LAST_BOOT_DEVELOPER
   bit.  That will be done on the next boot. */
vb2_error_t SetVirtualDevMode(struct vb2_context *ctx, int value)
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

uint32_t ReadSpaceKernel(RollbackSpaceKernel *rsk)
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

	r = TlclRead(KERNEL_NV_INDEX, rsk, sizeof(RollbackSpaceKernel));
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read secdatak returned 0x%x\n", r);
		return r;
	}
	PRINT_BYTES("TPM: read secdatak", rsk);

	return TPM_SUCCESS;
}

uint32_t WriteSpaceKernel(RollbackSpaceKernel *rsk)
{
	uint32_t r;

	PRINT_BYTES("TPM: write secdatak", rsk);
	r = SafeWrite(KERNEL_NV_INDEX, rsk, sizeof(RollbackSpaceKernel));
	if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: write secdatak failure\n");
		return r;
	}

	return TPM_SUCCESS;
}

#ifdef DISABLE_ROLLBACK_TPM
/* Dummy implementations which don't support TPM rollback protection */

uint32_t RollbackKernelLock(void)
{
	return TPM_SUCCESS;
}

uint32_t RollbackFwmpRead(struct vb2_context *ctx)
{
	return TPM_E_BADINDEX;
}

#else

uint32_t RollbackKernelLock(void)
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

uint32_t RollbackFwmpRead(struct vb2_context *ctx)
{
	struct RollbackSpaceFwmp *fwmp =
		(struct RollbackSpaceFwmp *)&ctx->secdata_fwmp;
	uint32_t size = sizeof(*fwmp);
	uint32_t r;

	/* Try to read entire 1.0 struct */
	r = TlclRead(FWMP_NV_INDEX, fwmp, size);
	if (TPM_E_BADINDEX == r) {
		memset(fwmp, 0, sizeof(*fwmp));
		VB2_DEBUG("TPM: FWMP space does not exist\n");
		return TPM_SUCCESS;
	} else if (TPM_SUCCESS != r) {
		VB2_DEBUG("TPM: read FWMP returned 0x%x\n", r);
		return r;
	}

	/* Re-read more data if necessary */
	if (vb2api_secdata_fwmp_check(ctx, &size) ==
	    VB2_ERROR_SECDATA_FWMP_INCOMPLETE) {
		r = TlclRead(FWMP_NV_INDEX, fwmp, size);
		if (TPM_SUCCESS != r) {
			VB2_DEBUG("TPM: re-read FWMP returned 0x%x\n", r);
			return r;
		}
	}

	return TPM_SUCCESS;
}

#endif /* DISABLE_ROLLBACK_TPM */
