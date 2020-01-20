/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for developer and recovery mode UIs.
 */

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
static struct vb2_ui_state screens_displayed[64];
static uint32_t screens_count = 0;
static int audio_start_calls_left;
static int audio_looping_calls_left;
static vb2_error_t vbtlk_retval[5];
static vb2_error_t vbtlk_last_retval;
static int vbtlk_retval_count;
static int vbtlk_retval_total;

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

static void add_vbtlk_retval(vb2_error_t retval, uint32_t get_info_flags)
{
	if (vbtlk_retval_total < ARRAY_SIZE(vbtlk_retval)) {
		vbtlk_retval[vbtlk_retval_total] = SET_RETVAL(retval,
							      get_info_flags);
		vbtlk_retval_total++;
	}
	else {
		fprintf(stderr, "Ran out of vbtlk_retval entries!\n");
	}
}

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

	memset(screens_displayed, 0, sizeof(screens_displayed));
	screens_count = 0;

	audio_start_calls_left = 1;
	audio_looping_calls_left = 100;

	memset(vbtlk_retval, 0, sizeof(vbtlk_retval));
	vbtlk_last_retval = SET_RETVAL(VB2_ERROR_MOCK, VB_DISK_FLAG_FIXED);
	vbtlk_retval_count = 0;
	vbtlk_retval_total = 0;
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
	audio_start_calls_left--;
}

int vb2_audio_looping(void)
{
	if (audio_looping_calls_left == 0)
		return 0;
	else if (audio_looping_calls_left > 0)
		audio_looping_calls_left--;

	return 1;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	if (vbtlk_retval_count < vbtlk_retval_total &&
	    vbtlk_retval[vbtlk_retval_count] != 0)
		vbtlk_last_retval = vbtlk_retval[vbtlk_retval_count++];

	return vbtlk_last_retval + get_info_flags;
}

vb2_error_t vb2ex_display_menu(const struct vb2_ui_state *state)
{
	if (screens_count < ARRAY_SIZE(screens_displayed))
		screens_displayed[screens_count++] = *state;
	else
		fprintf(stderr, "Ran out of screens_displayed entries!\n");

	fprintf(stderr, "vb2ex_display_menu: screens_displayed[%d],"
		" locale = %#x, screen = %#x\n",
		screens_count - 1, state->locale, state->screen);

	return VB2_SUCCESS;
}

/* Tests */

static void developer_tests(void)
{
	/* Proceed after timeout */
	reset_common_data(FOR_DEV);
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK, "Timeout");
	TEST_EQ(screens_displayed[0].screen, VB_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(screens_count, 1, "  no extra screens");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0,
		"  recovery reason");
	TEST_EQ(audio_start_calls_left, 0, "  used up audio start");
	TEST_EQ(audio_looping_calls_left, 0, "  used up audio looping");

	/* Reset timer whenever seeing a new key */
	reset_common_data(FOR_DEV);
	add_mock_keypress('A');  /* Not a shortcut key */
	audio_start_calls_left = 2;
	TEST_EQ(vb2_developer_menu(ctx), VB2_ERROR_MOCK,
		"Timeout after seeing a key");
	TEST_EQ(audio_start_calls_left, 0, "  used up audio start");
	TEST_EQ(audio_looping_calls_left, 0, "  used up audio looping");

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
	TEST_NEQ(audio_looping_calls_left, 0, "  aborts audio");
	TEST_EQ(screens_displayed[0].screen, VB_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(screens_count, 1, "  no extra screens");
}

static void broken_recovery_tests(void)
{
	/* Only infinite loop for current implementation, no test needed */
}

static void manual_recovery_tests(void)
{
	/* INSERT boots without screens if we have a valid image on first try */
	reset_common_data(FOR_MANUAL_REC);
	add_vbtlk_retval(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	add_vbtlk_retval(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(vb2_manual_recovery_menu(ctx), VB2_SUCCESS,
		"INSERT boots without screens if valid on first try");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_RECOVERY_REQUEST), 0, "  no recovery");
	TEST_EQ(screens_displayed[0].screen, VB_SCREEN_BLANK,
		"  final blank screen");
	TEST_EQ(screens_count, 1, "  no extra screens");
}

int main(void)
{
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
