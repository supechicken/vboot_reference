/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2return_codes.h"
#include "2secdata.h"
#include "2ui.h"
#include "vboot_api.h"  /* for VbExBeep */
#include "vboot_kernel.h"

/* Timeouts (in usec) for timer. */
#define TIMER_TIMEOUT		(30ULL * VB_USEC_PER_SEC)
#define TIMER_TIMEOUT_SHORT	(2ULL * VB_USEC_PER_SEC)
#define TIMER_BEEP_1		(20ULL * VB_USEC_PER_SEC)
#define TIMER_BEEP_2		(TIMER_BEEP_1 + VB_USEC_PER_SEC / 2)

/* Delay type (in msec) of developer and recovery mode menu looping. */
#define KEY_DELAY		20	/* Check keyboard inputs */

/*****************************************************************************/
/* Menu actions */

static vb2_error_t vb2_handle_menu_input(struct vb2_context *ctx,
					 uint32_t key, uint32_t key_flags)
{
	/* TODO(roccochen): handle keyboard input */

	switch (key) {
	case 0:
		/* nothing pressed */
		break;
	case VB_KEY_ENTER:
		return VBERROR_SHUTDOWN_REQUESTED;
	default:
		VB2_DEBUG("pressed key %#x, trusted? %d\n", key,
			  !!(key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD));
	}

	return VBERROR_KEEP_LOOPING;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	uint64_t timer_timeout_usec = TIMER_TIMEOUT;  /* GBB flags indicated */
	uint64_t timer_open_time, timer_elapsed;
	int timer_beep_count = 0;

	/* TODO(roccochen): Init menus. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Get delay context. */
	/* TODO(roccochen): Audio */
	timer_open_time = VbExGetTimer();
	if (gbb->flags & VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY) {
		VB2_DEBUG("using short dev screen delay\n");
		timer_timeout_usec = TIMER_TIMEOUT_SHORT;
	}

	/* We'll loop until we finish the delay or are interrupted. */
	do {
		/* TODO(roccochen): Make sure user knows dev mode disabled */
		/* TODO(roccochen): Check if booting from usb */

		/* Scan keyboard inputs. */
		uint32_t key = VbExKeyboardRead();

		rv = VBERROR_KEEP_LOOPING;  /* set to default */
		switch (key) {
		case VB_BUTTON_VOL_DOWN_LONG_PRESS:
			if (!DETACHABLE)
				break;
			/* fallthrough */
		case VB_KEY_CTRL('D'):
			if (vb2_dev_boot_allowed(ctx))
				rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
			break;
		case VB_KEY_CTRL('L'):
			if (vb2_dev_boot_allowed(ctx) &&
			    vb2_dev_boot_legacy_allowed(ctx))
				rv = VbExLegacy(VB_ALTFW_DEFAULT);
			break;
		case '0'...'9':
			VB2_DEBUG("developer UI - "
				  "user pressed key '%c': Boot alternative "
				  "firmware\n", key);
			if (vb2_dev_boot_allowed(ctx) &&
			    vb2_dev_boot_legacy_allowed(ctx))
				rv = VbExLegacy(key - '0');
			break;
		default:
			rv = vb2_handle_menu_input(ctx, key, 0);
		}

		/* Have loaded a kernel or decided to shut down now. */
		if (rv != VBERROR_KEEP_LOOPING)
			return rv;

		/* Reset 30 second timer whenever we see a new key. */
		if (key != 0) {
			timer_open_time = VbExGetTimer();
			timer_timeout_usec = TIMER_TIMEOUT;
		}

		/* Check if timeout occurred */
		VbExSleepMs(KEY_DELAY);
		timer_elapsed = VbExGetTimer() - timer_open_time;
		if (timer_timeout_usec == TIMER_TIMEOUT) {
			if ((timer_elapsed >= TIMER_BEEP_1 &&
			     timer_beep_count == 0) ||
			    (timer_elapsed >= TIMER_BEEP_2 &&
			     timer_beep_count == 1)) {
				VbExBeep(250, 400);
				timer_beep_count++;
			}
		}

		/* If dev mode was disabled, loop forever */
	} while (!vb2_dev_boot_allowed(ctx) ||
		 timer_elapsed < timer_timeout_usec);

	/* Timeout, boot from the default option. */
	switch (vb2_get_dev_boot_target(ctx)) {
	case VB2_DEV_DEFAULT_BOOT_LEGACY:
		if (vb2_dev_boot_legacy_allowed(ctx) &&
		    VbExLegacy(VB_ALTFW_DEFAULT) == VB2_SUCCESS)
			return VB2_SUCCESS;
		break;
	case VB2_DEV_DEFAULT_BOOT_USB:
		if (vb2_dev_boot_usb_allowed(ctx) &&
		    VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE) == VB2_SUCCESS)
			return VB2_SUCCESS;
		break;
	default:
		break;
	}

	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	/* TODO(roccochen): Init menus. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		rv = vb2_handle_menu_input(ctx, key, 0);
		if (rv != VBERROR_KEEP_LOOPING)
			return rv;
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	/* TODO(roccochen): Init menus. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for a recovery image or keyboard inputs */
	VB2_DEBUG("waiting for a recovery image or keyboard inputs\n");
	while(1) {
		/* TODO(roccochen): try load usb and check if usb good */

		/* Scan keyboard inputs. */
		uint32_t key, key_flags;
		key = VbExKeyboardReadWithFlags(&key_flags);

		rv = VBERROR_KEEP_LOOPING;  /* set to default */
		switch (key) {
		case VB_BUTTON_VOL_DOWN_LONG_PRESS:
			if (!DETACHABLE)
				break;
			/* fallthrough */
		case VB_KEY_CTRL('D'):
			if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD &&
			    !(ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) &&
			    vb2_allow_recovery(ctx)) {
				VB2_DEBUG("Enabling dev-mode...\n");
				if (vb2_enable_developer_mode(ctx) !=
				    VB2_SUCCESS) {
					rv = VBERROR_TPM_SET_BOOT_MODE_STATE;
					break;
				}

				/* This was meant for headless devices,
				 * shouldn't really matter here. */
				if (USB_BOOT_ON_DEV)
					vb2_nv_set(ctx, VB2_NV_DEV_BOOT_USB, 1);

				VB2_DEBUG("Reboot so it will take effect\n");
				rv =  VBERROR_REBOOT_REQUIRED;
			}
			break;
		default:
			rv = vb2_handle_menu_input(ctx, key, key_flags);
		}

		if (rv != VBERROR_KEEP_LOOPING)
			return rv;

		VbExSleepMs(KEY_DELAY);
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}
