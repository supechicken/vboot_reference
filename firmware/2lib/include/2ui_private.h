/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private declarations for 2ui.c. Defined here for easier testing.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_
#define VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_

enum power_button_state_t {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
};

extern enum power_button_state_t power_button_state;

extern int vb2_shutdown_requested(struct vb2_context *ctx, uint32_t key);

#endif
