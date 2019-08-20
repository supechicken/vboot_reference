/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#ifndef VBOOT_REFERENCE_ROLLBACK_INDEX_H_
#define VBOOT_REFERENCE_ROLLBACK_INDEX_H_

#include "2common.h"
#include "2constants.h"
#include "2secdata.h"
#include "tss_constants.h"

/* TPM NVRAM location indices. */
#define FIRMWARE_NV_INDEX 0x1007
#define KERNEL_NV_INDEX 0x1008
#define FWMP_NV_INDEX 0x100a

/* All functions return TPM_SUCCESS (zero) if successful, non-zero if error */

uint32_t ReadSpaceFirmware(struct vb2_context *ctx);
uint32_t WriteSpaceFirmware(struct vb2_context *ctx);

uint32_t ReadSpaceKernel(struct vb2_context *ctx);
uint32_t WriteSpaceKernel(struct vb2_context *ctx);
uint32_t LockSpaceKernel(void);

uint32_t ReadSpaceFwmp(struct vb2_context *ctx);

vb2_error_t vb2_secdata_load(struct vb2_context *ctx);
vb2_error_t vb2_secdata_commit(struct vb2_context *ctx);
vb2_error_t vb2_secdatak_load(struct vb2_context *ctx);
vb2_error_t vb2_secdatak_commit(struct vb2_context *ctx);

/**
 * Utility function to turn the virtual dev-mode flag on or off. 0=off, 1=on.
 */
vb2_error_t vb2_set_developer_mode(struct vb2_context *ctx, int value);

/****************************************************************************/

/*
 * The following functions are internal apis, listed here for use by unit tests
 * only.
 */

/**
 * Issue a TPM_Clear and reenable/reactivate the TPM.
 */
uint32_t TPMClearAndReenable(void);

/**
 * Like TlclWrite(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */
uint32_t SafeWrite(uint32_t index, const void *data, uint32_t length);

#endif  /* VBOOT_REFERENCE_ROLLBACK_INDEX_H_ */
