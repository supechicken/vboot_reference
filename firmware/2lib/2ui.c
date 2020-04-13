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
#include "2ui_private.h"
#include "vboot_api.h"
#include "vboot_audio.h"
#include "vboot_kernel.h"

/* Delay type (in msec) of developer and recovery mode menu looping. */
#define KEY_DELAY_MS		20	/* Check keyboard inputs */

/* Global variables */
enum power_button_state_t power_button_state;

static enum vb2_screen current_screen;
static int current_menu_idx;
static struct vb2_menu menus[];

/*****************************************************************************/
/* Utilities functions */

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * Returns true if a shutdown is requested and false otherwise.
 */
static int vb2_shutdown_requested(struct vb2_context *ctx, uint32_t key)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	uint32_t shutdown_request = VbExIsShutdownRequested();

	/*
	 * Ignore power button push until after we have seen it released.
	 * This avoids shutting down immediately if the power button is still
	 * being held on startup. After we've recognized a valid power button
	 * push then don't report the event until after the button is released.
	 */
	if (shutdown_request & VB_SHUTDOWN_REQUEST_POWER_BUTTON) {
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		if (power_button_state == POWER_BUTTON_RELEASED)
			power_button_state = POWER_BUTTON_PRESSED;
	} else {
		if (power_button_state == POWER_BUTTON_PRESSED)
			shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		power_button_state = POWER_BUTTON_RELEASED;
	}

	if (key == VB_BUTTON_POWER_SHORT_PRESS)
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	/* If desired, ignore shutdown request due to lid closure. */
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_LID_CLOSED;

	/*
	 * In detachables, disabling shutdown due to power button.
	 * We are using it for selection instead.
	 */
	if (DETACHABLE)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	return !!shutdown_request;
}

/* (Re-)Draw the screen identified by current_screen. */
static vb2_error_t vb2_screen_draw_current(struct vb2_context *ctx)
{
	return vb2ex_display_ui(current_screen, 0, 0, 0);
}

static void vb2_log_screen_change(void)
{
	/* TODO(roccochen): convert screen index to name */
	VB2_DEBUG("=============== %d Screen ===============\n",
		  current_screen);
}

/**
 * Switch to a new screen (but don't draw it yet).
 *
 * @param ctx:			Vboot2 context
 * @param new_current_screen:	new screen to set current_menu to
 * @param new_current_menu_idx: new idx to set current_menu_idx to
 */
static void vb2_screen_change(struct vb2_context *ctx,
			      enum vb2_screen new_current_screen,
			      int new_current_menu_idx)
{
	/* TODO(roccochen): maintain menu stack */
	/* TODO(roccochen): implement disabled_idx_mask */

	current_screen = new_current_screen;
	current_menu_idx = new_current_menu_idx;

	vb2_log_screen_change();
}

/*****************************************************************************/
/* Menu actions */

static vb2_error_t enter_broken_base_screen(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_broken_base_screen\n");
	vb2_screen_change(ctx, VB2_SCREEN_OS_BROKEN, 0);
	vb2_screen_draw_current(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_recovery_base_screen(struct vb2_context *ctx)
{
	/* Sanity check, should never happen. */
	if (!vb2_allow_recovery(ctx))
		return enter_broken_base_screen(ctx);

	VB2_DEBUG("enter_recovery_base_screen\n");
	/* TODO(roccochen): recovery select screen */
	vb2_screen_change(ctx, VB2_SCREEN_RECOVERY_SELECT, 0);
	vb2_screen_draw_current(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_usb_nogood_screen(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_usb_nogood_screen\n");
	vb2_screen_change(ctx, VB2_SCREEN_RECOVERY_NO_GOOD, 0);
	vb2_screen_draw_current(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_dev_menu(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_to_dev_menu\n");
	if (vb2_get_sd(ctx)->flags & VB2_SD_FLAG_DEV_MODE_ENABLED) {
		/* TODO(roccochen): flash screen */
		/* TODO(roccochen): notify dev mode already on */
		return VBERROR_KEEP_LOOPING;
	}
	vb2_screen_change(ctx, VB2_SCREEN_RECOVERY_TO_DEV, 0);
	vb2_screen_draw_current(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Action that enables developer mode and reboots. */
static vb2_error_t to_dev_action(struct vb2_context *ctx)
{
	/* Sanity check, should never happen. */
	if ((vb2_get_sd(ctx)->flags & VB2_SD_FLAG_DEV_MODE_ENABLED) ||
	    !vb2_allow_recovery(ctx))
		return VBERROR_KEEP_LOOPING;

	vb2_enable_developer_mode(ctx);

	VB2_DEBUG("Reboot so it will take effect\n");
	return VBERROR_REBOOT_REQUIRED;
}

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

	if (vb2_shutdown_requested(ctx, key)) {
		VB2_DEBUG("shutdown requested!\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	return VBERROR_KEEP_LOOPING;
}

/* Master table of all menus. Menus with size == 0 count as menuless screens. */
static struct vb2_menu menus[VB2_MENU_COUNT] = {
	[VB2_MENU_BLANK] = {
		.name = "Blank",
		.size = 0,
		.screen = VB2_SCREEN_BLANK,
		.items = NULL,
	},
	/* TODO(roccochen): recovery select menu */
};

/* Initialize menu state. Must be called once before displaying any menus. */
static vb2_error_t vb2_init_menus(struct vb2_context *ctx)
{
	/* TODO(roccochen): Initialize language menu */

	power_button_state = POWER_BUTTON_HELD_SINCE_BOOT;

	return VB2_SUCCESS;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0, 0, 0);

	/* Get audio/delay context. */
	vb2_audio_start(ctx);

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
		if (key != 0)
			vb2_audio_start(ctx);

		/* Check if timeout occurred */
		VbExSleepMs(KEY_DELAY_MS);

		/* If dev mode was disabled, loop forever */
	} while (!vb2_dev_boot_allowed(ctx) || vb2_audio_looping());

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
	uint32_t key;
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));

	enter_broken_base_screen(ctx);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		key = VbExKeyboardRead();
		rv = vb2_handle_menu_input(ctx, key, 0);
		if (rv != VBERROR_KEEP_LOOPING)
			return rv;
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	int usb_nogood = -1;
	uint32_t key;
	uint32_t key_flags;
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	while (1) {
		rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (rv == VB2_SUCCESS)
			return rv;  /* Found a recovery kernel */

		if (usb_nogood != (rv != VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* TODO: USB state changed, force back to base screen */
			usb_nogood = rv != VB2_ERROR_LK_NO_DISK_FOUND;
			if (usb_nogood)
				enter_usb_nogood_screen(ctx);
			else
				enter_recovery_base_screen(ctx);
		}

		key = VbExKeyboardReadWithFlags(&key_flags);
		rv = VBERROR_KEEP_LOOPING;

		if (key == VB_KEY_CTRL('D') ||
		    (key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS &&
		     DETACHABLE)) {
			if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD) {
				enter_to_dev_menu(ctx);
				/* TODO(roccochen): need user confirmation */
				rv = to_dev_action(ctx);
			}
			else
				VB2_DEBUG("ERROR: untrusted combo?!\n");
		} else {
			rv = vb2_handle_menu_input(ctx, key, key_flags);
		}

		if (rv != VBERROR_KEEP_LOOPING)
			return rv;

		VbExSleepMs(KEY_DELAY_MS);
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}
