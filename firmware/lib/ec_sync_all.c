/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC software sync routines for vboot
 */

#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2sysincludes.h"
#include "ec_sync.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

static int check_reboot_for_display(struct vb2_context *ctx)
{
	if (!(vb2_get_sd(ctx)->flags & VB2_SD_FLAG_DISPLAY_AVAILABLE)) {
		VB2_DEBUG("Reboot to initialize display\n");
		vb2_nv_set(ctx, VB2_NV_DISPLAY_REQUEST, 1);
		return 1;
	}
	return 0;
}

static void display_wait_screen(struct vb2_context *ctx, const char *fw_name)
{
	VB2_DEBUG("%s update is slow. Show WAIT screen.\n", fw_name);
	VbDisplayScreen(ctx, VB_SCREEN_WAIT, 0, NULL);
}

vb2_error_t ec_sync(struct vb2_context *ctx)
{
	vb2_error_t rv;

	/* Phase 1 checks if updates or reboots are necessary */
	rv = ec_sync_phase1(ctx);
	if (rv)
		return rv;

	/* Phase 2 performs the updates and/or jump to RW */
	rv = ec_sync_phase2(ctx);
	if (rv)
		return rv;

	/* Phase 3 finalizes and closes out EC vboot */
	rv = ec_sync_phase3(ctx);
	if (rv)
		return rv;

	return VB2_SUCCESS;
}

vb2_error_t auxfw_sync_all(struct vb2_context *ctx)
{
	VbAuxFwUpdateSeverity_t fw_update = VB_AUX_FW_NO_UPDATE;
	vb2_error_t rv;

#ifdef PD_SYNC
	const int do_pd_sync = !(gbb->flags &
				 VB2_GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC);
#else
	const int do_pd_sync = 0;
#endif

	if (do_pd_sync && check_ec_active(ctx, 1))
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;

#ifdef PD_SYNC
	/* Handle updates and jumps for PD */
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	if (!(gbb->flags & VB2_GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC)) {
		retval = sync_one_ec(ctx, 1);
		if (retval != VB2_SUCCESS)
			return retval;
	}
#endif

	/* Check if a wait screen is required */
	int need_wait_screen = ec_will_update_slowly(ctx);
	rv = auxfw_sync_check(ctx, &fw_update);
	if (rv)
		return rv;

	/* If AUX FW update is slow display the wait screen */
	if (need_wait_screen && fw_update == VB_AUX_FW_SLOW_UPDATE) {
		/* Display should be available, but better check again */
		if (check_reboot_for_display(ctx))
			return VBERROR_REBOOT_REQUIRED;

		display_wait_screen(ctx, "AUX FW");
	}

	if (fw_update > VB_AUX_FW_NO_UPDATE) {
		/* Do Aux FW software sync */
		rv = auxfw_sync(ctx);
		if (rv)
			return rv;
		/*
		 * AUX FW Update is applied successfully. Request EC reboot to
		 * RO, so that the chips that had FW update gets reset to a
		 * clean state.
		 */
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_ec_software_sync(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	/* The whole point is to support EC software sync */
	if (sd->flags & VBSD_EC_SOFTWARE_SYNC)
		ctx->flags |= VB2_CONTEXT_EC_SYNC_SUPPORTED;

	return ec_sync(ctx);
}
