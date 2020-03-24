/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for developer and recovery mode UIs.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2ui.h"
#include "2ui_private.h"
#include "test_common.h"
#include "vb2_ui_test_common.h"
#include "vboot_kernel.h"

/* Mock data */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;

static int mock_shutdown_request_left;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;

static enum vb2_dev_default_boot mock_default_boot;
static int mock_dev_boot_allowed;
static int mock_dev_boot_legacy_allowed;
static int mock_dev_boot_usb_allowed;

static int mock_vbexlegacy_called;
static enum VbAltFwIndex_t mock_altfw_num;

static vb2_error_t mock_vbtlk_retval[32];
static uint32_t mock_vbtlk_expected_flag[32];
static int mock_vbtlk_count;
static int mock_vbtlk_total;

static void add_mock_key(uint32_t press, uint32_t flags)
{
	if (mock_keypress_total >= ARRAY_SIZE(mock_keypress) ||
	    mock_keypress_total >= ARRAY_SIZE(mock_keyflags)) {
		TEST_TRUE(0, "Test failed as mock_key ran out of entries!");
		return;
	}

	mock_keypress[mock_keypress_total] = press;
	mock_keyflags[mock_keypress_total] = flags;
	mock_keypress_total++;
}

static void add_mock_keypress(uint32_t press)
{
	add_mock_key(press, 0);
}

static void add_mock_vbtlk(vb2_error_t retval, uint32_t get_info_flags)
{
	if (mock_vbtlk_total >= ARRAY_SIZE(mock_vbtlk_retval) ||
	    mock_vbtlk_total >= ARRAY_SIZE(mock_vbtlk_expected_flag)) {
		TEST_TRUE(0, "Test failed as mock_vbtlk ran out of entries!");
		return;
	}

	mock_vbtlk_retval[mock_vbtlk_total] = retval;
	mock_vbtlk_expected_flag[mock_vbtlk_total] = get_info_flags;
	mock_vbtlk_total++;
}

/* Type of test to reset for */
enum reset_type {
	FOR_DEVELOPER,
	FOR_BROKEN,
	FOR_RECOVERY,
};

/* Reset mock data (for use before each test) */
static void reset_common_data(enum reset_type t)
{
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	memset(&gbb, 0, sizeof(gbb));

	vb2_nv_init(ctx);

	sd = vb2_get_sd(ctx);

	/* For common data in vb2_ui_test_common.h */
	reset_ui_common_data();

	/* For shutdown_required */
	if (t == FOR_DEVELOPER)
		mock_shutdown_request_left = -1;  /* Never request shutdown */
	else
		mock_shutdown_request_left = 301;

	/* For VbExKeyboardRead */
	memset(mock_keypress, 0, sizeof(mock_keypress));
	memset(mock_keyflags, 0, sizeof(mock_keyflags));
	mock_keypress_count = 0;
	mock_keypress_total = 0;

	/* For dev_boot* in 2misc.h */
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_DISK;
	mock_dev_boot_allowed = 1;
	mock_dev_boot_legacy_allowed = 0;
	mock_dev_boot_usb_allowed = 0;

	/* For VbExLegacy */
	mock_vbexlegacy_called = 0;
	mock_altfw_num = -100;

	/* For VbTryLoadKernel */
	memset(mock_vbtlk_retval, 0, sizeof(mock_vbtlk_retval));
	memset(mock_vbtlk_expected_flag, 0, sizeof(mock_vbtlk_expected_flag));
	mock_vbtlk_count = 0;
	mock_vbtlk_total = 0;
}

/* Mock functions */
struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_shutdown_request_left == 0)
		return 1;
	else if (mock_shutdown_request_left > 0)
		mock_shutdown_request_left--;

	return 0;
}

uint32_t VbExKeyboardRead(void)
{
	return VbExKeyboardReadWithFlags(NULL);
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *key_flags)
{
	if (mock_keypress_count < mock_keypress_total) {
		if (key_flags != NULL)
			*key_flags = mock_keyflags[mock_keypress_count];
		return mock_keypress[mock_keypress_count++];
	}

	return 0;
}

enum vb2_dev_default_boot vb2_get_dev_boot_target(struct vb2_context *c)
{
	return mock_default_boot;
}

int vb2_dev_boot_allowed(struct vb2_context *c)
{
	return mock_dev_boot_allowed;
}

int vb2_dev_boot_legacy_allowed(struct vb2_context *c)
{

	return mock_dev_boot_legacy_allowed;
}

int vb2_dev_boot_usb_allowed(struct vb2_context *c)
{
	return mock_dev_boot_usb_allowed;
}

vb2_error_t VbExLegacy(enum VbAltFwIndex_t altfw_num)
{
	mock_vbexlegacy_called++;
	mock_altfw_num = altfw_num;

	return VB2_SUCCESS;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	/* Return last entry if called too many times */
	if (mock_vbtlk_count >= mock_vbtlk_total)
		mock_vbtlk_count = mock_vbtlk_total - 1;

	if (mock_vbtlk_expected_flag[mock_vbtlk_count] != get_info_flags)
		return VB2_ERROR_MOCK;

	return mock_vbtlk_retval[mock_vbtlk_count++];
}

/* Tests */
static void developer_tests(void)
{
	VB2_DEBUG("Testing developer mode...\n");

	/* Proceed */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed");
	displayed_no_extra();
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy */
	reset_common_data(FOR_DEVELOPER);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to legacy");
	TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
	TEST_EQ(mock_altfw_num, 0, "  check altfw_num");
	displayed_no_extra();
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default legacy not enabled");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	displayed_no_extra();
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to USB");
	displayed_no_extra();
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default USB not enabled");
	displayed_no_extra();
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	VB2_DEBUG("...done.\n");
}

static void broken_recovery_tests(void)
{
	VB2_DEBUG("Testing broken recovery mode...\n");

	/* Shutdown requested in BROKEN */
	reset_common_data(FOR_BROKEN);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* BROKEN screen shutdown request */
	if (!DETACHABLE) {
		reset_common_data(FOR_BROKEN);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		TEST_EQ(vb2_broken_recovery_menu(ctx),
			VBERROR_SHUTDOWN_REQUESTED,
			"power button short pressed = shutdown");
		displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
			     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
		displayed_no_extra();
	}

	/* BROKEN screen with disks inserted */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with disks");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* BROKEN screen with disks on second attempt */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with later disk");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* BROKEN screen even if dev switch is on */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_dev_boot_allowed = 1;
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with dev switch");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* Shortcuts that are always ignored in BROKEN */
	reset_common_data(FOR_BROKEN);
	add_mock_key(VB_KEY_CTRL('D'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	add_mock_key(VB_KEY_CTRL('U'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	add_mock_key(VB_KEY_CTRL('L'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	add_mock_key(VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS,
		     VB_KEY_FLAG_TRUSTED_KEYBOARD);
	add_mock_key(VB_BUTTON_VOL_UP_LONG_PRESS,
		     VB_KEY_FLAG_TRUSTED_KEYBOARD);
	add_mock_key(VB_BUTTON_VOL_DOWN_LONG_PRESS,
		     VB_KEY_FLAG_TRUSTED_KEYBOARD);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shortcuts ignored in BROKEN");
	TEST_EQ(mock_shutdown_request_left, 0, "  ignore all");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("broken screen", VB2_SCREEN_RECOVERY_BROKEN,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	VB2_DEBUG("...done.\n");
}

static void manual_recovery_tests(void)
{
	VB2_DEBUG("Testing manual recovery mode...\n");

	/* Timeout, shutdown */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"timeout, shutdown");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* Power button short pressed = shutdown request */
	if (!DETACHABLE) {
		reset_common_data(FOR_RECOVERY);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND,
			       VB_DISK_FLAG_REMOVABLE);
		TEST_EQ(vb2_manual_recovery_menu(ctx),
			VBERROR_SHUTDOWN_REQUESTED,
			"power button short pressed = shutdown");
		TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
			"  no recovery");
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
		displayed_no_extra();
	}

	/* Item 1 = phone recovery */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_ENTER);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"phone recovery");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 0, MOCK_FIXED);
	displayed_eq("phone recovery", VB2_SCREEN_RECOVERY_PHONE_STEP1,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* Item 2 = external disk recovery */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_ENTER);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"external disk recovery");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 0, MOCK_FIXED);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 1, MOCK_FIXED);
	displayed_eq("disk recovery", VB2_SCREEN_RECOVERY_DISK_STEP1,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* KEY_UP should not exceed boundary */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"KEY_UP should not out-of-bound");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 0, MOCK_FIXED);
	displayed_no_extra();

	/* KEY_DOWN should not exceed boundary, either */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"neither does KEY_DOWN");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 0, MOCK_FIXED);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, 1, MOCK_FIXED);
	displayed_no_extra();

	/* For DETACHABLE */
	if (DETACHABLE) {
		reset_common_data(FOR_RECOVERY);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND,
			       VB_DISK_FLAG_REMOVABLE);
		TEST_EQ(vb2_manual_recovery_menu(ctx),
			VBERROR_SHUTDOWN_REQUESTED, "DETACHABLE");
		TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
			"  no recovery");
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     MOCK_FIXED, 0, MOCK_FIXED);
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     MOCK_FIXED, 1, MOCK_FIXED);
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     MOCK_FIXED, 0, MOCK_FIXED);
		displayed_eq("phone recovery", VB2_SCREEN_RECOVERY_PHONE_STEP1,
			     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
		displayed_no_extra();
	}

	/* Boots if we have a valid image on first try */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"boots if valid on first try");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* Boots eventually if we get a valid image later */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"boots after valid image appears");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();

	/* Invalid image, then remove, then valid image */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"boots after valid image appears");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_INVALID,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     MOCK_FIXED, MOCK_FIXED, MOCK_FIXED);
	displayed_no_extra();



	VB2_DEBUG("...done.\n");
}

int main(void)
{
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
