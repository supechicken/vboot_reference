/* Copyright 2022 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Helper functions to retrieve vboot firmware information.
 */

#ifndef VBOOT_REFERENCE_2INFO_H_
#define VBOOT_REFERENCE_2INFO_H_

#include "2api.h"

/**
 * Convert Firmware Boot Mode into supported string
 *
 * @return char*   firmware boot mode string
 */
static inline const char *vb2api_boot_mode_string(uint8_t boot_mode)
{
	switch ((enum vb2_boot_mode)boot_mode) {
	/* 0x00 */ case VB2_BOOT_MODE_UNDEFINED:
		return "Undefined";
	/* 0x01 */ case VB2_BOOT_MODE_MANUAL_RECOVERY:
		return "Manual recovery";
	/* 0x02 */ case VB2_BOOT_MODE_BROKEN_SCREEN:
		return "Broken screen";
	/* 0x03 */ case VB2_BOOT_MODE_DIAGNOSTICS:
		return "Diagnostic";
	/* 0x04 */ case VB2_BOOT_MODE_DEVELOPER:
		return "Developer";
	/* 0x05 */ case VB2_BOOT_MODE_NORMAL:
		return "Secure";
	}

	return "Unknown";
}

/**
 * Convert Firmware Slot result into supported string
 *
 * @return char*   firmware slot result string
 */
static inline const char *vb2api_result_string(uint8_t result)
{
	switch ((enum vb2_fw_result)result) {
	/* 0x00 */ case VB2_FW_RESULT_UNKNOWN:
		return "Unknown";
	/* 0x01 */ case VB2_FW_RESULT_TRYING:
		return "Trying";
	/* 0x02 */ case VB2_FW_RESULT_SUCCESS:
		return "Success";
	/* 0x03 */ case VB2_FW_RESULT_FAILURE:
		return "Failure";
	}

	return "Unknown";
}

/**
 * Convert Firmware Slot into supported string
 *
 * @return char*   firmware slot name string
 */
static inline const char *vb2api_slot_string(uint8_t slot)
{
	if ((enum vb2_fw_slot)slot == VB2_FW_SLOT_A)
	/* 0x00 */ return "A";
	else
	/* 0x01 */ return "B";
}

#endif  /* VBOOT_REFERENCE_2INFO_H_ */
