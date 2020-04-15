/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private declarations for 2ui.c. Defined here for easier testing.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_
#define VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_

#include "2api.h"

enum power_button_state_t {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
};

extern enum power_button_state_t power_button_state;

/* Context of each select item */
struct vb2_menu_item {
	const char *text;
	vb2_error_t (*action)(struct vb2_context *ctx);
};

/* Context of each menu item in menu master table. */
struct vb2_menu {
	/* Menu name for vb2_log_menu_change only */
	const char *name;
	/* The size of itmes, should be *_COUNT defined in 2api.h. */
	uint16_t size;
	/* The corresponding mapping with VB2_SCREEN. */
	enum vb2_screen screen;
	/* Indicate the select item to certain action mapping. */
	struct vb2_menu_item *items;
};

/*
 * Quick index to menu master table.
 *
 * This enumeration is the index to vb2_menu master table.
 * It has a one-to-one relationship with VB2_SCREEN, where VB2_MENU_{*} should
 * maps to VB2_SCREEN_{*} and VB2_MENU_COUNT is always the enumeration size.
 */
typedef enum _VB2_MENU {
	VB2_MENU_BLANK,
	VB2_MENU_FIRMWARE_SYNC,
	VB2_MENU_COUNT,
} VB2_MENU;

#endif
