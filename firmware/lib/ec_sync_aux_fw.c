/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * PROM software sync (via EC passthrough) routines for vboot
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"

#include "sysincludes.h"
#include "ec_sync.h"
#include "gbb_header.h"
#include "vboot_common.h"
#include "vboot_kernel.h"

int ec_sync_will_update_tunneled_slowly(struct vb2_context *ctx,
					VbCommonParams *cparams)
{
	VbError_t status;

	VbSharedDataHeader *shared =
		(VbSharedDataHeader *)cparams->shared_data_blob;

	/* If we're not updating the EC, skip tunneled syncs as well */
	if (!(shared->flags & VBSD_EC_SOFTWARE_SYNC))
		return 0;
	if (cparams->gbb->flags & GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC)
		return 0;

	uint32_t severity;
	status = VbExCheckAuxFw(&severity);
	if (status == VBERROR_SUCCESS && severity > VB_AUX_FW_FAST_UPDATE)
		return 1;

	return 0;
}

VbError_t ec_sync_phase_tunneled(struct vb2_context *ctx,
				 VbCommonParams *cparams)
{
	VbSharedDataHeader *shared =
		(VbSharedDataHeader *)cparams->shared_data_blob;
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	/* If we're not updating the EC, skip tunneled updates as well */
	if (!(shared->flags & VBSD_EC_SOFTWARE_SYNC))
		return VBERROR_SUCCESS;
	if (cparams->gbb->flags & GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC)
		return VBERROR_SUCCESS;
	if (sd->recovery_reason)
		return VBERROR_SUCCESS;

	return VbExUpdateAuxFw();
}
