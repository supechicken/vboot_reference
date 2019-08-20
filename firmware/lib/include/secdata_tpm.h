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
#define FWMP_NV_INDEX 0x100a

/* All functions return TPM_SUCCESS (zero) if successful, non-zero if error */
uint32_t secdata_firmware_read(struct vb2_context *ctx);
uint32_t secdata_firmware_write(struct vb2_context *ctx);
uint32_t secdata_kernel_read(struct vb2_context *ctx);
uint32_t secdata_kernel_write(struct vb2_context *ctx);
uint32_t secdata_kernel_lock(struct vb2_context *ctx);
uint32_t secdata_fwmp_read(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_SECDATA_TPM_H_ */
