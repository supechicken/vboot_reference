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
#include "2rsa.h"
#include "2secdata.h"
#include "2ui.h"
#include "2sysincludes.h"
#include "2ui_private.h"
#include "load_kernel_fw.h"
#include "utility.h"
#include "vb2_common.h"
#include "vboot_api.h"
#include "vboot_audio.h"
#include "vboot_display.h"
#include "vboot_kernel.h"
#include "vboot_struct.h"
#include "vboot_ui_legacy_common.h"

/* Delay type (in ms) of developer and recovery mode menu looping */
#define KEY_DELAY		20	/* Check keyboard inputs */

/* Global variables */
static enum {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED, /* must have been previously released */
} power_button_state;

static const char dev_disable_msg[] =
	"Developer mode is disabled on this device by system policy.\n"
	"For more information, see http://dev.chromium.org/chromium-os/fwmp\n"
	"\n";

static VB_GROOT current_menu;
static int current_menu_idx, disabled_idx_mask;
static uint32_t current_page, num_page;
static uint32_t altfw_allowed;
static struct vb2_menu menus[];
static const char no_legacy[] = "Legacy boot failed. Missing BIOS?\n";

/*****************************************************************************/
/* Utilities */

/*
 * Stack implementation
 *
 * The stack is used to record screen history.
 */
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
	if (isempty()) {
		VB2_DEBUG("ERROR: calling peek() when stack is empty\n");
		return -1;
	}

	VB2_DEBUG("***** peek(0x%x), top = %d\n", stack[top], top);
	return stack[top];
}

static VB_GROOT pop(void) {
	VB_GROOT screen;

	if (!isempty()) {
		screen = stack[top];
		top--;
		VB2_DEBUG("***** pop(0x%x), top = %d\n", screen, top);
		return screen;
	}
	else
		return -1;
}

static int push(VB_GROOT screen) {
	VB2_DEBUG("***** push(0x%x), top = %d\n", screen, top);
	if (!isfull()) {
		top++;
		stack[top] = screen;
		return 0;
	}
	else
		return -1;
}

/* end of stack implementation */

// implementing increasement/decreasement of current_page
// check and return current_page after the modification

static uint32_t increase_current_page(void)
{
	if (current_page < num_page - 1)
		current_page++;
	return current_page;
}

static uint32_t decrease_current_page(void)
{
	if (current_page > 0)
		current_page--;
	return current_page;
}

//// end of current_page modification implementation

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * Returns true if a shutdown is required and false if no shutdown is required.
 */
static int VbWantShutdownGroot(struct vb2_context *ctx, uint32_t key)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	uint32_t shutdown_request = VbExIsShutdownRequested();

	/*
	 * Ignore power button push until after we have seen it released.
	 * This avoids shutting down immediately if the power button is still
	 * being held on startup. After we've recognized a valid power button
	 * push then don't report the event until after the button is released.
	 */
	if (shutdown_request & VB_SHUTDOWN_REQUEST_POWER_BUTTON) {
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		if (power_button_state == POWER_BUTTON_RELEASED)
			power_button_state = POWER_BUTTON_PRESSED;
	} else {
		if (power_button_state == POWER_BUTTON_PRESSED)
			shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		power_button_state = POWER_BUTTON_RELEASED;
	}

	if (key == VB_BUTTON_POWER_SHORT_PRESS)
		shutdown_request |= VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	/* If desired, ignore shutdown request due to lid closure. */
	if (gbb->flags & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_LID_CLOSED;

	/*
	 * In detachables, disabling shutdown due to power button.
	 * We are using it for selection instead.
	 */
	if (DETACHABLE)
		shutdown_request &= ~VB_SHUTDOWN_REQUEST_POWER_BUTTON;

	return !!shutdown_request;
}

/* (Re-)Draw the menu identified by current_menu[_idx] to the screen. */
static vb2_error_t vb2_draw_current_screen(struct vb2_context *ctx)
{
	return VbDisplayMenu(ctx, menus[peek()].screen,
			     0, current_menu_idx, disabled_idx_mask,
			     current_page);
}

/* Flash the screen to black to catch user awareness, then redraw menu. */
static void vb2_flash_screen(struct vb2_context *ctx)
{
	VbDisplayScreen(ctx, VB2_SCREEN_BLANK, 0, NULL);
	VbExSleepMs(50);
	vb2_draw_current_screen(ctx);
}

static void vb2_log_menu_change(void)
{
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
 * @param ctx:			Vboot2 context
 * @param new_current_menu:	new menu to set current_menu to
 * @param new_current_menu_idx: new idx to set current_menu_idx to
 */
static void vb2_change_menu(struct vb2_context *ctx,
			    VB_GROOT new_current_menu, int new_current_menu_idx)
{
	// push new menu onto the stack (current_menu should already be there)
	if (isempty() || current_menu != new_current_menu) {
		push(new_current_menu);
		current_menu = new_current_menu;
	}

	/* Reconfigure disabled_idx_mask for the new menu */
	disabled_idx_mask = 0;

	/* Disable Network Boot Option */
	/* if (current_menu == VB_GROOT_DEV) */
	/* 	disabled_idx_mask |= 1 << VB_DEV_NETWORK; */

	/* Disable cancel option if enterprise disabled dev mode */
	if (current_menu == VB_GROOT_TO_NORM && !vb2_dev_boot_allowed(ctx))
		disabled_idx_mask |= 1 << VB_GROOT_TO_NORM_CANCEL;

	/* Enable menu items for the selected bootloaders */
	if (current_menu == VB_GROOT_ALT_FW) {
		disabled_idx_mask = ~(VbExGetAltFwIdxMask() >> 1);

		/* Make sure 'cancel' is shown even with an invalid mask */
		disabled_idx_mask &= (1 << VB_ALTFW_COUNT) - 1;
	}

	if (current_menu == VB_GROOT_DEBUG_INFO || current_menu == VB_GROOT_SHOW_LOG) {
		if (current_page == 0)
			disabled_idx_mask |= 1 << VB_GROOT_LOG_PAGE_UP;
		if (current_page == num_page - 1)
			disabled_idx_mask |= 1 << VB_GROOT_LOG_PAGE_DOWN;
	}

	/* We assume that there is at least one enabled item */
	while ((1 << new_current_menu_idx) & disabled_idx_mask)
		new_current_menu_idx++;
	if (new_current_menu_idx < menus[current_menu].size)
		current_menu_idx = new_current_menu_idx;

	VB2_DEBUG("vb2_change_menu: new current_menu = 0x%x\n", current_menu);
	vb2_log_menu_change();
}

/*****************************************************************************/
/* Menu actions */

/* Boot from internal disk if allowed. */
static vb2_error_t boot_from_internal_action(struct vb2_context *ctx)
{
	if (!vb2_dev_boot_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_notify("Developer mode disabled\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	VB2_DEBUG("trying fixed disk\n");

	vb2_change_menu(ctx, VB_GROOT_BOOT_FROM_INTERNAL, 0);
	vb2_draw_current_screen(ctx);
	return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
}

/* Boot legacy BIOS if allowed and available. */
static vb2_error_t boot_legacy_action(struct vb2_context *ctx)
{
	if (!vb2_dev_boot_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_notify("Developer mode disabled\n", NULL,
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	if (!vb2_altfw_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_notify("WARNING: Booting legacy BIOS has not "
				 "been enabled. Refer to the developer"
				 "-mode documentation for details.\n",
				 "Legacy boot is disabled\n",
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	vb2_try_altfw(ctx, 1, VB_ALTFW_DEFAULT);
	vb2_flash_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Boot from USB or SD card if allowed and available. */
static vb2_error_t boot_usb_action(struct vb2_context *ctx)
{
	const char no_kernel[] = "No bootable kernel found on USB/SD.\n";

	if (!vb2_boot_usb_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_notify("WARNING: Booting from external media "
				 "(USB/SD) has not been enabled. Refer "
				 "to the developer-mode documentation "
				 "for details.\n",
				 "USB booting is disabled\n",
				 VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}

	if (VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE) == VB2_SUCCESS) {
		VB2_DEBUG("booting from USB\n");
		return VB2_SUCCESS;
	}

	vb2_flash_screen(ctx);
	vb2_error_notify(no_kernel, NULL, VB_BEEP_FAILED);

	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_developer_menu(struct vb2_context *ctx)
{
	int menu_idx;
	enum vb2_dev_default_boot default_boot = vb2_get_dev_boot_target(ctx);
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
	vb2_change_menu(ctx, VB_GROOT_DEV, menu_idx);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_dev_warning_menu(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_dev_warning_menu\n");
	vb2_change_menu(ctx, VB_GROOT_DEV_WARNING, VB_GROOT_WARN_DISK);
	vb2_draw_current_screen(ctx);
	VB2_DEBUG("exitting enter_dev_warning_menu\n");
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_language_menu(struct vb2_context *ctx)
{
	vb2_change_menu(ctx, VB_GROOT_LANGUAGES,
			vb2_nv_get(ctx, VB2_NV_LOCALIZATION_INDEX));
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_recovery_screen(struct vb2_context *ctx, int step)
{
	VB2_DEBUG("enter_recovery_screen: step = %d\n", step);
	if (!vb2_allow_recovery(ctx))
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_BROKEN, 0);
	else
	  switch(step) {
	  case 0:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP0,
				VB_GROOT_REC_STEP0_NEXT);
		break;
	  case 1:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP1,
				VB_GROOT_REC_STEP1_NEXT);
		break;
	  case 2:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP2,
				VB_GROOT_REC_STEP2_NEXT);
		break;
	  case 3:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP3,
				VB_GROOT_REC_STEP3_BACK);
		break;
	  default:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP0,
				VB_GROOT_REC_STEP0_NEXT);
		break;
	  }
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_usb_nogood_screen(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_usb_nogood_screen\n");
	vb2_change_menu(ctx, VB_GROOT_RECOVERY_NO_GOOD, 0);
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
	case VB_GROOT_RECOVERY_STEP0:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP1,
				VB_GROOT_REC_STEP1_NEXT);
		break;
	case VB_GROOT_RECOVERY_STEP1:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP2,
				VB_GROOT_REC_STEP2_NEXT);
		break;
	case VB_GROOT_RECOVERY_STEP2:
		vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP3,
				VB_GROOT_REC_STEP3_BACK);
		break;
	/* case VB_GROOT_RECOVERY_STEP3: */
	/* 	vb2_change_menu(ctx, VB_GROOT_RECOVERY_STEP1, 0); */
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
	vb2_change_menu(ctx, VB_GROOT_ADV_OPTIONS, VB_GROOT_OPTIONS_CANCEL);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_dev_menu(struct vb2_context *ctx)
{
	const char dev_already_on[] =
		"WARNING: TODEV rejected, developer mode is already on.\n";
	if (ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) {
		vb2_flash_screen(ctx);
		vb2_error_notify(dev_already_on, NULL, VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	vb2_change_menu(ctx, VB_GROOT_TO_DEV, VB_GROOT_TO_DEV_CANCEL);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_to_norm_menu(struct vb2_context *ctx)
{
	vb2_change_menu(ctx, VB_GROOT_TO_NORM, VB_GROOT_TO_NORM_CONFIRM);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t enter_boot_from_external_menu(struct vb2_context *ctx)
{
	if (!vb2_boot_usb_allowed(ctx))
		return VBERROR_KEEP_LOOPING;
	vb2_change_menu(ctx, VB_GROOT_BOOT_FROM_EXTERNAL, VB_GROOT_BOOT_USB_BACK);
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

/* Boot alternative bootloader if allowed and available. */
static vb2_error_t enter_altfw_menu(struct vb2_context *ctx)
{
	VB2_DEBUG("enter_altfw_menu()\n");
	if (!vb2_dev_boot_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_beep(VB_BEEP_NOT_ALLOWED);
		return VBERROR_KEEP_LOOPING;
	}
	if (!vb2_altfw_allowed(ctx)) {
		vb2_flash_screen(ctx);
		vb2_error_no_altfw();
		return VBERROR_KEEP_LOOPING;
	}
	vb2_change_menu(ctx, VB_GROOT_ALT_FW, 0);
	vb2_draw_current_screen(ctx);

	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t debug_info(struct vb2_context *ctx)
{
	VB2_DEBUG("num_page = %u, page = %u\n", num_page, current_page); //XXX
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

#define DEBUG_INFO_SIZE 512

static vb2_error_t debug_info_action(struct vb2_context *ctx)
{
	vb2_error_t rv;
	current_page = 0;
	char buf[DEBUG_INFO_SIZE];
	VbGetDebugInfoString(ctx, buf, DEBUG_INFO_SIZE);
	rv = VbExInitPageContent(buf, &num_page, VB2_SCREEN_DEBUG_INFO);
	if (rv != VB2_SUCCESS)
		return rv;

	vb2_change_menu(ctx, VB_GROOT_DEBUG_INFO, VB_GROOT_DEBUG_PAGE_DOWN);
	return debug_info(ctx);
}

static vb2_error_t show_log(struct vb2_context *ctx)
{
	VB2_DEBUG("num_page = %u, page = %u\n", num_page, current_page); //XXX
	vb2_draw_current_screen(ctx);
	return VBERROR_KEEP_LOOPING;
}

static vb2_error_t show_log_action(struct vb2_context *ctx)
{
	vb2_error_t rv;
	current_page = 0;
	rv = VbExInitPageContent(NULL, &num_page, VB2_SCREEN_BIOS_LOG);
	if (rv != VB2_SUCCESS)
		return rv;

	vb2_change_menu(ctx, VB_GROOT_SHOW_LOG, VB_GROOT_LOG_PAGE_DOWN);
	return show_log(ctx);
}

/* Return to previous menu */
static vb2_error_t goto_prev_menu(struct vb2_context *ctx)
{
	// pop off current menu and change to new top of the stack
	// NOTE: hacky, but need to pop off two screens because
	// vb2_change_menu will push the new screen back on
	pop();
	VB_GROOT prev_menu = pop();

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
	case VB_GROOT_RECOVERY_NO_GOOD:
	  return enter_usb_nogood_screen(ctx);
	case VB_GROOT_RECOVERY_BROKEN:
	  // TODO(phoenixshen): send back to first recovery screen for now.
	  // need to modify later.
	  return enter_recovery_screen(ctx, 0);
	default:
		/* This should never happen. */
		VB2_DEBUG("ERROR: unknown prev_menu %u, force shutdown\n",
			  prev_menu);
		return VBERROR_SHUTDOWN_REQUESTED;
	}
}

static vb2_error_t debug_info_page_up_action(struct vb2_context *ctx)
{
	decrease_current_page();
	vb2_change_menu(ctx, VB_GROOT_DEBUG_INFO, VB_GROOT_DEBUG_PAGE_UP);
	return debug_info(ctx);
}

static vb2_error_t debug_info_page_down_action(struct vb2_context *ctx)
{
	if (increase_current_page() == num_page - 1)
		vb2_change_menu(ctx, VB_GROOT_DEBUG_INFO,
				VB_GROOT_DEBUG_PAGE_UP);
	else
		vb2_change_menu(ctx, VB_GROOT_DEBUG_INFO,
				VB_GROOT_DEBUG_PAGE_DOWN);
	return debug_info(ctx);
}

static vb2_error_t show_log_page_up_action(struct vb2_context *ctx)
{
	decrease_current_page();
	vb2_change_menu(ctx, VB_GROOT_SHOW_LOG, VB_GROOT_LOG_PAGE_UP);
	return show_log(ctx);
}

static vb2_error_t show_log_page_down_action(struct vb2_context *ctx)
{
	if (increase_current_page() == num_page - 1)
		vb2_change_menu(ctx, VB_GROOT_SHOW_LOG, VB_GROOT_LOG_PAGE_UP);
	else
		vb2_change_menu(ctx, VB_GROOT_SHOW_LOG, VB_GROOT_LOG_PAGE_DOWN);
	return show_log(ctx);
}

static vb2_error_t free_log_prev_menu_action(struct vb2_context *ctx)
{
	switch(current_menu) {
	case VB_GROOT_DEBUG_INFO:
	case VB_GROOT_SHOW_LOG:
		VbExFreePageContent();
		break;
	default:
		/* This should never happen */
		VB2_DEBUG("ERROR: no log to free in current_menu %u, force shutdown\n",
			  current_menu);
		return VBERROR_SHUTDOWN_REQUESTED;
	}
	return goto_prev_menu(ctx);
}

/* Action when selecting a language entry in the language menu. */
static vb2_error_t language_action(struct vb2_context *ctx)
{
	/* Write selected language ID back to NVRAM. */
	vb2_nv_set(ctx, VB2_NV_LOCALIZATION_INDEX, current_menu_idx);

	/*
	 * Non-manual recovery mode is meant to be left via three-finger
	 * salute (into manual recovery mode). Need to commit nvdata
	 * changes immediately.  Ignore commit errors in recovery mode.
	 */
	if ((ctx->flags & VB2_CONTEXT_RECOVERY_MODE) &&
	    !vb2_allow_recovery(ctx))
		vb2ex_commit_data(ctx);

	return goto_prev_menu(ctx);
}

/* Action when selecting a bootloader in the alternative firmware menu. */
static vb2_error_t altfw_action(struct vb2_context *ctx)
{
	vb2_try_altfw(ctx, 1, current_menu_idx + 1);
	vb2_flash_screen(ctx);
	VB2_DEBUG(no_legacy);
	VbExDisplayDebugInfo(no_legacy, 0);

	return VBERROR_KEEP_LOOPING;
}

/* Action that enables developer mode and reboots. */
static vb2_error_t to_dev_action(struct vb2_context *ctx)
{
	/* Sanity check, should never happen. */
	if ((ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !vb2_allow_recovery(ctx))
		return VBERROR_KEEP_LOOPING;

	VB2_DEBUG("Enabling dev-mode...\n");
	if (VB2_SUCCESS != vb2_enable_developer_mode(ctx))
		return VBERROR_TPM_SET_BOOT_MODE_STATE;

	/* This was meant for headless devices, shouldn't really matter here. */
	if (USB_BOOT_ON_DEV)
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
	vb2_change_menu(ctx, VB_GROOT_TO_NORM_CONFIRMED, 0);
	vb2_draw_current_screen(ctx);
	VbExSleepMs(5000);
	return VBERROR_REBOOT_REQUIRED;
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
		if (DETACHABLE) {
			/* Menuless screens shut down on power button press. */
			if (!menus[current_menu].size)
				return VBERROR_SHUTDOWN_REQUESTED;

			return menus[current_menu].items[current_menu_idx]
				.action(ctx);
		}
		break;
	case VB_KEY_ENTER:
		/* Menuless screens shut down on power button press. */
		if (!menus[current_menu].size)
			return VBERROR_SHUTDOWN_REQUESTED;

		return menus[current_menu].items[current_menu_idx].action(ctx);
	default:
		VB2_DEBUG("pressed key 0x%x\n", key);
		break;
	}

	if (VbWantShutdownGroot(ctx, key)) {
		VB2_DEBUG("shutdown requested!\n");
		return VBERROR_SHUTDOWN_REQUESTED;
	}

	return VBERROR_KEEP_LOOPING;
}

/* Master table of all menus. Menus with size == 0 count as menuless screens. */
static struct vb2_menu menus[VB_GROOT_COUNT] = {
	[VB_GROOT_DEV_WARNING] = {
		.name = "You're now in dev mode",
		.size = VB_GROOT_WARN_COUNT,
		.screen = VB2_SCREEN_DEVELOPER_WARNING_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_WARN_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_WARN_ENABLE_VER] = {
				.text = "Return to original state",
				.action = enter_to_norm_menu,
			},
			[VB_GROOT_WARN_DISK] = {
				.text = "Boot from internal disk",
				.action = boot_from_internal_action,
			},
			[VB_GROOT_WARN_USB] = {
				.text = "Boot from external disk",
				.action = enter_boot_from_external_menu,
			},
			[VB_GROOT_WARN_LEGACY] = {
				.text = "Boot from legacy mode",
				.action = enter_altfw_menu,
			},
			[VB_GROOT_WARN_DBG_INFO] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
		},
	},
	[VB_GROOT_TO_NORM] = {
		.name = "Confirm returning to original state",
		.size = VB_GROOT_TO_NORM_COUNT,
		.screen = VB2_SCREEN_DEVELOPER_TO_NORM_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_TO_NORM_CONFIRM] = {
				.text = "Continue",
				.action = to_norm_action,
			},
			[VB_GROOT_TO_NORM_CANCEL] = {
				.text = "Cancel",
				.action = goto_prev_menu,
			},
		},
	},
	[VB_GROOT_TO_DEV] = {
		.name = "TO_DEV Confirmation",
		.size = VB_GROOT_TO_DEV_COUNT,
		.screen = VB2_SCREEN_RECOVERY_TO_DEV_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_TO_DEV_CONFIRM] = {
				.text = "Confirm disabling OS verification",
				.action = to_dev_action,
			},
			[VB_GROOT_TO_DEV_CANCEL] = {
				.text = "Cancel",
				.action = goto_prev_menu,
			},
		},
	},
	[VB_GROOT_LANGUAGES] = {
		.name = "Language Selection",
		.screen = VB2_SCREEN_LANGUAGES_MENU,
		/* Rest is filled out dynamically by vb2_init_menus() */
	},
	[VB_GROOT_ADV_OPTIONS] = {
		.name = "Advanced options",
		.size = VB_GROOT_OPTIONS_COUNT,
		.screen = VB2_SCREEN_OPTIONS_MENU,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_OPTIONS_TO_DEV] = {
				.text = "Enable developer mode",
				.action = enter_to_dev_menu,
			},
			[VB_GROOT_OPTIONS_DBG_INFO] = {
				.text = "Debug info",
				.action = debug_info_action,
			},
			[VB_GROOT_OPTIONS_BIOS_LOG] = {
				.text = "BIOS log",
				.action = show_log_action,
			},
			[VB_GROOT_OPTIONS_CANCEL] = {
				.text = "Back",
				.action = goto_prev_menu,
			},
		},
	},
	[VB_GROOT_DEBUG_INFO] = {
		.name = "Debug info",
		.size = VB_GROOT_DEBUG_COUNT,
		.screen = VB2_SCREEN_DEBUG_INFO,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_DEBUG_PAGE_UP] = {
				.text = "Page Up",
				.action = debug_info_page_up_action,
			},
			[VB_GROOT_DEBUG_PAGE_DOWN] = {
				.text = "Page Down",
				.action = debug_info_page_down_action,
			},
			[VB_GROOT_DEBUG_BACK] = {
				.text = "Back",
				.action = free_log_prev_menu_action,
			},
		},
	},
	[VB_GROOT_SHOW_LOG] = {
		.name = "BIOS log",
		.size = VB_GROOT_LOG_COUNT,
		.screen = VB2_SCREEN_BIOS_LOG,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_LOG_PAGE_UP] = {
				.text = "Page Up",
				.action = show_log_page_up_action,
			},
			[VB_GROOT_LOG_PAGE_DOWN] = {
				.text = "Page Down",
				.action = show_log_page_down_action,
			},
			[VB_GROOT_LOG_BACK] = {
				.text = "Back",
				.action = free_log_prev_menu_action,
			},
		},
	},
	[VB_GROOT_RECOVERY_NO_GOOD] = {
		.name = "Recovery NO_GOOD",
		.size = 0,
		.screen = VB2_SCREEN_RECOVERY_NO_GOOD,
		.items = NULL,
	},
	[VB_GROOT_RECOVERY_BROKEN] = {
		.name = "Non-manual Recovery (BROKEN)",
		.size = 0,
		.screen = VB2_SCREEN_OS_BROKEN,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_BROKEN_LANGUAGE] = {
				.text = "Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_BROKEN_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
		},
	},
	[VB_GROOT_TO_NORM_CONFIRMED] = {
		.name = "TO_NORM Interstitial",
		.size = 0,
		.screen = VB2_SCREEN_TO_NORM_CONFIRMED,
		.items = NULL,
	},
	[VB_GROOT_ALT_FW] = {
		.name = "Alternative Firmware Selection",
		.screen = VB2_SCREEN_ALT_FW_MENU,
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
		.name = "Recovery Step 0: "
			"Let's step you through the recovery process",
		.size = VB_GROOT_REC_STEP0_COUNT,
		.screen = VB2_SCREEN_RECOVERY_SELECT,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP0_LANGUAGE] = {
				.text = "Step 0: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP0_NEXT] = {
				.text = "Step 0: Recover using external disk",
				.action = step_next_recovery_screen,
			},
			[VB_GROOT_REC_STEP0_ADV_OPTIONS] = {
				.text = "Advanced Options",
				.action = enter_options_menu,
			},
		},
	},
	[VB_GROOT_RECOVERY_STEP1] = {
		.name = "Recovery Step 1: You'll need",
		.size = VB_GROOT_REC_STEP1_COUNT,
		.screen = VB2_SCREEN_RECOVERY_DISK_STEP1,
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
		},
	},
	[VB_GROOT_RECOVERY_STEP2] = {
		.name = "Recovery Step 2: External Disk Setup",
		.size = VB_GROOT_REC_STEP2_COUNT,
		.screen = VB2_SCREEN_RECOVERY_DISK_STEP2,
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
		},
	},
	[VB_GROOT_RECOVERY_STEP3] = {
		.name = "Recovery Step 3: Plug in USB",
		.size = VB_GROOT_REC_STEP3_COUNT,
		.screen = VB2_SCREEN_RECOVERY_DISK_STEP3,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_REC_STEP3_LANGUAGE] = {
				.text = "Step 3: Language",
				.action = enter_language_menu,
			},
			[VB_GROOT_REC_STEP3_BACK] = {
				.text = "Step 3: Back",
				.action = goto_prev_menu,
			},
		},
	},
	[VB_GROOT_BOOT_FROM_INTERNAL] = {
		.name = "Boot from internal disk",
		.size = 0,
		.screen = VB2_SCREEN_BOOT_FROM_INTERNAL,
		.items = NULL,
	},
	[VB_GROOT_BOOT_FROM_EXTERNAL] = {
		.name = "Boot from external disk",
		.size = VB_GROOT_BOOT_USB_COUNT,
		.screen = VB2_SCREEN_BOOT_FROM_EXTERNAL,
		.items = (struct vb2_menu_item[]){
			[VB_GROOT_BOOT_USB_BACK] = {
				.text = "Back",
				.action = goto_prev_menu,
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

	power_button_state = POWER_BUTTON_HELD_SINCE_BOOT;

	return VB2_SUCCESS;
}

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Show appropriate initial menu */
	if (!vb2_dev_boot_allowed(ctx))
		enter_to_norm_menu(ctx);
	else
		enter_dev_warning_menu(ctx);

	/* Get audio/delay context */
	vb2_audio_start(ctx);

	/* We'll loop until we finish the delay or are interrupted. */
	do {
		/* Make sure user knows dev mode disabled */
		if (!vb2_dev_boot_allowed(ctx))
			VbExDisplayDebugInfo(dev_disable_msg, 0);

		if (peek() == VB_GROOT_BOOT_FROM_EXTERNAL) {
			VB2_DEBUG("attempting to boot from USB\n");
			if (vb2_boot_usb_allowed(ctx)) {
				if (VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE)
				    == VB2_SUCCESS) {
					VB2_DEBUG("booting from USB\n");
					rv = VB2_SUCCESS;
					break;
				}
			}
		}

		/* Scan keyboard inputs. */
		uint32_t key = VbExKeyboardRead();

		if (key == VB_KEY_CTRL('D') ||
		    (key == VB_BUTTON_VOL_DOWN_LONG_PRESS &&
		     DETACHABLE)) {
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

		/* Have loaded a kernel or decided to shut down now. */
		if (rv != VBERROR_KEEP_LOOPING)
			break;

		/* Reset 30 second timer whenever we see a new key. */
		if (key != 0)
			vb2_audio_start(ctx);

		VbExSleepMs(KEY_DELAY);

		/* If dev mode was disabled, loop forever */
	} while (!vb2_dev_boot_allowed(ctx) || vb2_audio_looping());

	/* Timeout, boot from the default option. */
	if (rv == VBERROR_KEEP_LOOPING) {

		enum vb2_dev_default_boot default_boot =
			vb2_get_dev_boot_target(ctx);

		/* Boot legacy does not return on success */
		if (default_boot == VB2_DEV_DEFAULT_BOOT_LEGACY &&
		    vb2_dev_boot_legacy_allowed(ctx) && vb2ex_commit_data(ctx))
			VbExLegacy(VB_ALTFW_DEFAULT);

		if (default_boot == VB2_DEV_DEFAULT_BOOT_USB &&
		    vb2_dev_boot_usb_allowed(ctx) &&
		    VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE) == VB2_SUCCESS)
			return VB2_SUCCESS;

		return VbTryLoadKernel(ctx, VB_DISK_FLAG_FIXED);
	}

	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);
	return rv;
}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	vb2_error_t rv;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	enter_recovery_screen(ctx, 0);

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
	static int usb_good = -1;

	VB2_TRY(vb2_init_menus(ctx));
	vb2ex_display_ui(VB2_SCREEN_BLANK, 0);

	/* Loop and wait for a recovery image or keyboard inputs */
	VB2_DEBUG("waiting for a recovery image or keyboard inputs\n");
	while(1) {
		rv = VbTryLoadKernel(ctx, VB_DISK_FLAG_REMOVABLE);

		if (rv == VB2_SUCCESS)
			break; /* Found a recovery kernel */

		if (usb_good != (rv == VB2_ERROR_LK_NO_DISK_FOUND)) {
			/* USB state changed, force back to base screen */
			usb_good = (rv == VB2_ERROR_LK_NO_DISK_FOUND);
			if (usb_good)
				enter_recovery_screen(ctx, 0);
			else
				enter_usb_nogood_screen(ctx);
		}

		/* Scan keyboard inputs. */
		uint32_t key, key_flags;
		key = VbExKeyboardReadWithFlags(&key_flags);
		if (key == VB_KEY_CTRL('D') ||
		    (key == VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS &&
		    DETACHABLE)) {
			if (key_flags & VB_KEY_FLAG_TRUSTED_KEYBOARD)
				enter_to_dev_menu(ctx);
			else
				VB2_DEBUG("ERROR: untrusted combo?!\n");
		} else {
			rv = vb2_handle_menu_input(ctx, key,
						   key_flags);
			if (rv != VBERROR_KEEP_LOOPING)
				break;
		}

		VbExSleepMs(KEY_DELAY);

	}

	return rv;
}
