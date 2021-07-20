/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Kernel selection, loading, verification, and booting.
 */

#ifndef VBOOT_REFERENCE_2KERNEL_H_
#define VBOOT_REFERENCE_2KERNEL_H_

#include "2common.h"

/**
 * Handle a normal boot.
 *
 * @param ctx		Vboot context.
 * @return VB2_SUCCESS, or error code on error.
 */
vb2_error_t vb2_normal_boot(struct vb2_context *ctx);

/**
 * Fill VB2_CONTEXT_DEV_BOOT_ALLOWED, VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED and
 * VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED flags in ctx->flags.
 *
 * @param ctx		Vboot context.
 */
void vb2_fill_dev_boot_flags(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_2KERNEL_H_ */
