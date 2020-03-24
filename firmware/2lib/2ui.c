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

enum power_button_state power_button;
int invalid_disk_last = -1;

/*****************************************************************************/
/* Utility functions */

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * @param ctx		Context pointer
 * @param key		Pressed key (VB_BUTTON_POWER_SHORT_PRESS)
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
		if (power_button == POWER_BUTTON_RELEASED)
			power_button = POWER_BUTTON_PRESSED;
	} else {
		if (power_button == POWER_BUTTON_PRESSED)
			shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		power_button = POWER_BUTTON_RELEASED;
	}

	if (key == VB_BUTTON_POWER_SHORT_PRESS)
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	/* If desired, ignore shutdown request due to lid closure. */
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_LID_CLOSED;

	/*
	 * In detachables, disable shutdown due to power button.
	 * It is used for menu selection instead.
	 */
	if (DETACHABLE)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	return !!shutdown_request;
}

/*****************************************************************************/
/* Menu navigation actions */

/**
 * Update selected_item, taking into account disabled indices (from
 * disabled_item_mask).  The selection does not wrap, meaning that we block
 * on the 0 or max index when we hit the top or bottom of the menu.
 */
vb2_error_t menu_up_action(ACTION_ARGS)
{
	int idx = state->selected_item - 1;
	while (idx >= 0 &&
	       ((1 << idx) & state->disabled_item_mask))
		idx--;
	/* Only update if idx is valid */
	if (idx >= 0)
		state->selected_item = idx;
	return VBERROR_KEEP_LOOPING;
}

vb2_error_t menu_down_action(ACTION_ARGS)
{
	int idx = state->selected_item + 1;
	while (idx < screen_info->num_items &&
	       ((1 << idx) & state->disabled_item_mask))
		idx++;
	/* Only update if idx is valid */
	if (idx < screen_info->num_items)
		state->selected_item = idx;
	return VBERROR_KEEP_LOOPING;
}

/**
 * Navigate to the target screen of the current menu item selection.
 */
vb2_error_t menu_select_action(ACTION_ARGS)
{
	const struct vb2_menu_item *menu_item;

	if (screen_info->num_items == 0)
		return VBERROR_KEEP_LOOPING;

	menu_item = &screen_info->items[state->selected_item];

	VB2_DEBUG("Select <%s> menu item <%s>\n",
		  screen_info->name, menu_item->text);

	if (menu_item->target) {
		VB2_DEBUG("Changing to target screen %#x for menu item <%s>\n",
			  menu_item->target, menu_item->text);
		*new_screen = menu_item->target;
	} else {
		VB2_DEBUG("No target set for menu item <%s>\n",
			  menu_item->text);
	}

	return VBERROR_KEEP_LOOPING;
}

/**
 * Return back to the previous screen.
 */
vb2_error_t menu_back_action(ACTION_ARGS)
{
	*new_screen = VB2_SCREEN_BACK;
	return VBERROR_KEEP_LOOPING;
}

/*****************************************************************************/
/* Action lookup tables */

static struct input_action action_table[] = {
	{ VB_KEY_UP, menu_up_action },
	{ VB_KEY_DOWN, menu_down_action },
	{ VB_KEY_ENTER, menu_select_action },
	{ VB_KEY_ESC, menu_back_action },
};

static int preprocess_key(int key)
{
	if (DETACHABLE) {
		if (key == VB_BUTTON_VOL_UP_SHORT_PRESS)
			return VB_KEY_UP;
		else if (key == VB_BUTTON_VOL_DOWN_SHORT_PRESS)
			return VB_KEY_DOWN;
		else if (key == VB_BUTTON_POWER_SHORT_PRESS)
			return VB_KEY_ENTER;
	}
	return key;
}

vb2_error_t (*action_lookup(int key))(ACTION_ARGS)
{
	int i;
	key = preprocess_key(key);
	for (i = 0; i < ARRAY_SIZE(action_table); i++)
		if (action_table[i].key == key)
			return action_table[i].action;
	return NULL;
}

/*****************************************************************************/
/* UI loop */

void validate_selection(const struct vb2_screen_info *screen_info,
			struct vb2_screen_state *state)
{
	if ((state->selected_item == 0 && screen_info->num_items == 0) ||
	    (state->selected_item < screen_info->num_items &&
	     !((1 << state->selected_item) & state->disabled_item_mask)))
		return;

	/* Selection invalid; select the first available non-disabled item. */
	state->selected_item = 0;
	while (((1 << state->selected_item) & state->disabled_item_mask) &&
	       state->selected_item < screen_info->num_items)
		state->selected_item++;

	/* No non-disabled items available; just choose 0. */
	if (state->selected_item >= screen_info->num_items)
		state->selected_item = 0;
}

void display_ui(const struct vb2_screen_info *screen_info,
		struct vb2_screen_state *state)
{
	VB2_DEBUG("<%s> menu item <%s>\n",
		  screen_info->name, screen_info->num_items ?
		  screen_info->items[state->selected_item].text : NULL);

	/* TODO: Stop hard-coding the locale. */
	vb2ex_display_ui(state->screen, 0, state->selected_item,
			 state->disabled_item_mask);
}

vb2_error_t ui_loop(struct vb2_context *ctx, enum vb2_screen root_screen,
		    vb2_error_t (*global_action)(ACTION_ARGS))
{
	struct vb2_screen_state prev_state;
	struct vb2_screen_state state;
	enum vb2_screen new_screen = root_screen;
	const struct vb2_screen_info *screen_info
		= vb2_get_screen_info(root_screen);
	const struct vb2_screen_info *new_screen_info;
	uint32_t key;
	uint32_t key_flags;
	vb2_error_t (*action)(ACTION_ARGS);
	vb2_error_t rv;

	if (new_screen == VB2_SCREEN_BACK)
		VB2_DIE("Can't start from special BACK screen.\n");
	if (screen_info == NULL)
		VB2_DIE("Root screen not found.\n");

	while (1) {
		/* Transition to new screen. */
		if (new_screen == VB2_SCREEN_BACK)
			new_screen = root_screen;
		if (new_screen) {
			new_screen_info = vb2_get_screen_info(new_screen);
			if (new_screen_info == NULL) {
				VB2_DEBUG("Error: Screen entry %#x not found; "
					  "ignoring\n", new_screen);
			} else {
				memset(&state, 0, sizeof(state));
				state.screen = new_screen;
				screen_info = new_screen_info;
				validate_selection(screen_info, &state);
			}
		}
		new_screen = VB2_SCREEN_BLANK;

		/* Draw if there are state changes. */
		if (memcmp(&prev_state, &state, sizeof(state))) {
			memcpy(&prev_state, &state, sizeof(state));
			display_ui(screen_info, &state);
		}

		/* Check for shutdown request. */
		key = VbExKeyboardReadWithFlags(&key_flags);
		if (shutdown_required(ctx, key)) {
			VB2_DEBUG("Shutdown required!\n");
			return VBERROR_SHUTDOWN_REQUESTED;
		}

		/* Run action function if found. */
		action = action_lookup(key);
		if (action) {
			rv = action(ctx, screen_info, &state, &new_screen);
			if (rv != VBERROR_KEEP_LOOPING)
				return rv;
			validate_selection(screen_info, &state);
		} else if (key) {
			VB2_DEBUG("Pressed key %#x, trusted? %d\n", key,
				  !!(key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD));
		}

		/* Run global action function if available. */
		if (global_action) {
			rv = global_action(ctx, screen_info, &state,
					   &new_screen);
			validate_selection(screen_info, &state);
			if (rv != VBERROR_KEEP_LOOPING)
				return rv;
		}

		/* Delay. */
		VbExSleepMs(KEY_DELAY_MS);
	}

	return VB2_SUCCESS;
}

/*****************************************************************************/
/* Developer mode */

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

/*****************************************************************************/
/* Broken recovery */

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	return ui_loop(ctx, VB2_SCREEN_RECOVERY_BROKEN, NULL);
}

/*****************************************************************************/
/* Manual recovery */

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	return ui_loop(ctx, VB2_SCREEN_RECOVERY_SELECT, try_recovery_action);
}

vb2_error_t try_recovery_action(ACTION_ARGS)
{
	int invalid_disk;
	vb2_error_t rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

	if (rv == VB2_SUCCESS)
		return rv;

	/* If disk validity state changed, switch to appropriate screen. */
	invalid_disk = rv != VB2_ERROR_LK_NO_DISK_FOUND;
	if (invalid_disk_last != invalid_disk) {
		invalid_disk_last = invalid_disk;
		if (invalid_disk)
			*new_screen = VB2_SCREEN_RECOVERY_INVALID;
		else
			*new_screen = VB2_SCREEN_RECOVERY_SELECT;
	}

	return VBERROR_KEEP_LOOPING;
}
