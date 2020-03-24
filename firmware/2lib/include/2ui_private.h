/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private declarations for 2ui.c. Defined here for easier testing.
 */

#include "2api.h"

#ifndef VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_
#define VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_

enum power_button_state_type {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
};

extern enum power_button_state_type power_button_state;

extern const struct vb2_screen_info *current_screen;
extern uint32_t selected_item, disabled_item_mask;

extern const struct vb2_screen_info *vboot_screens[4];

extern int shutdown_required(struct vb2_context *ctx, uint32_t key);
extern void change_screen(struct vb2_context *ctx, enum vb2_screen new_screen);
extern void update_selection(int direction);

#endif
