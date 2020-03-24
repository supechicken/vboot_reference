/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware screen definitions.
 */

#include "2common.h"
#include "2ui.h"

#define MENU_ITEMS(a) \
	.size = ARRAY_SIZE(a), \
	.items = a

static struct vb2_menu_item empty_menu[] = { };

/******************************************************************************/
/* VB2_SCREEN_BLANK */


static struct vb2_screen_data blank_screen = {
	.screen = VB2_SCREEN_BLANK,
	.name = "Blank",
	MENU_ITEMS(empty_menu),
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_SELECT */

static struct vb2_menu_item recovery_select_items[] = {
	{
		.text = "Recovery using phone",
		.target = VB2_SCREEN_RECOVERY_PHONE_STEP1,
	},
	{
		.text = "Recovery using external disk",
		.target = VB2_SCREEN_RECOVERY_DISK_STEP1,
	},
};

static struct vb2_screen_data recovery_select_screen = {
	.screen = VB2_SCREEN_RECOVERY_SELECT,
	.name = "Recovery method selection",
	MENU_ITEMS(recovery_select_items),
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_PHONE_STEP1 */

static struct vb2_screen_data recovery_phone_step1_screen = {
	.screen = VB2_SCREEN_RECOVERY_PHONE_STEP1,
	.name = "Phone recovery step 1",
	MENU_ITEMS(empty_menu),
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_DISK_STEP1 */

static struct vb2_screen_data recovery_disk_step1_screen = {
	.screen = VB2_SCREEN_RECOVERY_DISK_STEP1,
	.name = "Disk recovery step 1",
	MENU_ITEMS(empty_menu),
};

/******************************************************************************/
/*
 * TODO(chromium:1035800): Refactor UI code across vboot and depthcharge.
 * Currently vboot and depthcharge maintain their own copies of menus/screens.
 * vboot detects keyboard input and controls the navigation among different menu
 * items and screens, while depthcharge performs the actual rendering of each
 * screen, based on the menu information passed from vboot.
 */
static struct vb2_screen_data *vboot_screens[] = {
	&blank_screen,
	&recovery_select_screen,
	&recovery_phone_step1_screen,
	&recovery_disk_step1_screen,
};

struct vb2_screen_data *vb2_get_screen(enum vb2_screen screen)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(vboot_screens); i++) {
		if (vboot_screens[i]->screen == screen)
			return vboot_screens[i];
	}
	return NULL;
}
