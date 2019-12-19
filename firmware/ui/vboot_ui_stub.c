/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stub implementations of firmware-provided UI API functions.
 */

#include "vboot_ui_api.h"

void VbExSleepMs(uint32_t msec)
{
}

vb2_error_t VbExBeep(uint32_t msec, uint32_t frequency)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			      const VbScreenData *data)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExDisplayMenu(uint32_t screen_type, uint32_t locale,
			    uint32_t selected_index, uint32_t disabled_idx_mask,
			    uint32_t redraw_base)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExDisplayDebugInfo(const char *info_str, int full_info)
{
	return VB2_SUCCESS;
}

uint32_t VbExKeyboardRead(void)
{
	return 0;
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *flags_ptr)
{
	return 0;
}

uint32_t VbExGetSwitches(uint32_t mask)
{
	return 0;
}

vb2_error_t VbExSetVendorData(const char *vendor_data_value)
{
	return 0;
}
