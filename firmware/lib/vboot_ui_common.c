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

/**
 * Run alternative firmware if allowed
 *
 * This will only return if it is not allowed, or the bootloader fails to
 * cannot be found / fails to start
 *
 * @ctx		Context
 * @altfw_num	Number of bootloader to start (0=any, 1=first, etc.)
 */
static void vb2_run_altfw(struct vb2_context *ctx,
			  enum VbAltFwIndex_t altfw_num)
{
	if (vb2_kernel_cleanup(ctx, VB2_SUCCESS)) {
		vb2_error_notify("Error locking kernel versions on legacy "
				 "boot.\n", NULL, VB_BEEP_FAILED);
	} else {
		VbExLegacy(altfw_num);	/* will not return if found */
		vb2_error_notify("Legacy boot failed. Missing BIOS?\n", NULL,
				 VB_BEEP_FAILED);
	}
}

void vb2_error_no_altfw(void)
{
	VB2_DEBUG("Legacy boot is disabled\n");
	VbExDisplayDebugInfo("WARNING: Booting legacy BIOS has not been "
			     "enabled. Refer to the developer-mode "
			     "documentation for details.\n", 0);
	vb2_error_beep(VB_BEEP_NOT_ALLOWED);
}

void vb2_try_alt_fw(struct vb2_context *ctx, int allowed,
		    enum VbAltFwIndex_t altfw_num)
{
	if (allowed)
		/* will not return if found */
		vb2_run_altfw(ctx, altfw_num);
	else
		vb2_error_no_altfw();
}
