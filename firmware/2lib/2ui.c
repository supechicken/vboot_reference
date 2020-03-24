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
#include "vboot_kernel.h"

#define KEY_DELAY_MS 20  /* Delay between key scans in UI loops */

/*****************************************************************************/
/* Global variables */

enum power_button_state_type power_button_state;

static enum vb2_screen root_screen;
static enum vb2_screen current_screen;
static uint32_t selected_item, disabled_item_mask;

/*****************************************************************************/
/* Utility functions */

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * @param ctx		Context pointer
 * @param key		Pressed key (VB_BUTTON_POWER_SHORT_PRESS)
 *
 * @return true if a shutdown is required, or false otherwise.
 */
int shutdown_required(struct vb2_context *ctx, uint32_t key)
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
/* UI functions */

/* (Re)draw the screen identified by current_screen. */
static void display_current_screen(void)
{
	struct vb2_screen_data *current_data = vb2_get_screen(current_screen);

	VB2_DEBUG("<%s> menu item <%s>\n",
		  current_data->name, current_data->size ?
		  current_data->items[selected_item].text : NULL);

	/* TODO: Stop hard-coding the locale. */
	vb2ex_display_ui(current_screen, 0, selected_item, disabled_item_mask);
}

/**
 * Switch to a new screen and display it.
 *
 * @param new_screen	New screen to set current_screen to
 * @param display	Display the screen after changing internally
 */
static void change_screen(enum vb2_screen new_screen, int display)
{
	current_screen = new_screen;
	selected_item = 0;

	/* Select the first available non-disabled item. */
	while (((1 << selected_item) & disabled_item_mask) &&
	       selected_item < vb2_get_screen(current_screen)->size)
		selected_item++;

	if (display)
		display_current_screen();
}

/**
 * Update the menu item selection.
 *
 * Updates selected_item, taking into account disabled indices (from
 * disabled_item_mask).  The selection will not wrap, meaning that we block on
 * the 0 or max index when we hit the top or bottom of the menu.
 *
 * @param direction	0 for up, 1 for down
 */
static void update_selection(int direction) {
	int idx;

	if (direction) {  /* Down */
		struct vb2_screen_data *current_data =
			vb2_get_screen(current_screen);
		idx = selected_item + 1;
		while (idx < current_data->size &&
		       ((1 << idx) & disabled_item_mask))
			idx++;
		/* Only update if idx is valid */
		if (idx < current_data->size)
			selected_item = idx;
	} else {  /* Up */
		idx = selected_item - 1;
		while (idx >= 0 &&
		       ((1 << idx) & disabled_item_mask))
			idx--;
		/* Only update if idx is valid */
		if (idx >= 0)
			selected_item = idx;
	}

	display_current_screen();
}

static vb2_error_t select_menu_item(struct vb2_context *ctx)
{
	struct vb2_screen_data *current_data = vb2_get_screen(current_screen);
	struct vb2_menu_item *current_item =
		&current_data->items[selected_item];
	vb2_error_t rv = VBERROR_KEEP_LOOPING;

	VB2_DEBUG("Select <%s> menu item <%s>\n",
		  current_data->name, current_item->text);

	if (current_item->target != VB2_SCREEN_BLANK) {
		VB2_DEBUG("Changing to target screen for <%s>\n",
			  current_item->text);
		change_screen(current_item->target, 0);
	}

	if (current_item->action) {
		VB2_DEBUG("Running action for <%s>\n", current_item->text);
		rv = current_item->action(ctx);
	}

	display_current_screen();

	return rv;
}

static vb2_error_t handle_menu_input(struct vb2_context *ctx,
				     uint32_t key, uint32_t key_flags)
{
	switch (key) {
	case 0:
		/* nothing pressed */
		break;
	case VB_KEY_ESC:
		change_screen(root_screen, 1);
		break;
	case VB_KEY_UP:
	case VB_BUTTON_VOL_UP_SHORT_PRESS:
		update_selection(0);
		break;
	case VB_KEY_DOWN:
	case VB_BUTTON_VOL_DOWN_SHORT_PRESS:
		update_selection(1);
		break;
	case VB_BUTTON_POWER_SHORT_PRESS:
	case VB_KEY_ENTER:
		return select_menu_item(ctx);
		break;
	default:
		VB2_DEBUG("pressed key %#x, trusted? %d\n", key,
			  !!(key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD));
		break;
	}

	if (shutdown_required(ctx, key)) {
		VB2_DEBUG("shutdown required!\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	return VBERROR_KEEP_LOOPING;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	enum vb2_dev_default_boot default_boot;

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
	/* TODO(roccochen): Init and wait for user to reset or shutdown. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0, 0, 0);

	while (1);

	return VB2_SUCCESS;
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	uint32_t key;
	uint32_t key_flags;
	vb2_error_t rv;

	root_screen = VB2_SCREEN_RECOVERY_SELECT;
	change_screen(root_screen, 1);

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	while (1) {
		key = VbExKeyboardReadWithFlags(&key_flags);
		rv = handle_menu_input(ctx, key, key_flags);
		if (rv != VBERROR_KEEP_LOOPING)
			return rv;

		VbExSleepMs(KEY_DELAY_MS);
	}

	return VBERROR_SHUTDOWN_REQUESTED;  /* Should never happen. */
}
