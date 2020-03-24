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
static const uint32_t mock_fixed = 0xffff;

struct mock_displayed_t {
	enum vb2_screen screen;
	uint32_t locale_id;
	uint32_t selected_item;
	uint32_t disabled_item_mask;
};

static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;

static struct mock_displayed_t mock_displayed[64];
static uint32_t mock_displayed_count = 0;
static uint32_t mock_displayed_i;

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

static int mock_shutdown_request_left;
static uint32_t mock_shutdown_request;

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

/* Helper for check vb2ex_display_ui calls.
 * Arguments which equal to mock_fixed should be ignored. */
static void displayed_eq(const char *text,
		       enum vb2_screen screen,
		       uint32_t locale_id,
		       uint32_t selected_item,
		       uint32_t disabled_item_mask)
{
	char text_buf[256];
	if (screen != mock_fixed) {
		sprintf(text_buf, "  screen of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].screen, screen,
			text_buf);
	}
	if (locale_id != mock_fixed) {
		sprintf(text_buf, "  locale_id of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].locale_id, locale_id,
			text_buf);
	}
	if (selected_item != mock_fixed) {
		sprintf(text_buf, "  selected_item of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].selected_item,
			selected_item, text_buf);
	}
	if (disabled_item_mask != mock_fixed) {
		sprintf(text_buf, "  disabled_item_mask of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].disabled_item_mask,
			disabled_item_mask, text_buf);
	}
	mock_displayed_i++;
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
	memset(mock_displayed, 0, sizeof(mock_displayed));
	mock_displayed_count = 0;
	mock_displayed_i = 0;

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

	/* for shutdown_required */
	power_button_state = POWER_BUTTON_HELD_SINCE_BOOT;
	if (t == FOR_DEVELOPER)
		mock_shutdown_request_left = -1;  /* Never request shutdown */
	else
		mock_shutdown_request_left = 301;
	/* no specified retval by default */
	mock_shutdown_request = mock_fixed;
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
	VB2_DEBUG("displayed %d: screen = %#x, locale_id = %u, "
		  "selected_item = %u, disabled_item_mask = %#x\n",
		  mock_displayed_count, screen, locale_id, selected_item,
		  disabled_item_mask);

	if (mock_displayed_count >= ARRAY_SIZE(mock_displayed)) {
		TEST_TRUE(0, "Test failed as mock vb2ex_display_ui ran out of"
			  " entries!");
		return VB2_ERROR_MOCK;
	}

	mock_displayed[mock_displayed_count] = (struct mock_displayed_t){
		.screen = screen,
		.locale_id = locale_id,
		.selected_item = selected_item,
		.disabled_item_mask = disabled_item_mask,
	};
	mock_displayed_count++;

	return VB2_SUCCESS;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_shutdown_request != mock_fixed)
		return mock_shutdown_request;  /* pre-specified */

	if (mock_shutdown_request_left == 0)
		return 1;
	else if (mock_shutdown_request_left > 0)
		mock_shutdown_request_left--;

	return 0;
}

/* Tests */

static void utilities_tests(void)
{
	VB2_DEBUG("Testing shutdown_required...\n");

	/* Release, press, hold, and release */
	if (!DETACHABLE) {
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"release, press, hold, and release");
		TEST_EQ(power_button_state, POWER_BUTTON_RELEASED,
			"  state: released");
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "  press");
		TEST_EQ(power_button_state, POWER_BUTTON_PRESSED,
			"  state: pressed");
		TEST_EQ(shutdown_required(ctx, 0), 0, "  hold");
		TEST_EQ(power_button_state, POWER_BUTTON_PRESSED,
			"  state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  release");
		TEST_EQ(power_button_state, POWER_BUTTON_RELEASED,
			"  state: released");
	}

	/* Press is ignored because we may held since boot */
	if (!DETACHABLE) {
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "press is ignored");
		TEST_NEQ(power_button_state, POWER_BUTTON_PRESSED,
			 "  state is not pressed");
	}

	/* Power button short press from key */
	if (!DETACHABLE) {
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
			"power button short press");
	}

	/* Lid closure = shutdown request anyway */
	reset_common_data(FOR_UTILITIES);
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_required(ctx, 0), 1, "lid closure");
	TEST_EQ(shutdown_required(ctx, 'A'), 1, "  lidsw + random key");

	/* Lid ignored by GBB flags */
	reset_common_data(FOR_UTILITIES);
	gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_required(ctx, 0), 0, "lid ignored");
	if (!DETACHABLE) {  /* power button works for non DETACHABLE */
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED |
					VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "  lidsw + pwdsw");
		TEST_EQ(power_button_state, POWER_BUTTON_PRESSED,
			"  state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  pwdsw release");
		TEST_EQ(power_button_state, POWER_BUTTON_RELEASED,
			"  state: released");
	}

	/* Lid ignored; power button short pressed */
	if (!DETACHABLE) {
		reset_common_data(FOR_UTILITIES);
		gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
		TEST_EQ(shutdown_required(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
			"lid ignored; power button short pressed");
	}

	/* DETACHABLE ignore power button */
	if (DETACHABLE) {
		/* pwdsw */
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"DETACHABLE: ignore pwdsw");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"  ignore on release");

		/* power button short press */
		reset_common_data(FOR_UTILITIES);
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(
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
	TEST_EQ(mock_displayed_count, 0, "  no screen");
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
	TEST_EQ(mock_displayed_count, 0, "  no screen");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default legacy not enabled");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_displayed_count, 0, "  no screen");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to USB");
	TEST_EQ(mock_displayed_count, 0, "  no screen");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to USB only if enabled */
	reset_common_data(FOR_DEVELOPER);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default USB not enabled");
	TEST_EQ(mock_displayed_count, 0, "  no screen");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	VB2_DEBUG("...done.\n");
}

static void broken_recovery_tests(void)
{
	VB2_DEBUG("Testing broken recovery mode...\n");

	VB2_DEBUG("...done.\n");
}

static void manual_recovery_tests(void)
{
	VB2_DEBUG("Testing manual recovery mode...\n");

	/* Timeout, shutdown */
	reset_common_data(FOR_RECOVERY);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"timeout, shutdown");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, mock_fixed, mock_fixed);
	TEST_EQ(mock_displayed_count, mock_displayed_i, "  no extra screens");

	/* Power button short pressed = shutdown request */
	/* TODO: FIXME */
	if (!DETACHABLE) {
		reset_common_data(FOR_RECOVERY);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		TEST_EQ(vb2_manual_recovery_menu(ctx),
			VBERROR_SHUTDOWN_REQUESTED,
			"power button short pressed = shutdown");
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     mock_fixed, mock_fixed, mock_fixed);
		TEST_EQ(mock_displayed_count, mock_displayed_i,
			"  no extra screens");
	}

	/* Item 1 = phone recovery */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_ENTER);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"phone recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("phone recovery", VB2_SCREEN_RECOVERY_PHONE_STEP1,
		     mock_fixed, mock_fixed, mock_fixed);
	TEST_EQ(mock_displayed_count, mock_displayed_i, "  no extra screens");

	/* Item 2 = external disk recovery */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_ENTER);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"external disk recovery");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 1, mock_fixed);
	displayed_eq("disk recovery", VB2_SCREEN_RECOVERY_DISK_STEP1,
		     mock_fixed, mock_fixed, mock_fixed);
	TEST_EQ(mock_displayed_count, mock_displayed_i, "  no extra screens");

	/* KEY_UP should not exceed boundary */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"KEY_UP should not out-of-bound");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	TEST_EQ(mock_displayed_count, mock_displayed_i, "  no extra screens");

	/* KEY_DOWN should not exceed boundary, either */
	reset_common_data(FOR_RECOVERY);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"neither does KEY_DOWN");
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 0, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 1, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 1, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 1, mock_fixed);
	displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
		     mock_fixed, 1, mock_fixed);
	TEST_EQ(mock_displayed_count, mock_displayed_i, "  no extra screens");

	/* For DETACHABLE */
	if (DETACHABLE) {
		reset_common_data(FOR_RECOVERY);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		TEST_EQ(vb2_manual_recovery_menu(ctx),
			VBERROR_SHUTDOWN_REQUESTED, "DETACHABLE");
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     mock_fixed, 0, mock_fixed);
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     mock_fixed, 0, mock_fixed);
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     mock_fixed, 1, mock_fixed);
		displayed_eq("recovery select", VB2_SCREEN_RECOVERY_SELECT,
			     mock_fixed, 0, mock_fixed);
		displayed_eq("phone recovery", VB2_SCREEN_RECOVERY_PHONE_STEP1,
			     mock_fixed, mock_fixed, mock_fixed);
		TEST_EQ(mock_displayed_count, mock_displayed_i,
			"  no extra screens");
	}

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
