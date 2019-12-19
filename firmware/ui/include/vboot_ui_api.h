/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * UI APIs between calling firmware and vboot_reference.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_API_H_
#define VBOOT_REFERENCE_VBOOT_UI_API_H_

#include "../2lib/include/2return_codes.h"
#include "../2lib/include/2sysincludes.h"

/*****************************************************************************/
/* Delay and beep */

/**
 * Delay for at least the specified number of milliseconds.  Should be accurate
 * to within 10% (a requested delay of 1000 ms should result in an actual delay
 * of between 1000 - 1100 ms).
 */
void VbExSleepMs(uint32_t msec);

/**
 * Play a beep tone of the specified frequency in Hz and duration in msec.
 * This is effectively a VbSleep() variant that makes noise.
 *
 * If the audio codec can run in the background, then:
 *   zero frequency means OFF, non-zero frequency means ON
 *   zero msec means return immediately, non-zero msec means delay (and
 *     then OFF if needed)
 * otherwise,
 *   non-zero msec and non-zero frequency means ON, delay, OFF, return
 *   zero msec or zero frequency means do nothing and return immediately
 *
 * The return value is used by the caller to determine the capabilities. The
 * implementation should always do the best it can if it cannot fully support
 * all features - for example, beeping at a fixed frequency if frequency
 * support is not available.  At a minimum, it must delay for the specified
 * non-zero duration.
 */
vb2_error_t VbExBeep(uint32_t msec, uint32_t frequency);

/*****************************************************************************/
/* Display */

/* Predefined (default) screens for VbExDisplayScreen(). */
enum VbScreenType_t {
	/* Blank (clear) screen */
	VB_SCREEN_BLANK = 0,
	/* Developer - warning */
	VB_SCREEN_DEVELOPER_WARNING = 0x101,
	/* REMOVED: Developer - easter egg (0x102) */
	/* REMOVED: Recovery - remove inserted devices (0x201) */
	/* Recovery - insert recovery image */
	VB_SCREEN_RECOVERY_INSERT   = 0x202,
	/* Recovery - inserted image invalid */
	VB_SCREEN_RECOVERY_NO_GOOD  = 0x203,
	/* Recovery - confirm dev mode */
	VB_SCREEN_RECOVERY_TO_DEV   = 0x204,
	/* Developer - confirm normal mode */
	VB_SCREEN_DEVELOPER_TO_NORM = 0x205,
	/* Please wait - programming EC */
	VB_SCREEN_WAIT              = 0x206,
	/* Confirm after DEVELOPER_TO_NORM */
	VB_SCREEN_TO_NORM_CONFIRMED = 0x207,
	/* Broken screen shown after verification failure */
	VB_SCREEN_OS_BROKEN         = 0x208,
	/* REMOVED: Display base screen (no icons, no text) (0x209) */
	/* Detachable Menu - Developer Warning */
	VB_SCREEN_DEVELOPER_WARNING_MENU = 0x20a,
	/* Detachable Menu - Developer Boot */
	VB_SCREEN_DEVELOPER_MENU = 0x20b,
	/* REMOVED: Detachable Menu - Recovery (0x20c) */
	/* Detachable Menu - Confirm Dev Mode */
	VB_SCREEN_RECOVERY_TO_DEV_MENU = 0x20d,
	/* Detachable Menu - Confirm Normal Mode */
	VB_SCREEN_DEVELOPER_TO_NORM_MENU = 0x20e,
	/* Detachable Menu - Languages */
	VB_SCREEN_LANGUAGES_MENU = 0x20f,
	/* Detachable Menu - Options */
	VB_SCREEN_OPTIONS_MENU = 0x210,
	/* REMOVED: Alt OS picker screen (0x211) */
	/* Alt firmware picker screen (for keyboard UI) */
	VB_SCREEN_ALT_FW_PICK = 0x212,
	/* Alt firmware menu screen (for detachable UI ) */
	VB_SCREEN_ALT_FW_MENU = 0x213,
	/* Set vendor data menu screen */
	VB_SCREEN_SET_VENDOR_DATA = 0x214,
	/* Confirm vendor data menu screen */
	VB_SCREEN_CONFIRM_VENDOR_DATA = 0x215,
	/* Confirm reboot for running diagnostics rom */
	VB_SCREEN_CONFIRM_DIAG = 0x216,
};

/**
 * Extra data needed when displaying vendor data screens
 */
typedef struct VbVendorData
{
	/* Current state of the the vendor data input */
	const char *input_text;
} VbVendorData;

/**
 * Extra data that may be used when displaying a screen
 */
typedef struct VbScreenData
{
	union {
		VbVendorData vendor_data;
	};
} VbScreenData;

/**
 * Display a predefined screen; see VB_SCREEN_* for valid screens.
 *
 * This is a backup method of screen display, intended for use if the GBB does
 * not contain a full set of bitmaps.  It is acceptable for the backup screen
 * to be simple ASCII text such as "NO GOOD" or "INSERT"; these screens should
 * only be seen during development.
 */
vb2_error_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			    const VbScreenData *data);

/**
 * Display a predefined menu screen; see VB_SCREEN_* for valid screens.
 *
 * @param screen_type       ID of screen to draw
 * @param locale            language to display
 * @param selected_index    Index of menu item that is currently selected.
 * @param disabled_idx_mask Bitmap for enabling/disabling certain menu items.
 *                          each bit corresponds to the menu item's index.
 * @param redraw_base       Setting 1 will force a full redraw of the screen
 *
 * @return VB2_SUCCESS or error code on error.
 */
vb2_error_t VbExDisplayMenu(uint32_t screen_type, uint32_t locale,
			  uint32_t selected_index, uint32_t disabled_idx_mask,
			  uint32_t redraw_base);

/**
 * Display a string containing debug information on the screen, rendered in a
 * platform-dependent font.  Should be able to handle newlines '\n' in the
 * string.  Firmware must support displaying at least 20 lines of text, where
 * each line may be at least 80 characters long.  If the firmware has its own
 * debug state, it may display it to the screen below this information if the
 * full_info parameter is set.
 *
 * @param info_str	The debug string to display
 * @param full_info	1 if firmware should append its own info, 0 if not
 *
 * @return VB2_SUCCESS or error code on error.
 */
vb2_error_t VbExDisplayDebugInfo(const char *info_str, int full_info);

/**
 * Write vendor data to read-only VPD
 *
 * @param vendor_data_value   The value of vendor data to write to VPD. The
 *                            string length will be exactly VENDOR_DATA_LENGTH
 *                            characters and null-terminated.
 *
 * @return VB2_SUCCESS or error code on error.
 */
vb2_error_t VbExSetVendorData(const char *vendor_data_value);

/*****************************************************************************/
/* Keyboard and switches */

/* Key code for CTRL + letter */
#define VB_KEY_CTRL(letter) (letter & 0x1f)

/* Key code for fn keys */
#define VB_KEY_F(num) (num + 0x108)

/* Key codes for required non-printable-ASCII characters. */
enum VbKeyCode_t {
	VB_KEY_ENTER = '\r',
	VB_KEY_ESC = 0x1b,
	VB_KEY_BACKSPACE = 0x8,
	VB_KEY_UP = 0x100,
	VB_KEY_DOWN = 0x101,
	VB_KEY_LEFT = 0x102,
	VB_KEY_RIGHT = 0x103,
	VB_KEY_CTRL_ENTER = 0x104,
};

/*
 * WARNING!!! Before updating the codes in enum VbButtonCode_t, ensure that the
 * code does not overlap the values in VbKeyCode_t unless the button action is
 * the same as key action.
 */
enum VbButtonCode_t {
	/* Volume up/down short press match the values in 8042 driver. */
	VB_BUTTON_VOL_UP_SHORT_PRESS = 0x62,
	VB_BUTTON_VOL_DOWN_SHORT_PRESS = 0x63,
	/* Dummy values used below. */
	VB_BUTTON_POWER_SHORT_PRESS = 0x90,
	VB_BUTTON_VOL_UP_LONG_PRESS = 0x91,
	VB_BUTTON_VOL_DOWN_LONG_PRESS = 0x92,
	VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS = 0x93,
};

/* Flags for additional information.
 * TODO(semenzato): consider adding flags for modifiers instead of
 * making up some of the key codes above.
 */
enum VbKeyFlags_t {
	VB_KEY_FLAG_TRUSTED_KEYBOARD = 1 << 0,
};

/**
 * Read the next keypress from the keyboard buffer.
 *
 * Returns the keypress, or zero if no keypress is pending or error.
 *
 * The following keys must be returned as ASCII character codes:
 *    0x08          Backspace
 *    0x09          Tab
 *    0x0D          Enter (carriage return)
 *    0x01 - 0x1A   Ctrl+A - Ctrl+Z (yes, those alias with backspace/tab/enter)
 *    0x1B          Esc (VB_KEY_ESC)
 *    0x20          Space
 *    0x30 - 0x39   '0' - '9'
 *    0x60 - 0x7A   'a' - 'z'
 *
 * Some extended keys must also be supported; see the VB_KEY_* defines above.
 *
 * Keys ('/') or key-chords (Fn+Q) not defined above may be handled in any of
 * the following ways:
 *    1. Filter (don't report anything if one of these keys is pressed).
 *    2. Report as ASCII (if a well-defined ASCII value exists for the key).
 *    3. Report as any other value in the range 0x200 - 0x2FF.
 * It is not permitted to report a key as a multi-byte code (for example,
 * sending an arrow key as the sequence of keys '\x1b', '[', '1', 'A'). */
uint32_t VbExKeyboardRead(void);

/**
 * Same as VbExKeyboardRead(), but return extra information.
 */
uint32_t VbExKeyboardReadWithFlags(uint32_t *flags_ptr);

/**
 * Return the current state of the switches specified in request_mask
 */
uint32_t VbExGetSwitches(uint32_t request_mask);

#endif  /* VBOOT_REFERENCE_VBOOT_UI_API_H_ */
