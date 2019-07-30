/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2common.h"
#include "2sysincludes.h"
#include "secdata_tpm.h"
#include "vboot_api.h"
#include "vboot_kernel.h"
#include "vboot_ui_common.h"

/* One or two beeps to notify that attempted action was disallowed. */
void vb2_error_beep(enum vb2_beep_type beep)
{
	switch (beep) {
	case VB_BEEP_FAILED:
		VbExBeep(250, 200);
		break;
	default:
	case VB_BEEP_NOT_ALLOWED:
		VbExBeep(120, 400);
		VbExSleepMs(120);
		VbExBeep(120, 400);
		break;
	}
}

void vb2_error_notify(const char *print_msg,
		      const char *log_msg,
		      enum vb2_beep_type beep)
{
	if (print_msg)
		VbExDisplayDebugInfo(print_msg, 0);
	if (!log_msg)
		log_msg = print_msg;
	if (log_msg)
		VB2_DEBUG(log_msg);
	vb2_error_beep(beep);
}

vb2_error_t vb2_run_altfw(struct vb2_context *ctx,
			  enum VbAltFwIndex_t altfw_num)
{
	vb2_error_t rv;
	
	rv = vb2_kernel_commit(ctx);
	if (rv) {
		/* vb2_kernel_commit runs vb2api_fail. */
		VB2_DEBUG("Failed to run vb2_kernel_commit\n");
		return rv;
	}

	/* Only disable TPM when starting diagnostic mode. */
	if (altfw_num == VB_ALTFW_DIAGNOSTIC) {
		rv = vb2ex_tpm_set_mode(VB2_TPM_MODE_DISABLED);
		if (rv) {
			VB2_DEBUG("Failed to disable TPM\n");
			vb2api_fail(ctx, VB2_RECOVERY_TPM_DISABLE_FAILED, rv);
			return rv;
		}
	}

	/* This call should not return in normal cases. */
	rv = VbExLegacy(altfw_num);
	if (rv) {
		VB2_DEBUG("Failed to run VbExLegacy\n");
		/* Assuming failure was due to bad hash, though the ROM could
		   just be missing or invalid. */
		vb2api_fail(ctx, VB2_RECOVERY_ALTFW_HASH_FAILED, 0);
		return rv;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2_error_no_altfw(void)
{
	VB2_DEBUG("Legacy boot is disabled\n");
	VbExDisplayDebugInfo("WARNING: Booting legacy BIOS has not been "
			     "enabled. Refer to the developer-mode "
			     "documentation for details.\n", 0);
	vb2_error_beep(VB_BEEP_NOT_ALLOWED);
	return VB2_SUCCESS;
}

vb2_error_t vb2_try_alt_fw(struct vb2_context *ctx, int allowed,
			   enum VbAltFwIndex_t altfw_num)
{
	if (allowed)
		/* This call should not return in normal cases. */
		return vb2_run_altfw(ctx, altfw_num);
	else
		return vb2_error_no_altfw();
}
