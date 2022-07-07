/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some helper function for tests.
 */

#include "2api.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "test_common.h"
#include "test_util.h"

void set_boot_mode(struct vb2_context *ctx, enum vb2_boot_mode expect_boot_mode,
		   uint32_t recovery_reason)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	enum vb2_boot_mode *boot_mode = (enum vb2_boot_mode *)&ctx->boot_mode;

	switch (expect_boot_mode) {
	case VB2_BOOT_MODE_MANUAL_RECOVERY:
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
		sd->recovery_reason = recovery_reason;
		gbb->flags |= VB2_GBB_FLAG_FORCE_MANUAL_RECOVERY;
		break;
	case VB2_BOOT_MODE_BROKEN_SCREEN:
		ctx->flags |= VB2_CONTEXT_RECOVERY_MODE;
		sd->recovery_reason = recovery_reason;
		break;
	case VB2_BOOT_MODE_DIAGNOSTICS:
		vb2_nv_set(ctx, VB2_NV_DIAG_REQUEST, 1);
		break;
	case VB2_BOOT_MODE_DEVELOPER:
		ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
		break;
	case VB2_BOOT_MODE_NORMAL:
		break;
	default:
		*boot_mode = VB2_BOOT_MODE_UNDEFINED;
		TEST_EQ(ctx->boot_mode, expect_boot_mode, "Set boot mode");
		return;
	}
	vb2_set_boot_mode(ctx);
	TEST_EQ(ctx->boot_mode, expect_boot_mode, "Set boot mode");
}
