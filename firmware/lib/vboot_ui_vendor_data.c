/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - user interface for RW firmware
 */

#include "2common.h"
#include "2nvstorage.h"
#include "2sysincludes.h"
#include "vboot_api.h"
#include "vboot_display.h"
#include "vboot_ui_common.h"
#include "vboot_ui_vendor_data.h"

static inline int is_vowel(uint32_t key) {
	return key == 'A' || key == 'E' || key == 'I' ||
	       key == 'O' || key == 'U';
}

/*
 * Prompt the user to enter the vendor data
 */
static vb2_error_t vb2_enter_vendor_data_ui(struct vb2_context *ctx,
					    char *data_value)
{
	int len = 0;
	VbScreenData data = {
		.vendor_data = { data_value }
	};

	data_value[0] = '\0';
	VbDisplayScreen(ctx, VB_SCREEN_SET_VENDOR_DATA, 1, &data);

	/* We'll loop until the user decides what to do */
	do {
		uint32_t key = VbExKeyboardRead();

		if (VbWantShutdown(ctx, key)) {
			VB2_DEBUG("Vendor Data UI - shutdown requested!\n");
			return VBERROR_SHUTDOWN_REQUESTED;
		}
		switch (key) {
		case 0:
			/* nothing pressed */
			break;
		case VB_KEY_ESC:
			/* Escape pressed - return to developer screen */
			VB2_DEBUG("Vendor Data UI - user pressed Esc: "
				  "exit to Developer screen\n");
			data_value[0] = '\0';
			return VB2_SUCCESS;
		case 'a'...'z':
			key = toupper(key);
			VBOOT_FALLTHROUGH;
		case '0'...'9':
		case 'A'...'Z':
			if ((len > 0 && is_vowel(key)) ||
			     len >= VENDOR_DATA_LENGTH) {
				vb2_error_beep(VB_BEEP_NOT_ALLOWED);
			} else {
				data_value[len++] = key;
				data_value[len] = '\0';
				VbDisplayScreen(ctx, VB_SCREEN_SET_VENDOR_DATA,
						1, &data);
			}

			VB2_DEBUG("Vendor Data UI - vendor_data: %s\n",
				  data_value);
			break;
		case VB_KEY_BACKSPACE:
			if (len > 0) {
				data_value[--len] = '\0';
				VbDisplayScreen(ctx, VB_SCREEN_SET_VENDOR_DATA,
						1, &data);
			}

			VB2_DEBUG("Vendor Data UI - vendor_data: %s\n",
				  data_value);
			break;
		case VB_KEY_ENTER:
			if (len == VENDOR_DATA_LENGTH) {
				/* Enter pressed - confirm input */
				VB2_DEBUG("Vendor Data UI - user pressed "
					  "Enter: confirm vendor data\n");
				return VB2_SUCCESS;
			} else {
				vb2_error_beep(VB_BEEP_NOT_ALLOWED);
			}
			break;
		default:
			VB2_DEBUG("Vendor Data UI - pressed key %#x\n", key);
			VbCheckDisplayKey(ctx, key, &data);
			break;
		}
		VbExSleepMs(KEY_DELAY_MS);
	} while (1);

	return VB2_SUCCESS;
}

/*
 * User interface for setting the vendor data in VPD
 */
vb2_error_t vb2_vendor_data_ui(struct vb2_context *ctx)
{
	char data_value[VENDOR_DATA_LENGTH + 1];
	VbScreenData data = {
		.vendor_data = { data_value }
	};

	vb2_error_t ret = vb2_enter_vendor_data_ui(ctx, data_value);

	if (ret)
		return ret;

	/* Vendor data was not entered just return */
	if (data_value[0] == '\0')
		return VB2_SUCCESS;

	VbDisplayScreen(ctx, VB_SCREEN_CONFIRM_VENDOR_DATA, 1, &data);
	/* We'll loop until the user decides what to do */
	do {
		uint32_t key = VbExKeyboardRead();

		if (VbWantShutdown(ctx, key)) {
			VB2_DEBUG("Vendor Data UI - shutdown requested!\n");
			return VBERROR_SHUTDOWN_REQUESTED;
		}
		switch (key) {
		case 0:
			/* nothing pressed */
			break;
		case VB_KEY_ESC:
			/* Escape pressed - return to developer screen */
			VB2_DEBUG("Vendor Data UI - user pressed Esc: "
				  "exit to Developer screen\n");
			return VB2_SUCCESS;
		case VB_KEY_ENTER:
			/* Enter pressed - write vendor data */
			VB2_DEBUG("Vendor Data UI - user pressed Enter: "
				  "write vendor data (%s) to VPD\n",
				  data_value);
			ret = VbExSetVendorData(data_value);

			if (ret == VB2_SUCCESS) {
				vb2_nv_set(ctx, VB2_NV_DISABLE_DEV_REQUEST, 1);
				return VBERROR_REBOOT_REQUIRED;
			} else {
				vb2_error_notify(
					"ERROR: Vendor data was not set.\n"
					"System will now shutdown\n",
					NULL,
					VB_BEEP_FAILED);
				VbExSleepMs(5000);
				return VBERROR_SHUTDOWN_REQUESTED;
			}
		default:
			VB2_DEBUG("Vendor Data UI - pressed key %#x\n", key);
			VbCheckDisplayKey(ctx, key, &data);
			break;
		}
		VbExSleepMs(KEY_DELAY_MS);
	} while (1);

	return VB2_SUCCESS;
}