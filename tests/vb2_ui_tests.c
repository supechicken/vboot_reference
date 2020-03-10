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
#include "test_common.h"
#include "vboot_api.h"
#include "vboot_kernel.h"

/* Mock data */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_gbb_header gbb;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;

static enum vb2_screen mock_screens_displayed[64];
static uint32_t mock_locales_displayed[64];
static uint32_t mock_screens_count;

static uint64_t mock_get_timer_last_retval[2];  /* check if finished late */
static uint64_t mock_time;
static uint64_t mock_time_fixed = (31ULL * VB_USEC_PER_SEC);
static int mock_vbexbeep_called;

static enum vb2_dev_default_boot mock_default_boot;
static int mock_dev_boot_allowed;
static int mock_dev_boot_legacy_allowed;
static int mock_dev_boot_usb_allowed;

static int mock_vbexlegacy_called;
static vb2_error_t mock_vbexlegacy_retval;
static enum VbAltFwIndex_t mock_altfw_num;

static vb2_error_t mock_vbtlk_retval[5];
static uint32_t mock_vbtlk_expected_flag[5];
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

/* Reset mock data (for use before each test) */
static void reset_common_data()
{
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	memset(&gbb, 0, sizeof(gbb));

	vb2_nv_init(ctx);

	memset(mock_keypress, 0, sizeof(mock_keypress));
	memset(mock_keyflags, 0, sizeof(mock_keyflags));
	mock_keypress_count = 0;
	mock_keypress_total = 0;

	memset(mock_screens_displayed, 0, sizeof(mock_screens_displayed));
	mock_screens_count = 0;

	memset(mock_get_timer_last_retval, 0,
	       sizeof(mock_get_timer_last_retval));
	mock_time = mock_time_fixed;
	mock_vbexbeep_called = 0;

	mock_default_boot = VB2_DEV_DEFAULT_BOOT_DISK;
	mock_dev_boot_allowed = 1;
	mock_dev_boot_legacy_allowed = 0;
	mock_dev_boot_usb_allowed = 0;

	mock_vbexlegacy_called = 0;
	mock_vbexlegacy_retval = VB2_SUCCESS;
	mock_altfw_num = -100;

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

uint64_t VbExGetTimer(void)
{
	mock_get_timer_last_retval[1] = mock_get_timer_last_retval[0];
	mock_get_timer_last_retval[0] = mock_time;

	return mock_time;
}

void VbExSleepMs(uint32_t msec)
{
	mock_time += msec * VB_USEC_PER_MSEC;
}

vb2_error_t VbExBeep(uint32_t msec, uint32_t frequency)
{
	mock_vbexbeep_called++;
	return VB2_SUCCESS;
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

	return mock_vbexlegacy_retval;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	if (mock_vbtlk_count >= mock_vbtlk_total) {
		TEST_TRUE(0, "  VbTryLoadKernel called too many times.");
		return VB2_ERROR_MOCK;
	}

	TEST_EQ(mock_vbtlk_expected_flag[mock_vbtlk_count], get_info_flags,
		"  unexpected get_info_flags");

	return mock_vbtlk_retval[mock_vbtlk_count++];
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale)
{
	VB2_DEBUG("screens %d: screen = %#x, locale = %u\n",
		  mock_screens_count, screen, locale);

	if (mock_screens_count >= ARRAY_SIZE(mock_screens_displayed) ||
	    mock_screens_count >= ARRAY_SIZE(mock_locales_displayed)) {
		TEST_TRUE(0, "Test failed as mock vb2ex_display_ui ran out of"
			  " entries!");
		return VB2_ERROR_MOCK;
	}

	mock_screens_displayed[mock_screens_count] = screen;
	mock_locales_displayed[mock_screens_count] = locale;
	mock_screens_count++;

	return VB2_SUCCESS;
}

/* Tests */

static void developer_tests(void)
{
	int i;
	char test_name[256];

	/* Proceed after timeout */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed after short delay */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	gbb.flags |= VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  2 * VB_USEC_PER_SEC, "  finished short delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  2 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 0, "  no beep for short delay twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Reset timer whenever seeing a new key */
	reset_common_data();
	add_mock_keypress('A');  /* Not a shortcut key */
	add_mock_keypress('A');  /* Need two keys since it read before slept */
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"Timeout after seeing a key");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay a little later");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Use normal delay after seeing a new key even if GBB is set */
	reset_common_data();
	add_mock_keypress('A');  /* Not a shortcut key */
	add_mock_keypress('A');  /* Need two keys since it read before slept */
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	gbb.flags |= VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"Use normal delay even if GBB is set");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished normal delay");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy after timeout */
	reset_common_data();
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to legacy");
	TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
	TEST_EQ(mock_altfw_num, 0, "  check altfw_num");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to legacy only if enabled */
	reset_common_data();
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
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* If legacy failed, tries fixed disk */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	mock_vbexlegacy_retval = VB2_ERROR_MOCK;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"legacy failed");
	TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to usb after timeout */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to usb");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Proceed to usb only if enabled */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default usb not enabled");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* If no usb, tries fixed disk */
	reset_common_data();
	add_mock_vbtlk(VB2_ERROR_LK, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"  default usb with no disk");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Enter = shutdown requested in loop */
	reset_common_data();
	add_mock_keypress(VB_KEY_ENTER);
	TEST_EQ(vb2_developer_menu(ctx), VBERROR_SHUTDOWN_REQUESTED,
		"shutdown requested");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  delay loop aborted");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");


	/* Ctrl+D = boot from internal in loop */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('D'));
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+D");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  delay loop aborted");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Ctrl+D doesn't boot legacy even if default boot specified */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('D'));
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+D no legacy");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* DETACHABLE volume-down long press shortcut acts like Ctrl+D */
	if (DETACHABLE) {
		reset_common_data();
		add_mock_keypress(VB_BUTTON_VOL_DOWN_LONG_PRESS);
		add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
		TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
			"DETACHABLE volume-down long press");
		TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
		TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
			"  final blank screen");
		TEST_EQ(mock_screens_count, 1, "  no extra screens");
		TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
			"  no recovery");
		TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed <
			  30 * VB_USEC_PER_SEC, "  delay loop aborted");
		TEST_EQ(mock_vbtlk_count, mock_vbtlk_total,
			"  used up mock_vbtlk");
	}

	/* Ctrl+L tries legacy boot mode only if enabled */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('L'));
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+L disabled");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
		  30 * VB_USEC_PER_SEC, "  finished delay");
	TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  not finished too late");
	TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* Ctrl+L = boot legacy if enabled */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('L'));
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+L");
	TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
	TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed <
		  30 * VB_USEC_PER_SEC, "  delay loop aborted");
	TEST_EQ(mock_vbtlk_count, mock_vbtlk_total, "  used up mock_vbtlk");

	/* 0...9 = boot alternative firmware */
	for (i = 0; i <= 9; i++) {
		/* disabled */
		sprintf(test_name, "key %d disabled", i);
		reset_common_data();
		add_mock_keypress('0' + i);
		add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
		TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, test_name);
		TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
		TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
			"  final blank screen");
		TEST_EQ(mock_screens_count, 1, "  no extra screens");
		TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
			"  no recovery");
		TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed >=
			  30 * VB_USEC_PER_SEC, "  finished delay");
		TEST_TRUE(mock_get_timer_last_retval[1] - mock_time_fixed <
			  30 * VB_USEC_PER_SEC, "  not finished too late");
		TEST_EQ(mock_vbexbeep_called, 2, "  beep twice");
		TEST_EQ(mock_vbtlk_count, mock_vbtlk_total,
			"  used up mock_vbtlk");

		/* enabled */
		sprintf(test_name, "key %d", i);
		reset_common_data();
		add_mock_keypress('0' + i);
		mock_dev_boot_legacy_allowed = 1;
		TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, test_name);
		TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
		TEST_EQ(mock_altfw_num, i, "  check altfw_num");
		TEST_TRUE(mock_get_timer_last_retval[0] - mock_time_fixed <
			  30 * VB_USEC_PER_SEC, "  delay loop aborted");
		TEST_EQ(mock_vbtlk_count, mock_vbtlk_total,
			"  used up mock_vbtlk");
	}
}

static void broken_recovery_tests(void)
{
	/* TODO(roccochen) */
}

static void manual_recovery_tests(void)
{
	/* TODO(roccochen) */
}

int main(void)
{
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
