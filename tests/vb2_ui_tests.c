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
static LoadKernelParams lkp;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;

static enum vb2_screen mock_screens_displayed[64];
static uint32_t mock_locales_displayed[64];
static uint32_t mock_screens_count = 0;

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
static int mock_vbtlk_retval_count;
static int mock_vbtlk_retval_total;

static void add_mock_vbtlk_retval(vb2_error_t retval, uint32_t get_info_flags)
{
	if (mock_vbtlk_retval_total < ARRAY_SIZE(mock_vbtlk_retval) &&
	    mock_vbtlk_retval_total < ARRAY_SIZE(mock_vbtlk_flag_expected)) {
		mock_vbtlk_retval[mock_vbtlk_retval_total] = retval;
		mock_vbtlk_flag_expected[mock_vbtlk_retval_total] =
		    get_info_flags;
		mock_vbtlk_retval_total++;
	}
	else {
		fprintf(stderr, "Ran out of mock_vbtlk_retval entries!\n");
	}
}

/* Reset mock data (for use before each test) */
static void reset_common_data()
{
	memset(&lkp, 0, sizeof(lkp));

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	vb2_nv_init(ctx);

	memset(mock_screens_displayed, 0, sizeof(mock_screens_displayed));
	mock_screens_count = 0;

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
	mock_vbtlk_retval_count = 0;
	mock_vbtlk_retval_total = 0;
}

/* Mock functions */

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
	if (mock_vbtlk_retval_count < mock_vbtlk_retval_total) {
		mock_vbtlk_last_retval =
		    mock_vbtlk_retval[mock_vbtlk_retval_count];
		mock_vbtlk_last_flag_expected =
		    mock_vbtlk_flag_expected[mock_vbtlk_retval_count];
		mock_vbtlk_retval_count++;
	}

	if (mock_vbtlk_last_flag_expected == get_info_flags)
		return mock_vbtlk_last_retval;

	return VB2_ERROR_MOCK;
}

vb2_error_t vb2ex_display_ui(enum vb2_screen screen, uint32_t locale)
{
	if (mock_screens_count < ARRAY_SIZE(mock_screens_displayed) &&
	    mock_screens_count < ARRAY_SIZE(mock_locales_displayed)) {
		mock_screens_displayed[mock_screens_count] = screen;
		mock_locales_displayed[mock_screens_count] = locale;
		mock_screens_count++;
	}
	else {
		fprintf(stderr, "Ran out of screens_displayed entries!\n");
	}

	fprintf(stderr, "vb2ex_display_menu: screens_displayed[%d],"
		" screen = %#x, locale = %#x\n",
		mock_screens_count - 1, screen, locale);

	return VB2_SUCCESS;
}

/* Tests */

static void developer_tests(void)
{
	/* Proceed */
	reset_common_data();
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");

	/* Proceed to legacy */
	reset_common_data();
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_LEGACY;
	mock_dev_boot_legacy_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to legacy");
	TEST_EQ(mock_vbexlegacy_called, 1, "  try legacy");
	TEST_EQ(mock_altfw_num, 0, "  check altfw_num");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

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

	/* Proceed to usb */
	reset_common_data();
	add_mock_vbtlk_retval(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	mock_default_boot = VB2_DEV_DEFAULT_BOOT_USB;
	mock_dev_boot_usb_allowed = 1;
	TEST_EQ(vb2_developer_menu(ctx), VB2_SUCCESS, "proceed to usb");
	TEST_EQ(mock_screens_displayed[0], VB2_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");

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
