/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#ifndef VBOOT_REFERENCE_2UI_H_
#define VBOOT_REFERENCE_2UI_H_

#include <2api.h>
#include <2sysincludes.h>

/*****************************************************************************/
/* Data structures */

struct vb2_menu_item {
	/* Text description of the menu item */
	const char *text;

	/* Target screen */
	enum vb2_screen target;

	/* Function to run before displaying the target screen */
	vb2_error_t (*action)(struct vb2_context *ctx);
};

struct vb2_screen_data {
	/* Corresponding VB2_SCREEN_* value */
	enum vb2_screen screen;

	/* Menu name for printing to console only */
	const char *name;

	/* Number of menu items */
	uint16_t size;

	/* List of menu items */
	struct vb2_menu_item *items;
};

/*
 * Get data struct of a screen.
 *
 * @param screen	Screen type from enum vb2_screen
 *
 * @return screen data struct on success, NULL on error.
 */
struct vb2_screen_data *vb2_get_screen(enum vb2_screen screen);

/*****************************************************************************/
/* UI loops */

/**
 * UI for a developer-mode boot.
 *
 * Enter the developer menu, which provides options to switch out of developer
 * mode, boot from external media, use legacy bootloader, or boot Chrome OS from
 * disk.
 *
 * If a timeout occurs, take the default boot action.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_developer_menu(struct vb2_context *ctx);

/**
 * UI for a non-manual recovery ("BROKEN").
 *
 * Enter the recovery menu, which shows that an unrecoverable error was
 * encountered last boot. Wait for the user to physically reset or shut down.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx);

/**
 * UI for a manual recovery-mode boot.
 *
 * Enter the recovery menu, which prompts the user to insert recovery media,
 * navigate the step-by-step recovery, or enter developer mode if allowed.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_2UI_H_ */
