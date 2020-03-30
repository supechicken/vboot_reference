/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Private declarations for 2ui.c. Defined here for easier testing.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_
#define VBOOT_REFERENCE_VBOOT_UI_MENU_PRIVATE_H_

#include "2api.h"
#include "vboot_api.h"

enum power_button_state_type {
	POWER_BUTTON_HELD_SINCE_BOOT = 0,
	POWER_BUTTON_RELEASED,
	POWER_BUTTON_PRESSED,  /* Must have been previously released */
};

extern enum power_button_state_type power_button_state;

extern int shutdown_requested(struct vb2_context *ctx, uint32_t key);

/* Context of each select item */
struct vb2_menu_item {
	const char *text;
	vb2_error_t (*action)(struct vb2_context *ctx);
};

struct vb2_menu {
	const char *name;
	uint16_t size;
	enum vb2_screen screen;
	struct vb2_menu_item *items;
};

/*
 * Quick index to menu master table.
 *
 * This enumeration is the index to vb2_menu master table.
 * It has a one-to-one relationship with VB2_SCREEN, where VB2_MENU_{*} should
 * maps to VB2_SCREEN_{*} and VB2_MENU_COUNT is always the enumeration size.
 */
typedef enum _VB2_MENU {
	VB2_MENU_BLANK,
	VB2_MENU_FIRMWARE_SYNC,
	VB2_MENU_COUNT,
} VB2_MENU;

/* The enumerations below are going to deprecated */
typedef enum _VB_GROOT {
	VB_GROOT_DEV_WARNING,
	VB_GROOT_DEV,
	VB_GROOT_TO_NORM,
	VB_GROOT_TO_DEV,
	VB_GROOT_LANGUAGES,
	VB_GROOT_ADV_OPTIONS,
	VB_GROOT_DEBUG_INFO,
	VB_GROOT_RECOVERY_STEP0,
	VB_GROOT_RECOVERY_STEP1,
	VB_GROOT_RECOVERY_STEP2,
	VB_GROOT_RECOVERY_STEP3,
	VB_GROOT_RECOVERY_NO_GOOD,
	VB_GROOT_RECOVERY_BROKEN,
	VB_GROOT_TO_NORM_CONFIRMED,
	VB_GROOT_BOOT_FROM_INTERNAL,
	VB_GROOT_BOOT_FROM_EXTERNAL,
	VB_GROOT_ALT_FW,
	VB_GROOT_SHOW_LOG,
	VB_GROOT_COUNT,
} VB_GROOT;

typedef enum _VB_DEV_WARNING_GROOT {
	VB_GROOT_WARN_LANGUAGE,
	VB_GROOT_WARN_ENABLE_VER,
	VB_GROOT_WARN_DISK,
	VB_GROOT_WARN_USB,
	VB_GROOT_WARN_LEGACY,
	VB_GROOT_WARN_DBG_INFO,
	VB_GROOT_WARN_COUNT,
} VB_DEV_WARNING_GROOT;

#if 0
typedef enum _VB_DEV_GROOT {
	VB_DEV_NETWORK,
	VB_DEV_LEGACY,
	VB_DEV_USB,
	VB_DEV_DISK,
	VB_DEV_CANCEL,
	VB_DEV_LANGUAGE,
	VB_DEV_COUNT,
} VB_DEV_GROOT;
#endif
typedef enum _VB_TO_NORM_GROOT {
	VB_GROOT_TO_NORM_CONFIRM,
	VB_GROOT_TO_NORM_CANCEL,
	VB_GROOT_TO_NORM_COUNT,
} VB_TO_NORM_GROOT;

typedef enum _VB_TO_DEV_GROOT {
	VB_GROOT_TO_DEV_CONFIRM,
	VB_GROOT_TO_DEV_CANCEL,
	VB_GROOT_TO_DEV_COUNT,
} VB_TO_DEV_GROOT;

// recovery insert screen
typedef enum _VB_RECOVERY_GROOT {
	VB_GROOT_REC_LANGUAGE,
	VB_GROOT_REC_BEGIN,
	VB_GROOT_REC_ADV_OPTIONS,
	VB_GROOT_REC_COUNT,
} VB_REC_GROOT;

typedef enum _VB_RECOVERY_GROOT_STEP0 {
	VB_GROOT_REC_STEP0_LANGUAGE,
	VB_GROOT_REC_STEP0_NEXT,
	VB_GROOT_REC_STEP0_ADV_OPTIONS,
	VB_GROOT_REC_STEP0_COUNT,
} VB_REC_GROOT_STEP0;

typedef enum _VB_RECOVERY_GROOT_STEP1 {
	VB_GROOT_REC_STEP1_LANGUAGE,
	VB_GROOT_REC_STEP1_NEXT,
	VB_GROOT_REC_STEP1_BACK,
	VB_GROOT_REC_STEP1_COUNT,
} VB_REC_GROOT_STEP1;

typedef enum _VB_RECOVERY_GROOT_STEP2 {
	VB_GROOT_REC_STEP2_LANGUAGE,
	VB_GROOT_REC_STEP2_NEXT,
	VB_GROOT_REC_STEP2_BACK,
	VB_GROOT_REC_STEP2_COUNT,
} VB_REC_GROOT_STEP2;

typedef enum _VB_RECOVERY_GROOT_STEP3 {
	VB_GROOT_REC_STEP3_LANGUAGE,
	VB_GROOT_REC_STEP3_BACK,
	VB_GROOT_REC_STEP3_COUNT,
} VB_REC_GROOT_STEP3;

typedef enum _VB_BOOT_USB_GROOT {
	VB_GROOT_BOOT_USB_BACK,
	VB_GROOT_BOOT_USB_COUNT,
} VB_BOOT_USB_GROOT;

// recovery advanced options menu
typedef enum _VB_OPTIONS_ADV {
	VB_GROOT_OPTIONS_TO_DEV,
	VB_GROOT_OPTIONS_DBG_INFO,
	VB_GROOT_OPTIONS_BIOS_LOG,
	VB_GROOT_OPTIONS_CANCEL,
	VB_GROOT_OPTIONS_COUNT,
} VB_OPTIONS_ADV;

typedef enum _VB_DEBUG_INFO {
	VB_GROOT_DEBUG_PAGE_UP,
	VB_GROOT_DEBUG_PAGE_DOWN,
	VB_GROOT_DEBUG_BACK,
	VB_GROOT_DEBUG_COUNT,
} VB_DEBUG_INFO;

typedef enum _VB_RECOVERY_BROKEN {
	VB_GROOT_REC_BROKEN_LANGUAGE,
	VB_GROOT_REC_BROKEN_ADV_OPTIONS,
} VB_RECOVERY_BROKEN;

typedef enum _VB_LOG {
	VB_GROOT_LOG_PAGE_UP,
	VB_GROOT_LOG_PAGE_DOWN,
	VB_GROOT_LOG_BACK,
	VB_GROOT_LOG_COUNT,
} VB_LOG;

#endif
