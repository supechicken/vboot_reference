/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#include "sysincludes.h"
#include "utility.h"

#include "rollback_index.h"

#include "tss_constants.h"


vb2_error_t SetVirtualDevMode(struct vb2_context *ctx, int value)
{
	return VB2_SUCCESS;
}

uint32_t TPMClearAndReenable(void)
{
	return TPM_SUCCESS;
}

uint32_t ReadSpaceFirmware(RollbackSpaceFirmware *rsf)
{
	return VB2_SUCCESS;
}

uint32_t WriteSpaceFirmware(RollbackSpaceFirmware *rsf);
{
	return VB2_SUCCESS;
}

uint32_t ReadSpaceKernel(RollbackSpaceKernel *rsk)
{
	return VB2_SUCCESS;
}

uint32_t WriteSpaceKernel(RollbackSpaceKernel *rsk);
{
	return VB2_SUCCESS;
}

uint32_t RollbackKernelLock()
{
	return TPM_SUCCESS;
}

uint32_t RollbackFwmpRead(struct RollbackSpaceFwmp *fwmp)
{
	memset(fwmp, 0, sizeof(*fwmp));
	return TPM_SUCCESS;
}
