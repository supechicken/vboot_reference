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
#include "vboot_audio.h"
#include "vboot_kernel.h"

/* Mock data */
static LoadKernelParams lkp;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;

static enum vb2_screen mock_screens_displayed[64];
static uint32_t mock_locales_displayed[64];
static uint32_t mock_screens_count = 0;

static int mock_audio_start_called;
static int mock_audio_looping_calls_left;

static enum vb2_dev_default_boot mock_default_boot;
static int mock_dev_boot_allowed;
static int mock_dev_boot_legacy_allowed;
static int mock_dev_boot_usb_allowed;

static int mock_vbexlegacy_called;
static enum VbAltFwIndex_t mock_altfw_num;

static vb2_error_t mock_vbtlk_retval[5];
static vb2_error_t mock_vbtlk_last_retval;
static uint32_t mock_vbtlk_flag_expected[5];
static uint32_t mock_vbtlk_last_flag_expected;
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
	    mock_vbtlk_total >= ARRAY_SIZE(mock_vbtlk_flag_expected)) {
		TEST_TRUE(0, "Test failed as mock_vbtlk ran out of entries!");
		return;
	}

	mock_vbtlk_retval[mock_vbtlk_total] = retval;
	mock_vbtlk_flag_expected[mock_vbtlk_total] = get_info_flags;
	mock_vbtlk_total++;
}

/* Reset mock data (for use before each test) */
static void reset_common_data()
{
	memset(&lkp, 0, sizeof(lkp));

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	vb2_nv_init(ctx);

	memset(mock_keypress, 0, sizeof(mock_keypress));
	memset(mock_keyflags, 0, sizeof(mock_keyflags));
	mock_keypress_count = 0;
	mock_keypress_total = 0;

	memset(mock_screens_displayed, 0, sizeof(mock_screens_displayed));
	mock_screens_count = 0;

	mock_audio_start_called = 0;
	mock_audio_looping_calls_left = 100;

	mock_default_boot = VB2_DEV_DEFAULT_BOOT_DISK;
	mock_dev_boot_allowed = 1;
	mock_dev_boot_legacy_allowed = 0;
	mock_dev_boot_usb_allowed = 0;

	mock_vbexlegacy_called = 0;
	mock_altfw_num = -100;

	memset(mock_vbtlk_retval, 0, sizeof(mock_vbtlk_retval));
	memset(mock_vbtlk_flag_expected, 0, sizeof(mock_vbtlk_flag_expected));
	mock_vbtlk_last_flag_expected = VB_DISK_FLAG_FIXED;
	mock_vbtlk_last_retval = VB2_SUCCESS;
	mock_vbtlk_count = 0;
	mock_vbtlk_total = 0;
}

/* Mock functions */

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

void vb2_audio_start(struct vb2_context *c)
{
	mock_audio_start_called++;
}

int vb2_audio_looping(void)
{
	if (mock_audio_looping_calls_left == 0)
		return 0;
	else if (mock_audio_looping_calls_left > 0)
		mock_audio_looping_calls_left--;

	return 1;
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
	if (mock_vbtlk_count < mock_vbtlk_total) {
		mock_vbtlk_last_retval = mock_vbtlk_retval[mock_vbtlk_count];
		mock_vbtlk_last_flag_expected =
		    mock_vbtlk_flag_expected[mock_vbtlk_count];
		mock_vbtlk_count++;
	}

	if (mock_vbtlk_last_flag_expected == get_info_flags)
		return mock_vbtlk_last_retval;

	return VB2_ERROR_MOCK;
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale)
{
	if (mock_screens_count >= ARRAY_SIZE(mock_screens_displayed) ||
	    mock_screens_count >= ARRAY_SIZE(mock_locales_displayed)) {
		TEST_TRUE(0, "Test failed as mock vb2ex_display_ui ran out of"
			  " entries!");
		return VB2_ERROR_MOCK;
	}

	mock_screens_displayed[mock_screens_count] = screen;
	mock_locales_displayed[mock_screens_count] = locale;
	mock_screens_count++;

	VB2_DEBUG("screens %d: screen = %#x, locale = %#x\n",
		  mock_screens_count - 1, screen, locale);

	return VB2_SUCCESS;
}

vb2_error_t VbExInitPageContent(const char *info_str, uint32_t *num_page,
			     enum VbScreenType_t screen)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExFreePageContent(void)
{
	return VB2_SUCCESS;
}
/* Tests */

static void developer_tests(void)
{
	/* Proceed after timeout */
	reset_common_data();
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed after timeout");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Reset timer whenever seeing a new key */
	reset_common_data();
	add_mock_keypress('A');  /* Not a shortcut key */
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"Timeout after seeing a key");
	TEST_EQ(mock_audio_start_called, 2, "  audio start called twice");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

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
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Proceed to legacy only if enabled */
	reset_common_data();
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default legacy not enabled");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Proceed to usb after timeout */
	reset_common_data();
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to usb");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Proceed to usb only if enabled */
	reset_common_data();
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"default usb not enabled");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* If no usb, tries fixed disk */
	reset_common_data();
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	add_mock_vbtlk(VB2_ERROR_LK, VB_DISK_FLAG_REMOVABLE);
	add_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS,
		"  default usb with no disk");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_EQ(mock_audio_start_called, 1, "  audio start called once");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Ctrl+D = boot from internal in loop */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('D'));
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+D");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  no recovery");
	TEST_NEQ(mock_audio_looping_calls_left, 0, "  audio aborted");

	/* Ctrl+D doesn't boot legacy even if default boot specified */
	reset_common_data();
	add_mock_keypress(VB_KEY_CTRL('D'));
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "Ctrl+D no legacy");
	TEST_EQ(mock_vbexlegacy_called, 0, "  not legacy");

	/* Volume-down long press shortcut acts like Ctrl+D */
	/* TODO(roccochen): how to bypass config DETACHABLE? */

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
	TEST_NEQ(mock_audio_looping_calls_left, 0, "  audio aborted");

	/* TODO: Ctrl+L; Ctrl+L only if; Ctrl+U; Ctrl+U only if; */
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
