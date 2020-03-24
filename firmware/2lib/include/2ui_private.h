/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private declarations for 2ui.c. Defined here for testing purposes.
 */

#include "2api.h"

#ifndef VBOOT_REFERENCE_2UI_PRIVATE_H_
#define VBOOT_REFERENCE_2UI_PRIVATE_H_

enum power_button_state {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
};
extern enum power_button_state power_button;
int shutdown_required(struct vb2_context *ctx, uint32_t key);

extern int invalid_disk_last;


#define ACTION_ARGS struct vb2_context *ctx, \
	struct vb2_screen_state *state, \
	enum vb2_screen *new_screen

struct input_action {
	int key;
	vb2_error_t (*action)(ACTION_ARGS);
};

vb2_error_t menu_up_action(ACTION_ARGS);
vb2_error_t menu_down_action(ACTION_ARGS);
vb2_error_t menu_select_action(ACTION_ARGS);
vb2_error_t menu_back_action(ACTION_ARGS);
vb2_error_t (*action_lookup(int key))(ACTION_ARGS);
void validate_selection(struct vb2_screen_state *state);
void display_ui(struct vb2_screen_state *state);
vb2_error_t ui_loop(struct vb2_context *ctx, enum vb2_screen root_screen,
		    vb2_error_t (*global_action)(ACTION_ARGS));
vb2_error_t try_recovery_action(ACTION_ARGS);

#endif  /* VBOOT_REFERENCE_2UI_PRIVATE_H_ */
