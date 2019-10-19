/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Auxiliary firmware (auxfw) software sync routines for vboot
 */

#include "2auxfw_sync.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2sysincludes.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/**
 * If no display is available, set DISPLAY_REQUEST in NV space
 */
static int check_reboot_for_display(struct vb2_context *ctx)
{
	if (!(vb2_get_sd(ctx)->flags & VB2_SD_FLAG_DISPLAY_AVAILABLE)) {
		VB2_DEBUG("Reboot to initialize display\n");
		vb2_nv_set(ctx, VB2_NV_DISPLAY_REQUEST, 1);
		return 1;
	}

	return 0;
}

/**
 * Display the WAIT screen
 */
static void display_wait_screen(struct vb2_context *ctx, const char *fw_name)
{
	VB2_DEBUG("%s update is slow. Show WAIT screen.\n", fw_name);
	VbDisplayScreen(ctx, VB_SCREEN_WAIT, 0, NULL);
}

/**
 * Set the RECOVERY_REQUEST flag in NV space
 */
static void request_recovery(struct vb2_context *ctx, uint32_t recovery_request)
{
	VB2_DEBUG("request_recovery(%u)\n", recovery_request);
	vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST, recovery_request);
}

/**
 * Wrapper around vb2ex_auxfw_protect() which sets recovery reason on error.
 */
static vb2_error_t protect_auxfw(struct vb2_context *ctx)
{
	vb2_error_t rv = vb2ex_auxfw_protect();

	if (rv == VBERROR_AUXFW_REBOOT_TO_RO_REQUIRED) {
		VB2_DEBUG("vb2ex_auxfw_protect() needs reboot\n");
	} else if (rv != VB2_SUCCESS) {
		VB2_DEBUG("vb2ex_auxfw_protect() returned %d\n", rv);
		request_recovery(ctx, VB2_RECOVERY_AUXFW_PROTECT);
	}

	return rv;
}

/**
 * Update the specified Aux FW and verify the update succeeded
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
static vb2_error_t update_auxfw(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_DEBUG("Updating Auxfw\n");

	/*
	 * The underlying platform is expected to know how and where to find the
	 * firmware image for all auxfw devices.
	 */
	rv = vb2ex_auxfw_update();
	if (rv != VB2_SUCCESS) {
		VB2_DEBUG("vb2ex_auxfw_update() returned %d\n", rv);

		/*
		 * The device may need a reboot.  It may need to unprotect the
		 * region before updating, or may need to reboot after updating.
		 * Either way, it's not an error requiring recovery mode.
		 *
		 * If we fail for any other reason, trigger recovery mode.
		 */
		if (rv != VBERROR_AUXFW_REBOOT_TO_RO_REQUIRED)
			request_recovery(ctx, VB2_RECOVERY_AUX_FW_UPDATE);

		return rv;
	}

	return rv;
}

vb2_error_t auxfw_sync_phase1(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	/* Reasons not to do sync at all */
	if (!(ctx->flags & VB2_CONTEXT_EC_SYNC_SUPPORTED))
		return VB2_SUCCESS;
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC)
		return VB2_SUCCESS;

#ifdef PD_SYNC
	const int do_pd_sync = !(gbb->flags &
				 VB2_GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC);

/* TODO; what to do about PD sync?  it requires a hash just like EC */


	/* Check if PD-RW needs to be updated */
	if (do_pd_sync && check_ec_hash(ctx, 1, VB_SELECT_FIRMWARE_EC_ACTIVE))
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
#endif

	return VB2_SUCCESS;
}

vb2_error_t auxfw_sync_phase2(struct vb2_context *ctx)
{
	vb2_error_t rv;

#ifdef PD_SYNC
	/* Handle updates and jumps for PD */
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	if (!(gbb->flags & VB2_GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC)) {
		retval = sync_one_ec(ctx, 1);
		if (retval != VB2_SUCCESS)
			return retval;
	}
#endif

	/* Attempt the update */
	rv = update_auxfw(ctx);
	if (rv != VB2_SUCCESS)
		return rv;

	/* Protect the firwmware from being overwritten */
	rv = protect_auxfw(ctx);
	if (rv != VB2_SUCCESS)
		return rv;

	return VB2_SUCCESS;
}

/**
 * Call the Auxfw Done callback
 */
vb2_error_t auxfw_sync_phase3(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);

	/* Auxfw verification (and possibly updating) is done */
	vb2_error_t rv = vb2ex_auxfw_vboot_done(!!sd->recovery_reason);
	if (rv)
		return rv;

	return VB2_SUCCESS;
}

/**
 * determine if we can update the auxfw device
 *
 * @param ctx		Vboot2 context
 * @return boolean (true iff we can update the axufw)
 */

static int auxfw_sync_allowed(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	/* Reasons not to do sync at all */
	if (!(ctx->flags & VB2_CONTEXT_EC_SYNC_SUPPORTED))
		return 0;
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_EC_SOFTWARE_SYNC)
		return 0;
	if (sd->recovery_reason)
		return 0;
	return 1;
}

vb2_error_t auxfw_sync_check(struct vb2_context *ctx,
			     VbAuxFwUpdateSeverity_t *severity)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	/* If we're not updating the EC, skip aux fw syncs as well */
	if (!auxfw_sync_allowed(ctx) ||
	    (gbb->flags & VB2_GBB_FLAG_DISABLE_PD_SOFTWARE_SYNC)) {
		*severity = VB_AUX_FW_NO_UPDATE;
		return VB2_SUCCESS;
	}

	return vb2ex_auxfw_check(severity);
}

vb2_error_t auxfw_sync(struct vb2_context *ctx)
{
	VbAuxFwUpdateSeverity_t fw_update = VB_AUX_FW_NO_UPDATE;
	vb2_error_t rv;

	/* Refactor all of this ridiculous checking for
required updates!
	*/

	/* Check for update severity */
	rv = auxfw_sync_check(ctx, &fw_update);
	if (rv)
		return rv;

	/* Phase 1; this determines if we need an update */
	rv = auxfw_sync_phase1(ctx);
	if (rv)
		return rv;

	/* If AUX FW update is slow display the wait screen */
	if (fw_update == VB_AUX_FW_SLOW_UPDATE) {
		/* Display should be available, but better check again */
		if (check_reboot_for_display(ctx))
			return VBERROR_REBOOT_REQUIRED;
		display_wait_screen(ctx, "AUX FW");
	}

#ifdef PD_SYNC
	/* Check for PD sync */
	rv = auxfw_sync_phase2(ctx, 1);
	if (rv)
		return rv;
#endif

	if (fw_update > VB_AUX_FW_NO_UPDATE) {
		/* Do Aux FW software sync */
		rv = auxfw_sync_phase2(ctx);
		if (rv)
			return rv;
		/*
		 * AUX FW Update is applied successfully. Request EC reboot to
		 * RO, so that the chips that had FW update gets reset to a
		 * clean state.
		 */
		return VBERROR_EC_REBOOT_TO_RO_REQUIRED;
	}

	/* Phase 3; Completes sync */
	rv = auxfw_sync_phase3(ctx);
	if (rv)
		return rv;

	return VB2_SUCCESS;
}
