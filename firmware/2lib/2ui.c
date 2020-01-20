/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2return_codes.h"
#include "2secdata.h"
#include "2ui.h"
#include "vboot_api.h"
#include "vboot_audio.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/* Delay type (in ms) of developer and recovery mode menu looping */
#define KEY_DELAY		20	/* Check keyboard inputs */

/*****************************************************************************/
/* Utilities */

/**
 * Check the default boot option.
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
 * Determine if developer mode is allowed.
 *
 * @param ctx		Vboot2 context
 * @return		1 if allowed, or 0 otherwise.
 */
static uint32_t dev_boot_allowed(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	/* Disable if FWMP_DEV_DISABLE_BOOT and no FORCE_DEV_SWITCH_ON */
	if (vb2_secdata_fwmp_get_flag(ctx, VB2_SECDATA_FWMP_DEV_DISABLE_BOOT))
		return !!(gbb->flags & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);

	return 1;
}

/*****************************************************************************/
/* Menu actions */

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

	switch (key) {
	case 0:
		/* nothing pressed */
		break;
	case VB_KEY_ENTER:
		return VBERROR_SHUTDOWN_REQUESTED;
	default:
		VB2_DEBUG("pressed key 0x%x\n", key);
	}

	return VBERROR_KEEP_LOOPING;
}

/* Initialize menu state. Must be called once before displaying any menus. */
static vb2_error_t vb2_init_menus(struct vb2_context *ctx)
{
	/* TODO(roccochen): init language menu */
	return VB2_SUCCESS;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted. */
	do {
		/* TODO(roccochen): Make sure user knows dev mode disabled */
		/* TODO(roccochen): Check if booting from usb */

		/* Scan keyboard inputs. */
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
		if (rv != VBERROR_KEEP_LOOPING)
			break;

		/* Reset 30 second timer whenever we see a new key. */
		if (key != 0)
			vb2_audio_start(ctx);

		VbExSleepMs(KEY_DELAY);

		/* If dev mode was disabled, loop forever */
	} while (!dev_boot_allowed(ctx) || vb2_audio_looping());

	/* Timeout, Boot from the default option. */
	if (rv == VBERROR_KEEP_LOOPING) {
		uint32_t default_boot = get_default_boot(ctx);

		/* Boot legacy does not return on success */
		if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY)
			boot_legacy_action(ctx);

		if (default_boot == VB2_DEV_DEFAULT_BOOT_USB &&
		    boot_usb_action(ctx) == VB2_SUCCESS)
			rv = VB2_SUCCESS;
		else
			rv = boot_from_internal_action(ctx);
	}

	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);
	return rv;
}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		rv = vb2_handle_menu_input(ctx, key, 0);
		if (rv != VBERROR_KEEP_LOOPING)
			break;
	}

	return rv;
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for a recovery image or keyboard inputs */
	VB2_DEBUG("waiting for a recovery image or keyboard inputs\n");
	while(1) {
		/* TODO(roccochen): try load usb and check if usb good */

		/* Scan keyboard inputs. */
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

	return rv;
}
