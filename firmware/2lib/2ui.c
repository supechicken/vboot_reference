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
#include "vboot_kernel.h"

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

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;
	uint32_t default_boot;

	/* TODO(roccochen): Init, wait for user, and boot. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* If dev mode was disabled, loop forever. */
	if (!dev_boot_allowed(ctx))
		while (1);

	/* Boot from the default option. */
	default_boot = get_default_boot(ctx);

	/* Boot legacy does not return on success */
	if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY)
		boot_legacy_action(ctx);
	if (default_boot == VB2_DEV_DEFAULT_BOOT_USB &&
	    boot_usb_action(ctx) == VB2_SUCCESS)
		rv = VB2_SUCCESS;
	else
		rv = boot_from_internal_action(ctx);

	return rv;

}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	/* TODO(roccochen): Init and wait for user to reset or shutdown. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	while (1);

	return VB2_SUCCESS;
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	/* TODO(roccochen): Init and wait for user. */
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	while (1);

	return VB2_SUCCESS;
}
