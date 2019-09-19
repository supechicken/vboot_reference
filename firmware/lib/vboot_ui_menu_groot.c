/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2rsa.h"
#include "ec_sync.h"
#include "load_kernel_fw.h"
#include "rollback_index.h"
#include "utility.h"
#include "vb2_common.h"
#include "vboot_api.h"
#include "vboot_audio.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_kernel.h"
#include "vboot_ui_common.h"
#include "vboot_ui_groot_private.h"

static const char dev_disable_msg[] =
	"Developer mode is disabled on this device by system policy.\n"
	"For more information, see http://dev.chromium.org/chromium-os/fwmp\n"
	"\n";

static VB_GROOT current_menu, prev_menu;
static int current_menu_idx, disabled_idx_mask, usb_nogood, force_redraw;
static uint32_t default_boot;
static uint32_t disable_dev_boot;
static uint32_t altfw_allowed;
static struct vb2_menu menus[];
static const char no_legacy[] = "Legacy boot failed. Missing BIOS?\n";

// implementing a small screen history stack
// current screen should always be on the top of the stack
// previous screen(s) under it, maintaining the order
static const int MAXSIZE = 4;
static int stack[4];
static int top = -1;

static int isempty(void) {
	return (top == -1);
}

static int isfull(void) {
	return (top == MAXSIZE);
}

static int peek(void) {
	VB2_DEBUG("***** peek(0x%x), top = %d\n", stack[top], top);
	return stack[top];
}

static VB_GROOT pop(void) {
	VB_GROOT screen;

	if (!isempty()) {
		screen = stack[top];
		top = top - 1;
		VB2_DEBUG("***** pop(0x%x), top = %d\n", screen, top);
		return screen;
	}
	else
		return -1;
}

static int push(VB_GROOT screen) {
  VB2_DEBUG("***** push(0x%x), top = %d\n", screen, top);
	if (!isfull()) {
  		top = top + 1;
		stack[top] = screen;
		return 0;
	}
	else
  		return -1;
}

//// end of stack implementation

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * Returns true if a shutdown is required and false if no shutdown is required.
 */
static int VbWantShutdownGroot(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	uint32_t shutdown_request = VbExIsShutdownRequested();

	/* If desired, ignore shutdown request due to lid closure. */
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_LID_CLOSED;

	/*
	 * In detachables, disabling shutdown due to power button.
	 * We are using it for selection instead.
	 */
	shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	return !!shutdown_request;
}

/* (Re-)Draw the menu identified by current_menu[_idx] to the screen. */
static vb2_error_t vb2_draw_current_screen(struct vb2_context *ctx) {
	vb2_error_t ret = VbDisplayGroot(ctx, menus[peek()].screen,
			force_redraw, current_menu_idx, disabled_idx_mask);
	force_redraw = 0;
	return ret;
}

/* Flash the screen to black to catch user awareness, then redraw menu. */
static void vb2_flash_screen(struct vb2_context *ctx)
{
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	VbExSleepMs(50);
	vb2_draw_current_screen(ctx);
}

static void vb2_log_menu_change(void)
{
  /*VB_GROOT */current_menu = peek();
	if (menus[current_menu].size)
		VB2_DEBUG("================ %s Menu ================ [ %s ]\n",
			  menus[current_menu].name,
			  menus[current_menu].items[current_menu_idx].text);
	else
		VB2_DEBUG("=============== %s Screen ===============\n",
			  menus[current_menu].name);
}

/**
 * Switch to a new menu (but don't draw it yet).
 *
 * @param new_current_menu:	new menu to set current_menu to
 * @param new_current_menu_idx: new idx to set current_menu_idx to
 */
static void vb2_change_menu(VB_GROOT new_current_menu,
			    int new_current_menu_idx)
{
	current_menu = new_current_menu;

	// push new menu onto the stack (current_menu should already be there)
	prev_menu = peek();
	current_menu = new_current_menu;
	push(new_current_menu);

	/* Reconfigure disabled_idx_mask for the new menu */
	disabled_idx_mask = 0;
	/* Disable Network Boot Option */
	/* if (current_menu == VB_GROOT_DEV) */
	/* 	disabled_idx_mask |= 1 << VB_DEV_NETWORK; */
	/* Disable cancel option if enterprise disabled dev mode */
	if (current_menu == VB_GROOT_TO_NORM &&
	    disable_dev_boot == 1)
		disabled_idx_mask |= 1 << VB_GROOT_TO_NORM_CANCEL;

	/* Enable menu items for the selected bootloaders */
	if (current_menu == VB_GROOT_ALT_FW) {
		disabled_idx_mask = ~(VbExGetAltFwIdxMask() >> 1);

		/* Make sure 'cancel' is shown even with an invalid mask */
		disabled_idx_mask &= (1 << VB_ALTFW_COUNT) - 1;
	}
	/* We assume that there is at least one enabled item */
	while ((1 << new_current_menu_idx) & disabled_idx_mask)
		new_current_menu_idx++;
	if (new_current_menu_idx < menus[current_menu].size)
		current_menu_idx = new_current_menu_idx;

	VB2_DEBUG("vb2_change_menu: new current_menu = 0x%x\n", current_menu);
	vb2_log_menu_change();
}

/************************
 *    Menu Actions      *
 ************************/

/* Boot from internal disk if allowed. */
static vb2_error_t boot_disk_action(struct vb2_context *ctx)
{
	if (disable_dev_boot) {
		vb2_flash_screen(ctx);
		vb2_error_notify("Developer mode disabled\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	VB2_DEBUG("trying fixed disk\n");
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

/* Boot legacy BIOS if allowed and available. */
static vb2_error_t boot_legacy_action(struct vb2_context *ctx)
{
	if (disable_dev_boot) {
		vb2_flash_screen(ctx);
		vb2_error_notify("Developer mode disabled\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	if (!altfw_allowed) {
		vb2_flash_screen(ctx);
		vb2_error_notify("WARNING: Booting legacy BIOS has not "
				 "been enabled. Refer to the developer"
				 "-mode documentation for details.\n",
				 "Legacy boot is disabled\n",
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	vb2_run_altfw(ctx, VB_ALTFW_DEFAULT);
	vb2_flash_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Boot from USB or SD card if allowed and available. */
static vb2_error_t boot_usb_action(struct vb2_context *ctx)
{
	const char no_kernel[] = "No bootable kernel found on USB/SD.\n";

	if (disable_dev_boot) {
		vb2_flash_screen(ctx);
		vb2_error_notify("Developer mode disabled\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	if (!vb2_nv_get(ctx, VB2_NV_DEV_BOOT_USB) &&
	    !(vb2_get_gbb(ctx)->flags & VB2_GBB_FLAG_FORCE_DEV_BOOT_USB) &&
	    !(vb2_get_fwmp_flags() & FWMP_DEV_ENABLE_USB)) {
		vb2_flash_screen(ctx);
		vb2_error_notify("WARNING: Booting from external media "
				 "(USB/SD) has not been enabled. Refer "
				 "to the developer-mode documentation "
				 "for details.\n",
				 "USB booting is disabled\n",
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	if (VB2_SUCCESS == VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE)) {
		VB2_DEBUG("booting USB\n");
		return VB2_SUCCESS;
	}

	/* Loading kernel failed. Clear recovery request from that. */
	vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST, VB2_RECOVERY_NOT_REQUESTED);
	vb2_flash_screen(ctx);
	vb2_error_notify(no_kernel, NULL, VB_BEEP_FAILED);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_developer_menu(struct vb2_context *ctx)
{
	int menu_idx;
	switch(default_boot) {
	default:
	case VB2_DEV_DEFAULT_BOOT_DISK:
		menu_idx = VB_GROOT_WARN_DISK;
		break;
	case VB2_DEV_DEFAULT_BOOT_USB:
		menu_idx = VB_GROOT_WARN_USB;
		break;
	case VB2_DEV_DEFAULT_BOOT_LEGACY:
		menu_idx = VB_GROOT_WARN_LEGACY;
		break;
	}
	vb2_change_menu(VB_GROOT_DEV, menu_idx);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_dev_warning_menu(struct vb2_context *ctx)
{
  VB2_DEBUG("enter_dev_warning_menu: VB_WARN_POWER_OFF = %d\n", VB_GROOT_WARN_POWER_OFF);
	vb2_change_menu(VB_GROOT_DEV_WARNING, VB_GROOT_WARN_POWER_OFF);
	vb2_draw_current_screen(ctx);
	VB2_DEBUG("exitting enter_dev_warning_menu\n");
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_language_menu(struct vb2_context *ctx)
{
	vb2_change_menu(VB_GROOT_LANGUAGES,
			vb2_nv_get(ctx, VB2_NV_LOCALIZATION_INDEX));
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_recovery_screen(struct vb2_context *ctx, int step)
{
  VB2_DEBUG("enter_recovery_screen: step = %d\n", step);
	if (!vb2_allow_recovery(ctx))
		vb2_change_menu(VB_GROOT_RECOVERY_BROKEN, 0);
	else if (usb_nogood)
		vb2_change_menu(VB_GROOT_RECOVERY_NO_GOOD, 0);
	else
	  switch(step) {
	  case 1:
	  	vb2_change_menu(VB_GROOT_RECOVERY_STEP1, 0);
		break;
	  case 2:
	  	vb2_change_menu(VB_GROOT_RECOVERY_STEP2, 0);
		break;
	  case 3:
	  	vb2_change_menu(VB_GROOT_RECOVERY_STEP3, 0);
		break;
	  default:
		vb2_change_menu(VB_GROOT_RECOVERY_STEP1, 0);
		break;
	  }
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t step_next_recovery_screen(struct vb2_context *ctx)
{
	VB2_DEBUG("entering step_next_recovery_screen\n");
	VB2_DEBUG("before vb2_change_menu\n");
	VB2_DEBUG("current_screen = 0x%x\n", menus[current_menu].screen);
	VB2_DEBUG("current_menu = 0x%x\n", current_menu);
	switch (current_menu) {
	case VB_GROOT_RECOVERY_INSERT:
		vb2_change_menu(VB_GROOT_RECOVERY_STEP0, 0);
		break;
	case VB_GROOT_RECOVERY_STEP0:
		vb2_change_menu(VB_GROOT_RECOVERY_STEP1, 0);
		break;
	case VB_GROOT_RECOVERY_STEP1:
		vb2_change_menu(VB_GROOT_RECOVERY_STEP2, 0);
		break;
	case VB_GROOT_RECOVERY_STEP2:
		vb2_change_menu(VB_GROOT_RECOVERY_STEP3, 0);
		break;
	/* case VB_GROOT_RECOVERY_STEP3: */
	/* 	vb2_change_menu(VB_GROOT_RECOVERY_STEP1, 0); */
	/* 	break; */
	default:
		break;
	}
	vb2_draw_current_screen(ctx);
	VB2_DEBUG("after vb2_change_menu\n");
	VB2_DEBUG("current_screen = 0x%x\n", menus[current_menu].screen);
	VB2_DEBUG("current_menu = 0x%x\n", current_menu);
	VB2_DEBUG("exitting step_next_recovery_screen\n");
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_options_menu(struct vb2_context *ctx)
{
	vb2_change_menu(VB_GROOT_ADV_OPTIONS, VB_GROOT_OPTIONS_CANCEL);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_dev_menu(struct vb2_context *ctx)
{
	const char dev_already_on[] =
		"WARNING: TODEV rejected, developer mode is already on.\n";
	if (vb2_get_sd(ctx)->vbsd->flags & VBSD_BOOT_DEV_SWITCH_ON) {
		vb2_flash_screen(ctx);
		vb2_error_notify(dev_already_on, NULL, VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	vb2_change_menu(VB_GROOT_TO_DEV, VB_GROOT_TO_DEV_CANCEL);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_norm_menu(struct vb2_context *ctx)
{
	vb2_change_menu(VB_GROOT_TO_NORM, VB_GROOT_TO_NORM_CONFIRM);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_boot_usb_menu(struct vb2_context *ctx)
{
	vb2_change_menu(VB_GROOT_BOOT_USB, VB_GROOT_BOOT_USB_NEXT);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Boot alternative bootloader if allowed and available. */
static vb2_error_t enter_altfw_menu(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_altfw_menu()\n");
	if (disable_dev_boot) {
		vb2_flash_screen(ctx);
		vb2_error_beep(VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	if (!altfw_allowed) {
		vb2_flash_screen(ctx);
		vb2_error_no_altfw();
		return VBERROR_KEEP_LOOPING;
	}
	vb2_change_menu(VB_GROOT_ALT_FW, 0);
	vb2_draw_current_screen(ctx);

	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t debug_info_action(struct vb2_context *ctx)
{
	force_redraw = 1;
	VbDisplayDebugInfo(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t show_log_action(struct vb2_context *ctx)
{
	vb2_change_menu(VB_GROOT_SHOW_LOG, VB_GROOT_LOG_PAGE_DOWN);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Return to previous menu */
static vb2_error_t goto_prev_menu(struct vb2_context *ctx)
{
	// pop off current menu and change to new top of the stack
	// NOTE: hacky, but need to pop off two screens because
	// vb2_change_menu will push the new screen back on
	pop();
	prev_menu = pop();

	/* Return to previous menu. */
	VB2_DEBUG("prev_menu = %d\n", prev_menu);
	switch (prev_menu) {
	case VB_GROOT_DEV_WARNING:
		return enter_dev_warning_menu(ctx);
	case VB_GROOT_DEV:
		return enter_developer_menu(ctx);
	case VB_GROOT_TO_NORM:
		return enter_to_norm_menu(ctx);
	case VB_GROOT_TO_DEV:
		return enter_to_dev_menu(ctx);
	case VB_GROOT_ADV_OPTIONS:
		return enter_options_menu(ctx);
	case VB_GROOT_RECOVERY_STEP0:
	  return enter_recovery_screen(ctx, 0);
	case VB_GROOT_RECOVERY_STEP1:
	  return enter_recovery_screen(ctx, 1);
	case VB_GROOT_RECOVERY_STEP2:
	  return enter_recovery_screen(ctx, 2);
	case VB_GROOT_RECOVERY_STEP3:
	  return enter_recovery_screen(ctx, 3);
	case VB_GROOT_RECOVERY_INSERT:
	case VB_GROOT_RECOVERY_NO_GOOD:
	  // send back to first recovery screen for now.  will need to modify later.
	  return enter_recovery_screen(ctx, 0);
	default:
		/* This should never happen. */
		VB2_DEBUG("ERROR: prev_menu state corrupted, force shutdown\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}
}

/* Action when selecting a language entry in the language menu. */
static vb2_error_t language_action(struct vb2_context *ctx)
{
	VbSharedDataHeader *vbsd = vb2_get_sd(ctx)->vbsd;

	/* Write selected language ID back to NVRAM. */
	vb2_nv_set(ctx, VB2_NV_LOCALIZATION_INDEX, current_menu_idx);

	/*
	 * Non-manual recovery mode is meant to be left via hard reset (into
	 * manual recovery mode). Need to commit NVRAM changes immediately.
	 */
	if (vbsd->recovery_reason && !vb2_allow_recovery(ctx))
		vb2_nv_commit(ctx);

	return goto_prev_menu(ctx);
}

/* Action when selecting a bootloader in the alternative firmware menu. */
static vb2_error_t altfw_action(struct vb2_context *ctx)
{
	vb2_run_altfw(ctx, current_menu_idx + 1);
	vb2_flash_screen(ctx);
	VB2_DEBUG(no_legacy);
	VbExDisplayDebugInfo(no_legacy, 0);

	return VBERROR_KEEP_LOOPING;
}

/* Action that enables developer mode and reboots. */
static vb2_error_t to_dev_action(struct vb2_context *ctx)
{
	uint32_t vbsd_flags = vb2_get_sd(ctx)->vbsd->flags;

	/* Sanity check, should never happen. */
	if ((vbsd_flags & VBSD_BOOT_DEV_SWITCH_ON) ||
	    !vb2_allow_recovery(ctx))
		return VBERROR_KEEP_LOOPING;

	VB2_DEBUG("Enabling dev-mode...\n");
	if (TPM_SUCCESS != SetVirtualDevMode(1))
		return VBERROR_TPM_SET_BOOT_MODE_STATE;

	/* This was meant for headless devices, shouldn't really matter here. */
	if (VbExGetSwitches(VB_SWITCH_FLAG_ALLOW_USB_BOOT))
		vb2_nv_set(ctx, VB2_NV_DEV_BOOT_USB, 1);

	VB2_DEBUG("Reboot so it will take effect\n");
	return VBERROR_REBOOT_REQUIRED;
}

/* Action that disables developer mode, shows TO_NORM_CONFIRMED and reboots. */
static vb2_error_t to_norm_action(struct vb2_context *ctx)
{
	if (vb2_get_gbb(ctx)->flags & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON) {
		vb2_flash_screen(ctx);
		vb2_error_notify("WARNING: TONORM prohibited by "
				 "GBB FORCE_DEV_SWITCH_ON.\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	VB2_DEBUG("leaving dev-mode.\n");
	vb2_nv_set(ctx, VB2_NV_DISABLE_DEV_REQUEST, 1);
	vb2_change_menu(VB_GROOT_TO_NORM_CONFIRMED, 0);
	vb2_draw_current_screen(ctx);
	VbExSleepMs(5000);
	return VBERROR_REBOOT_REQUIRED;
}

/* Action that will power off the system. */
static vb2_error_t power_off_action(struct vb2_context *ctx)
{
	VB2_DEBUG("Power off requested from screen 0x%x\n",
		  menus[current_menu].screen);
	return VBERROR_SHUTDOWN_REQUESTED;
}

/**
 * Updates current_menu_idx upon an up/down key press, taking into
 * account disabled indices (from disabled_idx_mask).  The cursor
 * will not wrap, meaning that we block on the 0 or max index when
 * we hit the ends of the menu.
 *
 * @param  key      VOL_KEY_UP = increase index selection
 *                  VOL_KEY_DOWN = decrease index selection.
 *                  Every other key has no effect now.
 */
static void vb2_update_selection(uint32_t key) {
	int idx;

	switch (key) {
	case VB_BUTTON_VOL_UP_SHORT_PRESS:
	case VB_KEY_UP:
		idx = current_menu_idx - 1;
		while (idx >= 0 &&
		       ((1 << idx) & disabled_idx_mask))
		  idx--;
		/* Only update if idx is valid */
		if (idx >= 0)
			current_menu_idx = idx;
		break;
	case VB_BUTTON_VOL_DOWN_SHORT_PRESS:
	case VB_KEY_DOWN:
		idx = current_menu_idx + 1;
		while (idx < menus[current_menu].size &&
		       ((1 << idx) & disabled_idx_mask))
		  idx++;
		/* Only update if idx is valid */
		if (idx < menus[current_menu].size)
			current_menu_idx = idx;
		break;
	default:
		VB2_DEBUG("ERROR: %s called with key 0x%x!\n", __func__, key);
		break;
	}

	vb2_log_menu_change();
}

static vb2_error_t vb2_handle_menu_input(struct vb2_context *ctx,
				       uint32_t key, uint32_t key_flags)
{
	switch (key) {
	case 0:
		/* nothing pressed */
		break;
	case '\t':
		/* Tab = display debug info */
		return debug_info_action(ctx);
	case VB_KEY_ESC:
		/* Esc = redraw screen (to clear old debug info) */
		vb2_draw_current_screen(ctx);
		break;
	case VB_KEY_UP:
	case VB_KEY_DOWN:
	case VB_BUTTON_VOL_UP_SHORT_PRESS:
	case VB_BUTTON_VOL_DOWN_SHORT_PRESS:
		/* Untrusted (USB keyboard) input disabled for TO_DEV menu. */
		if (current_menu == VB_GROOT_TO_DEV &&
		    !(key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD)) {
			vb2_flash_screen(ctx);
			vb2_error_notify("Please use the on-device volume "
					 "buttons to navigate\n",
					 "vb2_handle_menu_input() - Untrusted "
					 "(USB keyboard) input disabled\n",
					 VB_BEEP_NOT_ALLOWED);
			break;
		}

		/* Menuless screens enter OPTIONS on volume button press. */
		if (!menus[current_menu].size) {
			enter_options_menu(ctx);
			break;
		}

		vb2_update_selection(key);
		vb2_draw_current_screen(ctx);
		break;
	case VB_BUTTON_POWER_SHORT_PRESS:
	case VB_KEY_ENTER:
		/* Menuless screens shut down on power button press. */
		if (!menus[current_menu].size)
			return VBERROR_SHUTDOWN_REQUESTED;

		return menus[current_menu].items[current_menu_idx].action(ctx);
	default:
		VB2_DEBUG("pressed key 0x%x\n", key);
		break;
	}

	if (VbWantShutdownGroot(ctx)) {
		VB2_DEBUG("shutdown requested!\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	return VBERROR_KEEP_LOOPING;
}

/* Delay in developer menu */
#define DEV_KEY_DELAY        20       /* Check keys every 20ms */

/* Master table of all menus. Menus with size == 0 count as menuless screens. */
static struct vb2_menu menus[VB_GROOT_COUNT] = {
	[VB_GROOT_DEV_WARNING] = {
		.name = "You're now in developer mode",
		.size = VB_GROOT_WARN_COUNT,
		.screen = VB_SCREEN_DEVELOPER_WARNING_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_WARN_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_WARN_ENABLE_VER] = {
				.text = "Go back to NORMAL MODE",
				.action = enter_to_norm_menu,
			},
			[VB_GROOT_WARN_DISK] = {
				.text = "Boot From Internal Disk",
				.action = boot_disk_action,
			},
			[VB_GROOT_WARN_USB] = {
				.text = "Boot From External Media",
				.action = enter_boot_usb_menu,
			},
			[VB_GROOT_WARN_LEGACY] = {
				.text = "Boot Legacy BIOS",
				.action = enter_altfw_menu,
			},
			[VB_GROOT_WARN_DBG_INFO] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_WARN_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_TO_NORM] = {
		.name = "Confirm returning to NORMAL MODE",
		.size = VB_GROOT_TO_NORM_COUNT,
		.screen = VB_SCREEN_DEVELOPER_TO_NORM_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_TO_NORM_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_TO_NORM_CONFIRM] = {
				.text = "Continue returning to NORMAL MODE",
				.action = to_norm_action,
			},
			[VB_GROOT_TO_NORM_CANCEL] = {
				.text = "Cancel",
				.action = goto_prev_menu,
			},
			[VB_GROOT_TO_NORM_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_TO_DEV] = {
		.name = "TO_DEV Confirmation",
		.size = VB_GROOT_TO_DEV_COUNT,
		.screen = VB_SCREEN_RECOVERY_TO_DEV_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_TO_DEV_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_TO_DEV_CONFIRM] = {
				.text = "Confirm Disabling OS Verification",
				.action = to_dev_action,
			},
			[VB_GROOT_TO_DEV_CANCEL] = {
				.text = "Cancel",
				.action = goto_prev_menu,
			},
			[VB_GROOT_TO_DEV_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_LANGUAGES] = {
		.name = "Language Selection",
		.screen = VB_SCREEN_LANGUAGES_MENU,
		/* Rest is filled out dynamically by vb2_init_menus() */
	},
	[VB_GROOT_ADV_OPTIONS] = {
		.name = "Options",
		.size = VB_GROOT_OPTIONS_COUNT,
		.screen = VB_SCREEN_OPTIONS_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_OPTIONS_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_OPTIONS_TO_DEV] = {
				.text = "Switch to Developer Mode",
				.action = enter_to_dev_menu,
			},
			[VB_GROOT_OPTIONS_DBG_INFO] = {
				.text = "View Debug Info",
				.action = debug_info_action,
			},
			[VB_GROOT_OPTIONS_BIOS_LOG] = {
				.text = "View BIOS log",
				.action = show_log_action,
			},
			[VB_GROOT_OPTIONS_CANCEL] = {
				.text = "Back",
				.action = goto_prev_menu,
			},
			[VB_GROOT_OPTIONS_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_DEBUG_INFO] = {
		.name = "Recovery INSERT",
		.size = VB_GROOT_DEBUG_COUNT,
		.screen = VB_SCREEN_RECOVERY_INSERT,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_DEBUG_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_DEBUG_CANCEL] = {
				.text = "Back",
				.action = goto_prev_menu,
			},
			[VB_GROOT_DEBUG_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},		
	},
	[VB_GROOT_RECOVERY_INSERT] = {
		.name = "Recovery INSERT",
		.size = VB_GROOT_REC_COUNT,
		.screen = VB_SCREEN_RECOVERY_INSERT,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_OPTIONS_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_BEGIN] = {
				.text = "Begin",
				.action = step_next_recovery_screen,
			},
			[VB_GROOT_REC_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_REC_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_RECOVERY_NO_GOOD] = {
		.name = "Recovery NO_GOOD",
		.size = 0,
		.screen = VB_SCREEN_RECOVERY_NO_GOOD,
		.items = NULL,
	},
	[VB_GROOT_RECOVERY_BROKEN] = {
		.name = "Non-manual Recovery (BROKEN)",
		.size = 0,
		.screen = VB_SCREEN_OS_BROKEN,
		.items = NULL,
	},
	[VB_GROOT_TO_NORM_CONFIRMED] = {
		.name = "TO_NORM Interstitial",
		.size = 0,
		.screen = VB_SCREEN_TO_NORM_CONFIRMED,
		.items = NULL,
	},
	[VB_GROOT_ALT_FW] = {
		.name = "Alternative Firmware Selection",
		.screen = VB_SCREEN_ALT_FW_MENU,
		.size = VB_ALTFW_COUNT + 1,
		.items = (struct vb2_menu_item[]) {{
				.text = "Bootloader 1",
				.action = altfw_action,
			}, {
				.text = "Bootloader 2",
				.action = altfw_action,
			}, {
				.text = "Bootloader 3",
				.action = altfw_action,
			}, {
				.text = "Bootloader 4",
				.action = altfw_action,
			}, {
				.text = "Bootloader 5",
				.action = altfw_action,
			}, {
				.text = "Bootloader 6",
				.action = altfw_action,
			}, {
				.text = "Bootloader 7",
				.action = altfw_action,
			}, {
				.text = "Bootloader 8",
				.action = altfw_action,
			}, {
				.text = "Bootloader 9",
				.action = altfw_action,
			}, {
				.text = "Cancel",
				.action = enter_developer_menu,
			},
		},
	},
	[VB_GROOT_RECOVERY_STEP0] = {
		.name = "Recovery Step 0: Let's step you through the recovery process",
		.size = VB_GROOT_REC_STEP0_COUNT,
		.screen = VB_SCREEN_RECOVERY_STEP0,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP0_LANGUAGE] = {
				.text = "Step 0: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP0_NEXT] = {
				.text = "Step 0: Next (external disk)",
				.action = step_next_recovery_screen,
			},
			/* [VB_GROOT_REC_STEP0_BACK] = { */
			/* 	.text = "Step 0: Next (phone)", */
			/* 	.action = goto_prev_menu, */
			/* }, */
			[VB_GROOT_REC_STEP0_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_REC_STEP0_POWER_OFF] = {
				.text = "Step 0: Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_RECOVERY_STEP1] = {
		.name = "Recovery Step 1: Let's setp you through the recovery process",
		.size = VB_GROOT_REC_STEP1_COUNT,
		.screen = VB_SCREEN_RECOVERY_STEP1,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP1_LANGUAGE] = {
				.text = "Step 1: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP1_NEXT] = {
				.text = "Step 1: Next",
				.action = step_next_recovery_screen,
			},
			[VB_GROOT_REC_STEP1_BACK] = {
				.text = "Step 1: Back",
				.action = goto_prev_menu,
			},
			[VB_GROOT_REC_STEP1_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_REC_STEP1_POWER_OFF] = {
				.text = "Step 1: Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_RECOVERY_STEP2] = {
		.name = "Recovery Step 2: External Disk Setup",
		.size = VB_GROOT_REC_STEP2_COUNT,
		.screen = VB_SCREEN_RECOVERY_STEP2,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP2_LANGUAGE] = {
				.text = "Step 2: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP2_NEXT] = {
				.text = "Step 2: Next",
				.action = step_next_recovery_screen,
			},
			[VB_GROOT_REC_STEP2_BACK] = {
				.text = "Step 2: Back",
				.action = goto_prev_menu,
			},
			[VB_GROOT_REC_STEP2_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_REC_STEP2_POWER_OFF] = {
				.text = "Step 2: Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_RECOVERY_STEP3] = {
		.name = "Recovery Step 3: Plug in USB",
		.size = VB_GROOT_REC_STEP3_COUNT,
		.screen = VB_SCREEN_RECOVERY_STEP3,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP3_LANGUAGE] = {
				.text = "Step 3: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP3_BACK] = {
				.text = "Step 3: Back",
				.action = goto_prev_menu,
			},
			[VB_GROOT_REC_STEP3_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
			[VB_GROOT_REC_STEP3_POWER_OFF] = {
				.text = "Step 3: Power Off",
				.action = power_off_action,
			},
		},
	},
	[VB_GROOT_SHOW_LOG] = {
		.name = "Recovery Step 3: Plug in USB",
		.size = VB_GROOT_LOG_COUNT,
		.screen = VB_SCREEN_LOG,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_LOG_PAGE_UP] = {
				.text = "Page Up",
				//.action = 
			},
			[VB_GROOT_LOG_PAGE_DOWN] = {
				.text = "Page Down",
				//.action =
			},
			[VB_GROOT_LOG_BACK] = {
				.text = "Back",
				.action = goto_prev_menu,
			},
		},
	},
	[VB_GROOT_BOOT_USB] = {
		.name = "Boot from external media",
		.size = VB_GROOT_BOOT_USB_COUNT,
		.screen = VB_SCREEN_BOOT_USB_CONFIRM,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_BOOT_USB_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_BOOT_USB_NEXT] = {
				.text = "Boot From USB",
				.action = boot_usb_action,
			},
			[VB_GROOT_BOOT_USB_CANCEL] = {
				.text = "Cancel",
				.action = goto_prev_menu,
			},
			[VB_GROOT_BOOT_USB_POWER_OFF] = {
				.text = "Power Off",
				.action = power_off_action,
			},
		},
	},
};

/* Initialize menu state. Must be called once before displaying any menus. */
static vb2_error_t vb2_init_menus(struct vb2_context *ctx)
{
	struct vb2_menu_item *items;
	uint32_t count;
	int i;

	/* Initialize language menu with the correct amount of entries. */
	VbExGetLocalizationCount(&count);
	if (!count)
		count = 1;	/* Always need at least one language entry. */

	items = malloc(count * sizeof(struct vb2_menu_item));
	if (!items)
		return VB2_ERROR_UNKNOWN;

	for (i = 0; i < count; i++) {
		/* The actual language is drawn by the bootloader */
		items[i].text = "Some Language";
		items[i].action = language_action;
	}
	menus[VB_GROOT_LANGUAGES].size = count;
	menus[VB_GROOT_LANGUAGES].items = items;

	return VB2_SUCCESS;
}

/**
 * Main function that handles developer warning menu functionality
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
static vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	vb2_error_t ret;

	/* Check if the default is to boot using disk, usb, or legacy */
	default_boot = vb2_nv_get(ctx, VB2_NV_DEV_DEFAULT_BOOT);
	if (gbb->flags & VB2_GBB_FLAG_DEFAULT_DEV_BOOT_LEGACY)
		default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;

	/* Check if developer mode is disabled by FWMP */
	disable_dev_boot = 0;
	if (vb2_get_fwmp_flags() & FWMP_DEV_DISABLE_BOOT) {
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
	    (vb2_get_fwmp_flags() & FWMP_DEV_ENABLE_LEGACY);

	/* Show appropriate initial menu */
	if (disable_dev_boot)
		enter_to_norm_menu(ctx);
	else
		enter_dev_warning_menu(ctx);

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted */
	do {
		uint32_t key = VbExKeyboardRead();

		/* Make sure user knows dev mode disabled */
		if (disable_dev_boot)
			VbExDisplayDebugInfo(dev_disable_msg, 0);

		switch (key) {
		case VB_BUTTON_VOL_DOWN_LONG_PRESS:
		case VB_KEY_CTRL('D'):
			/* Ctrl+D = boot from internal disk */
			ret = boot_disk_action(ctx);
			break;
		case VB_KEY_CTRL('L'):
			/* Ctrl+L = boot alternative bootloader */
			ret = enter_altfw_menu(ctx);
			break;
		case VB_BUTTON_VOL_UP_LONG_PRESS:
		case VB_KEY_CTRL('U'):
			/* Ctrl+U = boot from USB or SD card */
			ret = boot_usb_action(ctx);
			break;
		/* We allow selection of the default '0' bootloader here */
		case '0'...'9':
			VB2_DEBUG("VbBootDeveloper() - "
				  "user pressed key '%c': Boot alternative "
				  "firmware\n", key);
			vb2_try_alt_fw(ctx, altfw_allowed, key - '0');
			ret = VBERROR_KEEP_LOOPING;
			break;
		default:
			ret = vb2_handle_menu_input(ctx, key, 0);
			break;
		}

		/* We may have loaded a kernel or decided to shut down now. */
		if (ret != VBERROR_KEEP_LOOPING)
			return ret;

		/* Reset 30 second timer whenever we see a new key. */
		if (key != 0)
			vb2_audio_start(ctx);

		VbExSleepMs(DEV_KEY_DELAY);

		/* If dev mode was disabled, loop forever (never timeout) */
	} while (disable_dev_boot ? 1 : vb2_audio_looping());

	if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY)
		boot_legacy_action(ctx);	/* Doesn't return on success. */

	if (default_boot == VB2_DEV_DEFAULT_BOOT_USB)
		if (VB2_SUCCESS == boot_usb_action(ctx))
			return VB2_SUCCESS;

	return boot_disk_action(ctx);
}

/* Developer mode entry point. */
vb2_error_t VbBootDeveloperGroot(struct vb2_context *ctx)
{
	vb2_error_t retval = vb2_init_menus(ctx);
	if (VB2_SUCCESS != retval)
		return retval;
	retval = vb2_developer_menu(ctx);
	VbDisplayScreen(ctx, VB_SCREEN_BLANK, 0, NULL);
	return retval;
}

/* Main function that handles non-manual recovery (BROKEN) menu functionality */
static vb2_error_t broken_ui(struct vb2_context *ctx)
{
	VbSharedDataHeader *vbsd = vb2_get_sd(ctx)->vbsd;

	/*
	 * Temporarily stash recovery reason in subcode so we'll still know what
	 * to display if the user reboots into manual recovery from here. Commit
	 * immediately since the user may hard-reset out of here.
	 */
	VB2_DEBUG("saving recovery reason (%#x)\n", vbsd->recovery_reason);
	vb2_nv_set(ctx, VB2_NV_RECOVERY_SUBCODE, vbsd->recovery_reason);
	vb2_nv_commit(ctx);

	enter_recovery_screen(ctx, 0);

	/* Loop and wait for the user to reset or shut down. */
	VB2_DEBUG("waiting for manual recovery\n");
	while (1) {
		uint32_t key = VbExKeyboardRead();
		vb2_error_t ret = vb2_handle_menu_input(ctx, key, 0);
		if (ret != VBERROR_KEEP_LOOPING)
			return ret;
	}
}

/* Delay in recovery mode */
#define REC_DISK_DELAY       1000     /* Check disks every 1s */
#define REC_KEY_DELAY        20       /* Check keys every 20ms */
#define REC_MEDIA_INIT_DELAY 500      /* Check removable media every 500ms */

/**
 * Main function that handles recovery menu functionality
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
		VB2_DEBUG("attempting to load kernel2\n");
		vb2_log_menu_change();
		VB2_DEBUG("current_menu_idx = %d\n", current_menu_idx);
		ret = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		/*
		 * Clear recovery requests from failed kernel loading, since
		 * we're already in recovery mode.  Do this now, so that
		 * powering off after inserting an invalid disk doesn't leave
		 * us stuck in recovery mode.
		 */
		vb2_nv_set(ctx, VB2_NV_RECOVERY_REQUEST,
			   VB2_RECOVERY_NOT_REQUESTED);

		if (VB2_SUCCESS == ret)
			return ret; /* Found a recovery kernel */

		if (usb_nogood != (ret != VBERROR_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_nogood = ret != VBERROR_NO_DISK_FOUND;
			enter_recovery_screen(ctx, 1);
		}

		/*
		 * Scan keyboard more frequently than media, since x86
		 * platforms don't like to scan USB too rapidly.
		 */
		for (i = 0; i < REC_DISK_DELAY; i += REC_KEY_DELAY) {
			key = VbExKeyboardReadWithFlags(&key_flags);
			if (key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS ||
			    key == VB_KEY_CTRL('D')) {  // NOTE: This is for debugging ONLY.  Take out when committing code.
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
			VbExSleepMs(REC_KEY_DELAY);
		}
	}
}

/* Recovery mode entry point. */
vb2_error_t VbBootRecoveryGroot(struct vb2_context *ctx)
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
