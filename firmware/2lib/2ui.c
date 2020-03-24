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
#include "vboot_kernel.h"

/* Delay type (in msec) of developer and recovery mode menu looping. */
#define KEY_DELAY_MS		20	/* Check keyboard inputs */

/* Global variables */
static enum {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
} power_button_state;

static int usb_nogood;

/*****************************************************************************/
/* Utilities */

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * Returns true if a shutdown is required and false if no shutdown is required.
 */
static int vb2_want_shutdown(struct vb2_context *ctx, uint32_t key)
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

/*****************************************************************************/
/* Menu actions */

/* Action that enables developer mode and reboots. */
static vb2_error_t to_dev_action(struct vb2_context *ctx)
{
	if ((ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !vb2_allow_recovery(ctx))
		return VBERROR_KEEP_LOOPING;

	VB2_DEBUG("Enabling dev-mode...\n");
	if (vb2_enable_developer_mode(ctx) != VB2_SUCCESS)
		return VBERROR_TPM_SET_BOOT_MODE_STATE;

	/* This was meant for headless devices, shouldn't really matter here. */
	if (USB_BOOT_ON_DEV)
		vb2_nv_set(ctx, VB2_NV_DEV_BOOT_USB, 1);

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

	if (vb2_want_shutdown(ctx, key)) {
		VB2_DEBUG("shutdown requested!\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	return VBERROR_KEEP_LOOPING;
}

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
	enum vb2_dev_default_boot default_boot;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* If dev mode was disabled, loop forever. */
	if (!vb2_dev_boot_allowed(ctx))
		while (1);

	/* Boot from the default option. */
	default_boot = vb2_get_dev_boot_target(ctx);

	/* Boot legacy does not return on success */
	if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY &&
	    vb2_dev_boot_legacy_allowed(ctx) &&
	    VbExLegacy(VB_ALTFW_DEFAULT) == VB2_SUCCESS)
		return VB2_SUCCESS;

	if (default_boot == VB2_DEV_DEFAULT_BOOT_USB &&
	    vb2_dev_boot_usb_allowed(ctx) &&
	    VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE) == VB2_SUCCESS)
		return VB2_SUCCESS;

	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	/* TODO: enter_recovery_base_screen(ctx); */
	VB2_TRY(vb2_init_menus(ctx));
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
	uint32_t key;
	uint32_t key_flags;
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	usb_nogood = -1;
	while (1) {
		rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (rv == VB2_SUCCESS)
			return rv;  /* Found a recovery kernel */

		if (usb_nogood != (rv != VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_nogood = rv != VB2_ERROR_LK_NO_DISK_FOUND;
			/* TODO:enter_recovery_base_screen(ctx); */
		}

		key = VbExKeyboardReadWithFlags(&key_flags);
		if (key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS) {
			if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD)
				/* TODO: enter_to_dev_menu(ctx); */
				to_dev_action(ctx);
			else
				VB2_DEBUG("ERROR: untrusted combo?!\n");
		} else {
			rv = vb2_handle_menu_input(ctx, key, key_flags);
			if (rv != VBERROR_KEEP_LOOPING)
				return rv;
		}
		VbExSleepMs(KEY_DELAY_MS);
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}
