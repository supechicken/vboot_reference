/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#include "2sysincludes.h"
#include "rollback_index.h"
#include "tss_constants.h"
#include "utility.h"

vb2_error_t vb2_set_developer_mode(struct vb2_context *ctx, int value)
{
	return VB2_SUCCESS;
}

uint32_t TPMClearAndReenable(void)
{
	return TPM_SUCCESS;
}

uint32_t ReadSpaceFirmware(struct vb2_context *ctx)
{
	return VB2_SUCCESS;
}

uint32_t WriteSpaceFirmware(struct vb2_context *ctx);
{
	return VB2_SUCCESS;
}

uint32_t ReadSpaceKernel(struct vb2_context *ctx)
{
	return VB2_SUCCESS;
}

uint32_t WriteSpaceKernel(struct vb2_context *ctx);
{
	return VB2_SUCCESS;
}

uint32_t LockSpaceKernel()
{
	return TPM_SUCCESS;
}

uint32_t ReadSpaceFwmp(struct vb2_secdata_fwmp *fwmp)
{
	memset(fwmp, 0, sizeof(*fwmp));
	return TPM_SUCCESS;
}
