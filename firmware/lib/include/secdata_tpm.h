/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking secure data spaces
 * stored in the TPM NVRAM.
 */

#ifndef VBOOT_REFERENCE_SECDATA_TPM_H_
#define VBOOT_REFERENCE_SECDATA_TPM_H_

#include "2api.h"

/* TPM NVRAM location indices. */
#define FIRMWARE_NV_INDEX 0x1007
#define KERNEL_NV_INDEX 0x1008
/* BACKUP_NV_INDEX (size 16) used to live at 0x1009; now deprecated */
#define FWMP_NV_INDEX 0x100a
#define REC_HASH_NV_INDEX 0x100b
#define REC_HASH_NV_SIZE VB2_SHA256_DIGEST_SIZE
/* Space to hold a temporary SHA256 digest of a public key for USB autoconfig;
 * see crbug.com/845589. */
#define OOBE_USB_AUTOCONFIG_KEY_DIGEST_NV_INDEX 0x100c
#define OOBE_USB_AUTOCONFIG_KEY_DIGEST_NV_SIZE VB2_SHA256_DIGEST_SIZE

/* All functions return TPM_SUCCESS (zero) if successful, non-zero if error */
uint32_t secdata_firmware_read(struct vb2_context *ctx);
uint32_t secdata_firmware_write(struct vb2_context *ctx);
uint32_t secdata_kernel_read(struct vb2_context *ctx);
uint32_t secdata_kernel_write(struct vb2_context *ctx);
uint32_t secdata_kernel_lock(struct vb2_context *ctx);
uint32_t secdata_fwmp_read(struct vb2_context *ctx);

/****************************************************************************/

/*
 * The following are internal APIs, listed here for use by unit tests only.
 */

extern int _secdata_kernel_locked;

/**
 * Issue a TPM_Clear and reenable/reactivate the TPM.
 */
uint32_t tpm_clear_and_reenable(void);

/**
 * Like TlclWrite(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */
uint32_t tlcl_safe_write(uint32_t index, const void *data, uint32_t length);

#endif  /* VBOOT_REFERENCE_SECDATA_TPM_H_ */
