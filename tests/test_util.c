/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some helper function for tests.
 */

#include "2api.h"
#include "2misc.h"
#include "test_util.h"

void set_boot_mode(struct vb2_context *ctx, enum vb2_boot_mode boot_mode,
		   uint32_t recovery_reason)
{
	switch (boot_mode) {
	case VB2_BOOT_MODE_MANUAL_RECOVERY:
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
		sd->recovery_reason = VB2_RECOVERY_RO_MANUAL;
		gbb->flags |= VB2_GBB_FLAG_FORCE_MANUAL_RECOVERY
		break;
	case VB2_BOOT_MODE_BROKEN_SCREEN:
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
		sd->recovery_reason = 123;
		break;
	case VB2_BOOT_MODE_DIAGNOSTICS:
		break;
	case VB2_BOOT_MODE_DEVELOPER:
		ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
		break;
	case VB2_BOOT_MODE_NORMAL:
		break;
	default:
		break;
	}
}
