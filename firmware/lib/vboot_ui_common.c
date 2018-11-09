/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "sysincludes.h"

#include "2sysincludes.h"
#include "2common.h"

#include "rollback_index.h"
#include "vboot_api.h"
#include "vboot_ui_common.h"
#include "vboot_ui_menu_private.h"

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


/* Print a message to log, another message to the screen, and beep once or
 * twice.
 *
 * Called when an attempted action was disallowed. NULL messages are ignored.
 */
void vb2_error_notify(struct vb2_context *ctx,
		      const char *print_msg,
		      const char *log_msg,
		      enum vb2_beep_type beep)
{
	if (ctx)
		vb2_flash_screen(ctx);
	if (print_msg)
		VbExDisplayDebugInfo(print_msg);
	if (!log_msg)
		log_msg = print_msg;
	if (log_msg)
		VB2_DEBUG(log_msg);
	vb2_error_beep(beep);
}

void vb2_run_altfw(int altfw_num)
{
	if (RollbackKernelLock(0))
		VB2_DEBUG("Error locking kernel versions on legacy boot.\n");
	else
		VbExLegacy(altfw_num);	/* will not return if found */
	vb2_error_beep(VB_BEEP_FAILED);
}
