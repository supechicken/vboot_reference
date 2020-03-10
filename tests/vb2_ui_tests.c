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
#include "vboot_display.h"
#include "vboot_kernel.h"

#define SET_RETVAL(retval, flag) ((retval)-(flag))

/* Mock data */
static LoadKernelParams lkp;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;
static struct vb2_secdata_fwmp *fwmp;

static uint32_t mock_keypress[64];
static uint32_t mock_keyflags[64];
static uint32_t mock_keypress_count;
static uint32_t mock_keypress_total;
static enum vb2_screen mock_screens_displayed[64];
static uint32_t mock_locales_displayed[64];
static uint32_t mock_screens_count = 0;
static int mock_audio_start_calls_left;
static int mock_audio_looping_calls_left;
static vb2_error_t mock_vbtlk_retval[5];
static vb2_error_t mock_vbtlk_last_retval;
static int mock_vbtlk_retval_count;
static int mock_vbtlk_retval_total;

/* Type of test to reset for */

enum reset_type {
	FOR_DEV,
	FOR_BROKEN_REC,
	FOR_MANUAL_REC,
};

static void add_mock_key(uint32_t press, uint32_t flags)
{
	if (mock_keypress_total < ARRAY_SIZE(mock_keypress) &&
	    mock_keypress_total < ARRAY_SIZE(mock_keyflags)) {
		mock_keypress[mock_keypress_total] = press;
		mock_keyflags[mock_keypress_total] = flags;
		mock_keypress_total++;
	}
	else {
		fprintf(stderr, "Ran out of mock_key entries!\n");
	}
}

static void add_mock_keypress(uint32_t press)
{
	add_mock_key(press, 0);
}

/*
 * TODO(roccochen): add add_mock_vbtlk_retval() if we need it
static void add_mock_vbtlk_retval(vb2_error_t retval, uint32_t get_info_flags)
{
	if (mock_vbtlk_retval_total < ARRAY_SIZE(mock_vbtlk_retval)) {
		mock_vbtlk_retval[mock_vbtlk_retval_total] =
		    SET_RETVAL(retval, get_info_flags);
		mock_vbtlk_retval_total++;
	}
	else {
		fprintf(stderr, "Ran out of mock_vbtlk_retval entries!\n");
	}
}
 */

/* Reset mock data (for use before each test) */
static void reset_common_data(enum reset_type t)
{
	memset(&lkp, 0, sizeof(lkp));

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	vb2_nv_init(ctx);

	sd = vb2_get_sd(ctx);
	if (t == FOR_DEV)
		sd->flags |= VB2_SD_FLAG_DEV_MODE_ENABLED;
	else if (t == FOR_MANUAL_REC)
		sd->flags |= VB2_SD_FLAG_MANUAL_RECOVERY;

	/* CRC will be invalid after here, but nobody's checking */
	sd->status |= VB2_SD_STATUS_SECDATA_FWMP_INIT;
	fwmp = (struct vb2_secdata_fwmp *)ctx->secdata_fwmp;

	memset(&gbb, 0, sizeof(gbb));

	memset(mock_keyflags, 0, sizeof(mock_keyflags));
	mock_keypress_count = 0;
	mock_keypress_total = 0;

	memset(mock_screens_displayed, 0, sizeof(mock_screens_displayed));
	mock_screens_count = 0;

	mock_audio_start_calls_left = 1;
	mock_audio_looping_calls_left = 100;

	memset(mock_vbtlk_retval, 0, sizeof(mock_vbtlk_retval));
	mock_vbtlk_last_retval = SET_RETVAL(VB2_ERROR_MOCK, VB_DISK_FLAG_FIXED);
	mock_vbtlk_retval_count = 0;
	mock_vbtlk_retval_total = 0;
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

void vb2_audio_start(struct vb2_context *c)
{
	mock_audio_start_calls_left--;
}

int vb2_audio_looping(void)
{
	if (mock_audio_looping_calls_left == 0)
		return 0;
	else if (mock_audio_looping_calls_left > 0)
		mock_audio_looping_calls_left--;

	return 1;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	if (mock_vbtlk_retval_count < mock_vbtlk_retval_total &&
	    mock_vbtlk_retval[mock_vbtlk_retval_count] != 0)
		mock_vbtlk_last_retval =
		    mock_vbtlk_retval[mock_vbtlk_retval_count++];

	return mock_vbtlk_last_retval + get_info_flags;
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
	/* Proceed after timeout */
	reset_common_data(FOR_DEV);
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK, "Timeout");
	TEST_EQ(mock_screens_displayed[0], VB_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_EQ(mock_audio_start_calls_left, 0, "  used up audio start");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* Reset timer whenever seeing a new key */
	reset_common_data(FOR_DEV);
	add_mock_keypress('A');  /* Not a shortcut key */
	mock_audio_start_calls_left = 2;
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK,
		"Timeout after seeing a key");
	TEST_EQ(mock_audio_start_calls_left, 0, "  used up audio start");
	TEST_EQ(mock_audio_looping_calls_left, 0, "  used up audio looping");

	/* If no USB tries fixed disk */
	reset_common_data(FOR_DEV);
	vb2_nv_set(ctx, VB2_NV_DEV_BOOT_USB, 1);
	vb2_nv_set(ctx, VB2_NV_DEV_DEFAULT_BOOT, VB2_DEV_DEFAULT_BOOT_USB);
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK,
		"default USB with no disk");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");

	/* Ctrl+D dismisses warning */
	reset_common_data(FOR_DEV);
	add_mock_keypress(VB_KEY_CTRL('D'));
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK, "Ctrl+D");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_NEQ(mock_audio_looping_calls_left, 0, "  aborts audio");
	TEST_EQ(mock_screens_displayed[0], VB_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(mock_screens_count, 1, "  no extra screens");
}

static void broken_recovery_tests(void)
{
	/* Only infinite loop for current implementation, no test needed */
}

static void manual_recovery_tests(void)
{
	/* Only infinite loop for current implementation, no test needed */
}

int main(void)
{
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
