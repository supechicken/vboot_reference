/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2common.h"
#include "vboot_api.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/* Developer mode entry point. */
vb2_error_t VbBootDeveloperMenu(struct vb2_context *ctx)
{
	VbDisplayMenu(ctx, VB_SCREEN_BLANK, 0, 0, 0);
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

/* Recovery mode entry point. */
vb2_error_t VbBootRecoveryMenu(struct vb2_context *ctx)
{
	VbDisplayMenu(ctx, VB_SCREEN_BLANK, 0, 0, 0);
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}
