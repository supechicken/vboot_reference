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
#include "vboot_api.h"
#include "vboot_kernel.h"

/* Mock data */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;

static enum vb2_screen mock_screens_displayed[64];
static uint32_t mock_locales_displayed[64];
static uint32_t mock_screens_count = 0;

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

static int mock_ec_trusted;

static int mock_shutdown_request_left;
static const uint32_t mock_shutdown_request_fixed = 0xff;
static uint32_t mock_shutdown_request;

static int mock_virtdev_set;
static int mock_virtdev_allowed;

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
	FOR_UTILITIES,
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

	/* for VbExKeyboardRead */
	memset(mock_keypress, 0, sizeof(mock_keypress));
	memset(mock_keyflags, 0, sizeof(mock_keyflags));
	mock_keypress_count = 0;
	mock_keypress_total = 0;

	/* for vb2ex_display_ui */
	memset(mock_screens_displayed, 0, sizeof(mock_screens_displayed));
	mock_screens_count = 0;

	/* for dev_boot* in 2misc.h */
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_DISK;
	mock_dev_boot_allowed = 1;
	mock_dev_boot_legacy_allowed = 0;
	mock_dev_boot_usb_allowed = 0;

	/* for VbExLegacy */
	mock_vbexlegacy_called = 0;
	mock_altfw_num = -100;

	/* for VbTryLoadKernel */
	memset(mock_vbtlk_retval, 0, sizeof(mock_vbtlk_retval));
	memset(mock_vbtlk_expected_flag, 0, sizeof(mock_vbtlk_expected_flag));
	mock_vbtlk_count = 0;
	mock_vbtlk_total = 0;

	/* for shutdown_requested */
	power_button_state = POWER_BUTTON_HELD_SINCE_BOOT;
	if (t == FOR_DEVELOPER)
		mock_shutdown_request_left = -1;  /* Never request shutdown */
	else
		mock_shutdown_request_left = 301;
	if (t != FOR_UTILITIES)
		mock_shutdown_request = mock_shutdown_request_fixed;

	/* for vb2_allow_recovery */
	sd->flags |= VB2_SD_FLAG_MANUAL_RECOVERY;
	if (t == FOR_RECOVERY)
		mock_ec_trusted = 1;
	else
		mock_ec_trusted = 0;

	/* for vb2_enable_developer_mode */
	mock_virtdev_set = 0;
	mock_virtdev_allowed = 1;
}

/* Mock functions */

struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
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

vb2_error_t vb2ex_display_ui(enum vb2_screen screen,
			     uint32_t locale_id,
			     uint32_t selected_item,
			     uint32_t disabled_item_mask)
{
	VB2_DEBUG("screens %d: screen = %#x, locale_id = %u\n",
		  mock_screens_count, screen, locale_id);

	if (mock_screens_count >= ARRAY_SIZE(mock_screens_displayed) ||
	    mock_screens_count >= ARRAY_SIZE(mock_locales_displayed)) {
		TEST_TRUE(0, "Test failed as mock vb2ex_display_ui ran out of"
			  " entries!");
		return VB2_ERROR_MOCK;
	}

	mock_screens_displayed[mock_screens_count] = screen;
	mock_locales_displayed[mock_screens_count] = locale_id;
	/* TODO(roccochen): handle the rest of two arguments */
	mock_screens_count++;

	return VB2_SUCCESS;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_shutdown_request != mock_shutdown_request_fixed)
		return mock_shutdown_request;  /* pre-specified */

	if (mock_shutdown_request_left == 0)
		return 1;
	else if (mock_shutdown_request_left > 0)
		mock_shutdown_request_left--;

	return 0;
}

int vb2ex_ec_trusted(void)
{
	return mock_ec_trusted;
}

/* TODO: change prototype after CL:2152212 */
vb2_error_t vb2_enable_developer_mode(struct vb2_context *c)
{
	if (!mock_virtdev_allowed)
		VB2_DIE("vb2_enable_developer_mode failed");

	mock_virtdev_set = 1;

	return VB2_SUCCESS;
}

/* Tests */

static void utilities_tests(void)
{
	VB2_DEBUG("Testing shutdown_requested...\n");

	/* Release, press, hold, and release */
	reset_common_data(FOR_UTILITIES);
	mock_shutdown_request = 0;
	TEST_EQ(shutdown_requested(ctx, 0), 0,
		"release, press, hold, and release");
	TEST_EQ(power_button_state, POWER_BUTTON_RELEASED, "  state: released");
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	TEST_EQ(shutdown_requested(ctx, 0), 0, "  press");
	TEST_EQ(power_button_state, POWER_BUTTON_PRESSED, "  state: pressed");
	TEST_EQ(shutdown_requested(ctx, 0), 0, "  hold");
	TEST_EQ(power_button_state, POWER_BUTTON_PRESSED, "  state: pressed");
	mock_shutdown_request = 0;
	TEST_EQ(shutdown_requested(ctx, 0), 1, "  release");
	TEST_EQ(power_button_state, POWER_BUTTON_RELEASED, "  state: released");

	/* Press is ignored because we may held since boot */
	reset_common_data(FOR_UTILITIES);
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	TEST_EQ(shutdown_requested(ctx, 0), 0, "press is ignored");
	TEST_NEQ(power_button_state, POWER_BUTTON_PRESSED,
		 "  state is not pressed");

	/* Power button short press from key */
	reset_common_data(FOR_UTILITIES);
	mock_shutdown_request = 0;
	TEST_EQ(shutdown_requested(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
		"power button short press");

	/* Lid closure = shutdown request anyway */
	reset_common_data(FOR_UTILITIES);
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_requested(ctx, 0), 1, "lid closure");
	TEST_EQ(shutdown_requested(ctx, 'A'), 1, "  lidsw + random key");
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED |
				VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	TEST_EQ(shutdown_requested(ctx, 0), 1, "  lidsw + pwdsw");
	TEST_EQ(shutdown_requested(ctx, 0), 1, "  state does not affect");

	/* Lid ignored by GBB flags */
	reset_common_data(FOR_UTILITIES);
	gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_requested(ctx, 0), 0, "lid ignored");
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED |
				VB_SHUTDOWN_REQUEST_POWER_BUTTON;
	TEST_EQ(shutdown_requested(ctx, 0), 0, "  lidsw + pwdsw");
	TEST_EQ(power_button_state, POWER_BUTTON_PRESSED, "  state: pressed");
	mock_shutdown_request = 0;
	TEST_EQ(shutdown_requested(ctx, 0), 1, "  pwdsw release");
	TEST_EQ(power_button_state, POWER_BUTTON_RELEASED, "  state: released");

	/* Lid ignored; power button short pressed */
	reset_common_data(FOR_UTILITIES);
	gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_requested(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
		"lid ignored; power button short pressed");

	/* DETACHABLE ignore power button */
	if (DETACHABLE) {
		/* pwdsw */
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_requested(ctx, 0), 0,
			"DETACHABLE: ignore pwdsw");
		TEST_EQ(power_button_state, POWER_BUTTON_PRESSED,
			"  state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_requested(ctx, 0), 0,
			"  ignore on release");
		TEST_EQ(power_button_state, POWER_BUTTON_RELEASED,
			"  state: released");

		/* power button short press */
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_requested(
		    ctx, VB_BUTTON_POWER_SHORT_PRESS), 0,
		    "DETACHABLE: ignore power button short press");

	}

	VB2_DEBUG("...done.\n");
}

static void developer_tests(void)
{
	VB2_DEBUG("Testing developer mode...\n");

	/* Proceed */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
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
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default legacy not enabled");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to USB");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default USB not enabled");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
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
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	/* BROKEN screen with disks inserted */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with disks");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	/* BROKEN screen with disks on second attempt */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with later disk");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	/* BROKEN screen even if dev switch is on */
	reset_common_data(FOR_BROKEN);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_dev_boot_allowed = 1;
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shutdown requested in BROKEN with dev switch");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

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
	add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
	TEST_EQ(vb2_broken_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Shortcuts ignored in BROKEN");
	TEST_EQ(mock_virtdev_set, 0, "  virtual dev mode off");
	TEST_NEQ(mock_shutdown_request_left, 0, "  powered down explicitly");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	VB2_DEBUG("...done.\n");
}

static void manual_recovery_tests(void)
{
	VB2_DEBUG("Testing manual recovery mode...\n");

	/* Stay at BROKEN if recovery button not physically pressed */
	/* Sanity check, should never happen. */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	sd->flags &= ~VB2_SD_FLAG_MANUAL_RECOVERY;
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Go to BROKEN if recovery not manually requested");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	/* Stay at BROKEN if EC is untrusted */
	/* Sanity check, should never happen. */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	mock_ec_trusted = 0;
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Go to BROKEN if EC is not trusted");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_OS_BROKEN,
		"  broken screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

	/* INSERT boots without screens if we have a valid image on first try */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"INSERT boots without screens if valid on first try");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_virtdev_set, 0, "  virtual dev mode off");
	TEST_EQ(mock_screens_count, 0, "  no screen");

	/* INSERT boots eventually if we get a valid image later */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"INSERT boots after valid image appears");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_virtdev_set, 0, "  virtual dev mode off");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_SELECT,
		"  recovery base");
	TEST_EQ(mock_screens_count, 1, "  no extra screen");

	/* invalid image, then remove, then valid image */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"INSERT boots after valid image appears");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_virtdev_set, 0, "  virtual dev mode off");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_NO_GOOD,
		"  nogood screen");
	TEST_EQ(mock_screens_displayed[1], VB2_SCREEN_RECOVERY_SELECT,
		"  recovery base");
	TEST_EQ(mock_screens_count, 2, "  no extra screens");

	/* Navigate to confirm dev mode selection and then cancel */
	/* TODO: TO_DEV_MENU confirmation */

	/* Navigate to confirm dev mode selection and then confirm */
	/* TODO: TO_DEV_MENU confirmation */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_key(VB_KEY_CTRL('D'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_REBOOT_REQUIRED,
		"go to to_dev screen and confirm");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_virtdev_set, 1, "  virtual dev mode on");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_SELECT,
		"  recovery base");
	TEST_EQ(mock_screens_displayed[1], VB2_SCREEN_RECOVERY_TO_DEV,
		"  recovery to_dev");
	TEST_EQ(mock_screens_count, 2, "  no extra screens");

	/* DETACHABLE Navigate to confirm dev mode selection and then confirm */
	/* TODO: TO_DEV_MENU */
	if (DETACHABLE) {
		reset_common_data(FOR_RECOVERY);
		add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND,
			       VB_DISK_FLAG_REMOVABLE);
		add_mock_key(VB_BUTTON_VOL_UP_DOWN_COMBO_PRESS,
			     VB_KEY_FLAG_TRUSTED_KEYBOARD);
		TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_REBOOT_REQUIRED,
			"DETACHABLE volume-up-down long press");
		TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
			"  no recovery");
		TEST_EQ(mock_virtdev_set, 1, "  virtual dev mode on");
		TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_SELECT,
			"  recovery base");
		TEST_EQ(mock_screens_displayed[1], VB2_SCREEN_RECOVERY_TO_DEV,
			"  recovery to_dev");
		TEST_EQ(mock_screens_count, 2, "  no extra screens");
	}

	/* Untrusted keyboard cannot enter TO_DEV (must be malicious anyway) */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_keypress(VB_KEY_CTRL('D'));  /* try to_dev */
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Untrusted keyboard cannot enter TO_DEV");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_shutdown_request_left, 0, "  timed out");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_SELECT,
		"  recovery base");
	TEST_EQ(mock_screens_count, 1, "  no extra screen");

	/* Untrusted keyboard cannot navigate in TO_DEV menu if already there */
	/* TODO: TO_DEV_MENU confirmation */

	/* Handle TPM error in enabling dev mode */
	/* TODO: TO_DEV_MENU confirmation */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_key(VB_KEY_CTRL('D'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	mock_virtdev_allowed = 0;
	TEST_ABORT(vb2_manual_recovery_menu(ctx), "to_dev TPM failure");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_virtdev_set, 0, "  set virtual dev failed");

	/* Cannot enable dev mode if already enabled. */
	reset_common_data(FOR_RECOVERY);
	add_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	add_mock_key(VB_KEY_CTRL('D'), VB_KEY_FLAG_TRUSTED_KEYBOARD);
	sd->flags |= VB2_SD_FLAG_DEV_MODE_ENABLED;
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"Ctrl+D ignored if already in dev mode");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(mock_shutdown_request_left, 0, "  timed out");
	TEST_EQ(mock_virtdev_set, 0, "  virtual dev mode wasn't enabled again");
	/* TODO: flash screen */
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_RECOVERY_SELECT,
		"  recovery base");
	TEST_EQ(mock_screens_count, 1, "  no extra screen");
	/* TODO: beeps */

	VB2_DEBUG("...done.\n");
}

int main(void)
{
	utilities_tests();
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
