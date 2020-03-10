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
#include "vboot_audio.h"
#include "vboot_kernel.h"

/* Delay type (in ms) of developer and recovery mode menu looping */
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
		VB2_DEBUG("pressed key 0x%x, trusted? %d\n", key,
			  !!(key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD));
	}

	return VBERROR_KEEP_LOOPING;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	/* TODO(roccochen): Init menus. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted. */
	do {
		/* TODO(roccochen): Make sure user knows dev mode disabled */
		/* TODO(roccochen): Check if booting from usb */

		/* Scan keyboard inputs. */
		uint32_t key = VbExKeyboardRead();

		rv = VBERROR_KEEP_LOOPING;  /* set to default */
		if (key == VB_KEY_CTRL('D') ||
		    (key == VB_BUTTON_VOL_DOWN_LONG_PRESS &&
		     DETACHABLE)) {
			if (vb2_dev_boot_allowed(ctx))
				rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
		} else if (key == VB_KEY_CTRL('L')) {
			if (vb2_dev_boot_allowed(ctx))
				rv = vb2_dev_try_legacy(ctx, VB_ALTFW_DEFAULT);
		} else if ('0' <= key && key <= '9') {
			VB2_DEBUG("developer UI - "
				  "user pressed key '%c': Boot alternative "
				  "firmware\n", key);
			rv = vb2_dev_try_legacy(ctx, key - '0');
		} else {
			rv = vb2_handle_menu_input(ctx, key, 0);
		}

		/* Have loaded a kernel or decided to shut down now. */
		if (rv != VBERROR_KEEP_LOOPING)
			break;

		/* Reset 30 second timer whenever we see a new key. */
		if (key != 0)
			vb2_audio_start(ctx);

		VbExSleepMs(KEY_DELAY);

		/* If dev mode was disabled, loop forever */
	} while (!vb2_dev_boot_allowed(ctx) || vb2_audio_looping());

	/* Timeout, boot from the default option. */
	if (rv == VBERROR_KEEP_LOOPING) {

		enum vb2_dev_default_boot default_boot =
			vb2_get_dev_boot_target(ctx);

		/* Boot legacy does not return on success. */
		if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY &&
		    vb2_dev_try_legacy(ctx, VB_ALTFW_DEFAULT) == VB2_SUCCESS)
			return VB2_SUCCESS;

		if (default_boot == VB2_DEV_DEFAULT_BOOT_USB &&
		    vb2_dev_boot_usb_allowed(ctx) &&
		    VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE) == VB2_SUCCESS)
			return VB2_SUCCESS;

		return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
	}

	return rv;
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
			break;
	}

	return rv;
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

		/*
		 * TODO(roccochen): handle the following combo key sets
		 *
		 * Valid combo key sets:
		 * Ctrl+D = enter to developer menu if keyboard is
		 * trusted
		 *
		 * Valid combo press (for DETACHABLE):
		 * VOL_UP_DOWN_COMBO_PRESS = enter to developer menu if
		 * keyboard is trusted
		 */
		rv = vb2_handle_menu_input(ctx, key, key_flags);
		if (rv != VBERROR_KEEP_LOOPING)
			break;

		VbExSleepMs(KEY_DELAY);
	}

	return rv;
}
