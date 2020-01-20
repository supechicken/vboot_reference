/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "vboot_api.h"
#include "vboot_audio.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/* Delay types (in ms) of developer and recovery mode menu looping */
#define KEY_DELAY		20	/* Check keyboard inputs */
#define MEDIA_DELAY		1000	/* Check external media */

/* Global variables */
static const char dev_disable_msg[] =
	"Developer mode is disabled on this device by system policy.\n"
	"For more information, see http://dev.chromium.org/chromium-os/fwmp\n"
	"\n";

static int usb_nogood;
static uint32_t default_boot;
static uint32_t disable_dev_boot;
static uint32_t altfw_allowed;

/************************
 *    Utilities         *
 ************************/

// TODO: move these utils to vboot_ui_menu_private.h

enum vb2_beep_type {
	VB_BEEP_FAILED,		/* Permitted but the operation failed */
	VB_BEEP_NOT_ALLOWED,	/* Operation disabled by user setting */
};

static void vb2_error_beep(enum vb2_beep_type beep)
{
	switch (beep) {
	case VB_BEEP_FAILED:
		VbExBeep(250, 200);
		break;
	default:
	case VB_BEEP_NOT_ALLOWED:
		VbExBeep(120, 400);
		VbExSleepMs(120);
		VbExBeep(120, 400);
		break;
	}
}

static void vb2_error_notify(const char *print_msg,
		      const char *log_msg,
		      enum vb2_beep_type beep)
{
	if (print_msg)
		VbExDisplayDebugInfo(print_msg, 0);
	if (!log_msg)
		log_msg = print_msg;
	if (log_msg)
		VB2_DEBUG(log_msg);
	vb2_error_beep(beep);
}

static void vb2_error_no_altfw(void)
{
	VB2_DEBUG("Legacy boot is disabled\n");
	VbExDisplayDebugInfo("WARNING: Booting legacy BIOS has not been "
			     "enabled. Refer to the developer-mode "
			     "documentation for details.\n", 0);
	vb2_error_beep(VB_BEEP_NOT_ALLOWED);
}

static void vb2_try_altfw(struct vb2_context *ctx, int allowed,
		   enum VbAltFwIndex_t altfw_num)
{
	if (!allowed) {
		vb2_error_no_altfw();
		return;
	}

	if (vb2ex_commit_data(ctx)) {
		vb2_error_notify("Error committing data on legacy boot.\n",
				 NULL, VB_BEEP_FAILED);
		return;
	}

	/* Will not return if successful */
	VbExLegacy(altfw_num);

	vb2_error_notify("Legacy boot failed. Missing BIOS?\n", NULL,
			 VB_BEEP_FAILED);
}

/************************
 *    Menu Actions      *
 ************************/

/* Boot from internal disk if allowed. */
static vb2_error_t boot_from_internal_action(struct vb2_context *ctx)
{
	// TODO
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

/* Boot legacy BIOS if allowed and available. */
static vb2_error_t boot_legacy_action(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}

/* Boot from USB or SD card if allowed and available. */
static vb2_error_t boot_usb_action(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_dev_warning_menu(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
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

static vb2_error_t enter_to_norm_menu(struct vb2_context *ctx)
{
	// TODO
	return VBERROR_KEEP_LOOPING;
}


/* Boot alternative bootloader if allowed and available. */
static vb2_error_t enter_altfw_menu(struct vb2_context *ctx)
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

/************************
 *    Main Functions    *
 ************************/

/**
 * Main function that handles developer warning menu functionality
 *
 * This function performs a loop and wait for a external media or keyboard
 * input. It checks removable media every MEDIA_DELAY ms and scans keyboard
 * every KEY_DELAY ms. It scans keyboard more frequently than media since x86
 * platforms do not like to scan USB too rapidly.
 *
 * Valid combo key sets:
 * Ctrl+D = boot from internal disk
 * Ctrl+L = boot alternative bootloader
 * 0...9 = allow selection of the default '0' bootloader
 *
 * Valid combo press (for DETACHABLE):
 * VOL_DOWN_LONG_PRESS = boot from internal disk
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static vb2_error_t developer_ui(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	vb2_error_t rv;
	int i, keep_looping;

	/* Check if the default is to boot using disk, usb, or legacy */
	default_boot = vb2_nv_get(ctx, VB2_NV_DEV_DEFAULT_BOOT);
	if (gbb->flags & VB2_GBB_FLAG_DEFAULT_DEV_BOOT_LEGACY)
		default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;

	/* Check if developer mode is disabled by FWMP */
	disable_dev_boot = 0;
	if (vb2_secdata_fwmp_get_flag(ctx, VB2_SECDATA_FWMP_DEV_DISABLE_BOOT)) {
		if (gbb->flags & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON) {
			VB2_DEBUG("FWMP_DEV_DISABLE_BOOT rejected by"
				  "FORCE_DEV_SWITCH_ON\n");
		} else {
			/* If dev mode is disabled, only allow TONORM */
			disable_dev_boot = 1;
			VB2_DEBUG("dev_disable_boot is set.\n");
		}
	}

	altfw_allowed = vb2_nv_get(ctx, VB2_NV_DEV_BOOT_LEGACY) ||
	    (gbb->flags & VB2_GBB_FLAG_FORCE_DEV_BOOT_LEGACY) ||
	    vb2_secdata_fwmp_get_flag(ctx, VB2_SECDATA_FWMP_DEV_ENABLE_LEGACY);

	/* Show appropriate initial menu */
	if (disable_dev_boot)
		enter_to_norm_menu(ctx);
	else
		enter_dev_warning_menu(ctx);

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted */
	do {
		keep_looping = 1;
		/* Make sure user knows dev mode disabled */
		if (disable_dev_boot)
			VbExDisplayDebugInfo(dev_disable_msg, 0);

		// TODO
		/*if (peek() == VB_SCREEN_BOOT_FROM_EXTERNAL) {
			VB2_DEBUG("attempting to boot from USB\n");
			if (boot_usb_allowed(ctx)) {
				if (VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE)
				    == VB2_SUCCESS) {
					VB2_DEBUG("booting from USB\n");
					return VB2_SUCCESS;
				}
			}
		}*/

		/* Scan keyboard inputs. */
		for (i = 0; i < MEDIA_DELAY; i += KEY_DELAY) {
			uint32_t key = VbExKeyboardRead();

			if (key == VB_KEY_CTRL('D') ||
			    key == VB_BUTTON_VOL_DOWN_LONG_PRESS) {
				rv = boot_from_internal_action(ctx);
			} else if (key == VB_KEY_CTRL('L')) {
				rv = enter_altfw_menu(ctx);
			} else if ('0' <= key && key <= '9') {
				VB2_DEBUG("developer UI - "
					  "user pressed key '%c': Boot alternative "
					  "firmware\n", key);
				vb2_try_altfw(ctx, altfw_allowed, key - '0');
				rv = VBERROR_KEEP_LOOPING;
			} else {
				rv = vb2_handle_menu_input(ctx, key, 0);
			}

			/*
			 * We may have loaded a kernel or decided to shut down
			 * now.
			 */
			if (rv != VBERROR_KEEP_LOOPING)
				return rv;

			/* Reset 30 second timer whenever we see a new key. */
			if (key != 0)
				vb2_audio_start(ctx);

			VbExSleepMs(KEY_DELAY);

			keep_looping = disable_dev_boot || vb2_audio_looping();
			if (!keep_looping)
				break;
		}

		/* If dev mode was disabled, loop forever (never timeout) */
	} while (keep_looping);

	if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY)
		boot_legacy_action(ctx);	/* Doesn't return on success. */

	if (default_boot == VB2_DEV_DEFAULT_BOOT_USB)
		if (VB2_SUCCESS == boot_usb_action(ctx))
			return VB2_SUCCESS;

	return boot_from_internal_action(ctx);
}

/* Main function that handles non-manual recovery (BROKEN) menu functionality */
static vb2_error_t broken_ui(struct vb2_context *ctx)
{
	enter_recovery_base_screen(ctx);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		vb2_error_t rv = vb2_handle_menu_input(ctx, key, 0);
		if (rv != VBERROR_KEEP_LOOPING)
			return rv;
	}
}

/**
 * Main function that handles recovery menu functionality
 *
 * This function performs a loop and wait for a recovery image or keyboard
 * input. It checks removable media every MEDIA_DELAY ms and scans keyboard
 * every KEY_DELAY ms. It scans keyboard more frequently than media since x86
 * platforms do not like to scan USB too rapidly.
 *
 * Valid combo key sets:
 * Ctrl+D = enter to developer menu if keyboard is trusted
 *
 * Valid combo press (for DETACHABLE):
 * VOL_UP_DOWN_COMBO_PRESS = enter to developer menu if keyboard is trusted
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static vb2_error_t recovery_ui(struct vb2_context *ctx)
{
	vb2_error_t rv;
	int i;

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	usb_nogood = -1;
	while (1) {
		rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (rv == VB2_SUCCESS)
			return rv; /* Found a recovery kernel */

		if (usb_nogood != (rv != VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_nogood = rv != VB2_ERROR_LK_NO_DISK_FOUND;
			enter_recovery_base_screen(ctx);
		}

		/* Scan keyboard inputs */
		for(i = 0; i < MEDIA_DELAY; i += KEY_DELAY) {
			uint32_t key, key_flags;
			key = VbExKeyboardReadWithFlags(&key_flags);
			if (key == VB_KEY_CTRL('D') ||
			    key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS) {
				if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD)
					enter_to_dev_menu(ctx);
				else
					VB2_DEBUG("ERROR: untrusted combo?!\n");
			} else {
				rv = vb2_handle_menu_input(ctx, key,
							    key_flags);
				if (rv != VBERROR_KEEP_LOOPING)
					return rv;
			}
			VbExSleepMs(KEY_DELAY);
		}
	}
}

/************************
 *    Entry Points      *
 ************************/

/* Developer mode entry point. */
vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv = vb2_init_menus(ctx);
	if (rv != VB2_SUCCESS)
		return rv;
	rv = developer_ui(ctx);
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return rv;
}

/* Recovery mode entry point. */
vb2_error_t vb2_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv = vb2_init_menus(ctx);
	if (rv != VB2_SUCCESS)
		return rv;
	if (vb2_allow_recovery(ctx))
		rv = recovery_ui(ctx);
	else
		rv = broken_ui(ctx);
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return rv;
}
