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

/************************
 *    Utilities         *
 ************************/

/**
 * Check if the default is to boot using disk, usb, or legacy
 *
 * @param ctx		Vboot2 context
 * @return		VB2_DEV_DEFAULT_BOOT_DISK (default),
 *			VB2_DEV_DEFAULT_BOOT_USB,
 *			or VB2_DEV_DEFAULT_BOOT_LEGACY.
 */
static uint32_t get_default_boot(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	uint32_t default_boot = vb2_nv_get(ctx, VB2_NV_DEV_DEFAULT_BOOT);

	if (gbb->flags & VB2_GBB_FLAG_DEFAULT_DEV_BOOT_LEGACY)
		return VB2_DEV_DEFAULT_BOOT_LEGACY;
	else if (default_boot)
		return default_boot;

	return VB2_DEV_DEFAULT_BOOT_DISK;
}

/**
 * Check if dev boot is allowed
 *
 * Dev boot is disabled if and only if FWMP_DEV_DISABLE_BOOT and no
 * FORCE_DEV_SWITCH_ON
 *
 * @param ctx		Vboot2 context
 * @return		1 if allowed, or 0 otherwise.
 */
static uint32_t dev_boot_allowed(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	if (vb2_secdata_fwmp_get_flag(ctx, VB2_SECDATA_FWMP_DEV_DISABLE_BOOT))
		return !!(gbb->flags & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);
	return 1;
}

/************************
 *    Menu Actions      *
 ************************/

/* Boot from internal disk if allowed. */
static vb2_error_t boot_from_internal_action(struct vb2_context *ctx)
{
	if (!dev_boot_allowed(ctx)) {
		/* TODO(roccochen): flash screen then show error notify */
		return VBERROR_KEEP_LOOPING;
	}
	VB2_DEBUG("trying fixed disk\n");
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

/* Boot legacy BIOS if allowed and available. */
static vb2_error_t boot_legacy_action(struct vb2_context *ctx)
{
	/* TODO(roccochen) */
	return VBERROR_KEEP_LOOPING;
}

/* Boot from USB or SD card if allowed and available. */
static vb2_error_t boot_usb_action(struct vb2_context *ctx)
{
	/* TODO(roccochen) */
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t vb2_handle_menu_input(struct vb2_context *ctx,
					 uint32_t key, uint32_t key_flags)
{
	/* TODO(roccochen): handle keyboard input */

	VB2_DEBUG("pressed key 0x%x\n", key);

	return VBERROR_KEEP_LOOPING;
}

/* Initialize menu state. Must be called once before displaying any menus. */
static vb2_error_t vb2_init_menus(struct vb2_context *ctx)
{
	/* TODO(roccochen): init language menu */
	return VB2_SUCCESS;
}

/************************
 *    Entry Points      *
 ************************/

/* Developer mode menu entry point */
vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;
	int i, keep_looping;

	rv = vb2_init_menus(ctx);
	if (rv != VB2_SUCCESS)
		return rv;

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted. */
	do {
		keep_looping = 1;
		/* TODO(roccochen): Make sure user knows dev mode disabled */
		/* TODO(roccochen): Check if booting from usb */

		/*
		 * Scan keyboard inputs. Scan keybaord more frequently than
		 * media since x86 platforms do not like to scan USB too
		 * rapidly.
		 */
		for (i = 0; i < MEDIA_DELAY; i += KEY_DELAY) {
			uint32_t key = VbExKeyboardRead();

			/*
			 * TODO(roccochen): handle the following combo key sets
			 *
			 * Valid combo key sets:
			 * Ctrl+L = boot alternative bootloader
			 * 0...9 = allow selection of the default '0' bootloader
			 *
			 * Valid combo press (for DETACHABLE):
			 * VOL_DOWN_LONG_PRESS = boot from internal disk
			 */
			if (key == VB_KEY_CTRL('D'))
				rv = boot_from_internal_action(ctx);
			else
				rv = vb2_handle_menu_input(ctx, key, 0);

			/* Have loaded a kernel or decided to shut down now. */
			if (rv != VBERROR_KEEP_LOOPING) {
				keep_looping = 0;
				break;
			}

			/* Reset 30 second timer whenever we see a new key. */
			if (key != 0)
				vb2_audio_start(ctx);

			VbExSleepMs(KEY_DELAY);

			/* If dev mode was disabled, loop forever */
			keep_looping = !dev_boot_allowed(ctx) ||
				       vb2_audio_looping();
			if (!keep_looping)
				break;
		}
	} while (keep_looping);

	/* Timeout, boot from the default option. */
	if (rv == VBERROR_KEEP_LOOPING) {
		uint32_t default_boot = get_default_boot(ctx);

		/* boot legacy does not return on success */
		if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY)
			boot_legacy_action(ctx);

		if (default_boot == VB2_DEV_DEFAULT_BOOT_USB)
			if (VB2_SUCCESS == boot_usb_action(ctx))
				rv = VB2_SUCCESS;

		rv = boot_from_internal_action(ctx);
	}

	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return rv;

}

/* Recovery mode menu entry point for very broken non-manual recovery */
vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	rv = vb2_init_menus(ctx);
	if (rv != VB2_SUCCESS)
		return rv;

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		rv = vb2_handle_menu_input(ctx, key, 0);
		if (rv != VBERROR_KEEP_LOOPING)
			break;
	}

	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return rv;
}

/* Recovery mode menu entry point for manual recovery */
vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;
	int i;
	static int usb_good = -1;

	rv = vb2_init_menus(ctx);
	if (rv != VB2_SUCCESS)
		return rv;

	/* Loop and wait for a recovery image */
	VB2_DEBUG("waiting for a recovery image\n");
	do {
		rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (rv == VB2_SUCCESS)
			break; /* Found a recovery kernel */

		if (usb_good != (rv == VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_good = (rv == VB2_ERROR_LK_NO_DISK_FOUND);
		}

		/*
		 * Scan keyboard inputs. Scan keybaord more frequently than
		 * media since x86 platforms do not like to scan USB too
		 * rapidly.
		 */
		for(i = 0; i < MEDIA_DELAY; i += KEY_DELAY) {
			uint32_t key, key_flags;
			key = VbExKeyboardReadWithFlags(&key_flags);

			/*
			 * TODO(roccochen): handle the following combo key sets
			 *
			 * Valid combo key sets:
			 * Ctrl+D = enter to developer menu if keyboard is
			 * trusted
			 *
			 * Valid combo press (for DETACHABLE):
			 * VOL_UP_DOWN_COMBO_PRESS = enter to developer menu if
			 * keyboard is trusted
			 */
			rv = vb2_handle_menu_input(ctx, key, key_flags);
			if (rv != VBERROR_KEEP_LOOPING)
				break;
			VbExSleepMs(KEY_DELAY);
		}
	} while(rv != VBERROR_KEEP_LOOPING);

	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return rv;
}
