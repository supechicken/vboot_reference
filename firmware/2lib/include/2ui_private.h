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


#define ACTION_ARGS struct vb2_context *ctx, \
	const struct vb2_screen_info *screen_info, \
	struct vb2_ui_state *state, \
	enum vb2_screen *new_screen

struct input_action {
	int key;
	void (*action)(ACTION_ARGS);
};
void menu_up_action(ACTION_ARGS);
void menu_down_action(ACTION_ARGS);
void menu_select_action(ACTION_ARGS);
void menu_back_action(ACTION_ARGS);
void (*action_lookup(int key))(ACTION_ARGS);
void validate_selection(const struct vb2_screen_info *screen_info,
			struct vb2_ui_state *state);
void display_ui(const struct vb2_screen_info *screen_info,
		struct vb2_ui_state *state);
vb2_error_t ui_loop(struct vb2_context *ctx, enum vb2_screen root_screen);

#endif  /* VBOOT_REFERENCE_2UI_PRIVATE_H_ */
