/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2common.h"
#include "2misc.h"
#include "vboot_api.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/* Delay types (in ms) of developer and recovery mode menu looping */
#define KEY_DELAY		20	/* Check keyboard inputs */
#define DISK_DELAY		1000	/* Check disks */
#define MEDIA_INIT_DELAY	500	/* Check removable media */

/* Global variables */
static int usb_nogood;

/************************
 *    Menu Actions      *
 ************************/

/* Boot from internal disk if allowed. */
static vb2_error_t boot_from_internal_action(struct vb2_context *ctx)
{
	// TODO
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

static vb2_error_t enter_recovery_base_screen(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_dev_menu(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t vb2_handle_menu_input(struct vb2_context *ctx,
					 uint32_t key, uint32_t key_flags)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}

/* Initialize menu state. Must be called once before displaying any menus. */
static vb2_error_t vb2_init_menus(struct vb2_context *ctx)
{
	// TODO
	return VB2_SUCCESS;
}

/**
 * Main function that handles developer warning menu functionality
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static vb2_error_t developer_ui(struct vb2_context *ctx)
{
	// TODO
	return boot_from_internal_action(ctx);
}

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t retval = vb2_init_menus(ctx);
	if (VB2_SUCCESS != retval)
		return retval;
	retval = developer_ui(ctx);
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return retval;
}

/* Main function that handles non-manual recovery (BROKEN) menu functionality */
static vb2_error_t broken_ui(struct vb2_context *ctx)
{
	enter_recovery_base_screen(ctx);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		vb2_error_t ret = vb2_handle_menu_input(ctx, key, 0);
		if (ret != VBERROR_KEEP_LOOPING)
			return ret;
	}
}

/**
 * Main function that handles recovery menu functionality
 *
 * This function performs a loop and wait for a ecovery image or keyboard input.
 * It checks removable media every MEDIA_INIT_DELAY ms and scans keyboard every
 * KEY_DELAY ms. It scans keyboard more frequently than media since x86
 * platforms do not like to scan USB too rapidly.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static vb2_error_t recovery_ui(struct vb2_context *ctx)
{
	uint32_t key;
	uint32_t key_flags;
	vb2_error_t ret;
	int i;

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	usb_nogood = -1;
	while (1) {
		ret = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (VB2_SUCCESS == ret)
			return ret; /* Found a recovery kernel */

		if (usb_nogood != (ret != VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_nogood = ret != VB2_ERROR_LK_NO_DISK_FOUND;
			enter_recovery_base_screen(ctx);
		}

		/* Scan keyboard inputs */
		for(i = 0; i < DISK_DELAY; i += KEY_DELAY) {
			key = VbExKeyboardReadWithFlags(&key_flags);
			if (key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS) {
				if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD)
					enter_to_dev_menu(ctx);
				else
					VB2_DEBUG("ERROR: untrusted combo?!\n");
			} else {
				ret = vb2_handle_menu_input(ctx, key,
							    key_flags);
				if (ret != VBERROR_KEEP_LOOPING)
					return ret;
			}
			VbExSleepMs(KEY_DELAY);
		}
	}
}

/* Recovery mode entry point. */
vb2_error_t vb2_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t retval = vb2_init_menus(ctx);
	if (VB2_SUCCESS != retval)
		return retval;
	if (vb2_allow_recovery(ctx))
		retval = recovery_ui(ctx);
	else
		retval = broken_ui(ctx);
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return retval;
}
