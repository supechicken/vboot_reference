/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for UI utility functions.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2ui.h"
#include "2ui_private.h"
#include "test_common.h"
#include "vb2_ui_test_common.h"
#include "vboot_api.h"

/* Mock screen index for testing screen utility functions. */
#define MOCK_SCREEN1 0xeff
#define MOCK_SCREEN2 0xfff
#define MOCK_SCREEN_TARGET0 0xff0
#define MOCK_SCREEN_TARGET1 0xff1
#define MOCK_SCREEN_TARGET2 0xff2
#define MOCK_SCREEN_TARGET3 0xff3
#define MOCK_SCREEN_TARGET4 0xff4

/* Mock data */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_gbb_header gbb;

static uint32_t mock_shutdown_request;

static struct vb2_screen_state mock_state;
static vb2_screen mock_new_screen;

/* Mocks for testing screen utility functions. */
const struct vb2_menu_item mock_screen1_items[] = {};
struct vb2_screen_info mock_screen1 =
{
	.id = MOCK_SCREEN1,
	.name = "mock_screen1: menuless screen",
	.num_items = ARRAY_SIZE(mock_screen1_items),
	.items = mock_screen1_items,
};
struct vb2_menu_item mock_screen2_items[] =
{
	{
		.text = "option 0",
		.target = MOCK_SCREEN_TARGET0,
	},
	{
		.text = "option 1",
		.target = MOCK_SCREEN_TARGET1,
	},
	{
		.text = "option 2",
		.target = MOCK_SCREEN_TARGET2,
	},
	{
		.text = "option 3",
		.target = MOCK_SCREEN_TARGET3,
	},
	{
		.text = "option 4",
		.target = MOCK_SCREEN_TARGET4,
	},
};

const struct vb2_screen_info mock_screen2 =
{
	.id = MOCK_SCREEN2,
	.name = "mock_screen2: menu screen",
	.num_items = ARRAY_SIZE(mock_screen2_items),
	.items = mock_screen2_items,
};

void screen_state_eq(const struct vb2_screen_state *state,
		     enum vb2_screen screen,
		     uint32_t locale_id,
		     uint32_t selected_item,
		     uint32_t disabled_item_mask)
{
	char text_buf[256];
	if (screen != MOCK_FIXED) {
		sprintf(text_buf, "  screen of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].screen, screen,
			text_buf);
	}
	if (locale_id != MOCK_FIXED) {
		sprintf(text_buf, "  locale_id of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].locale_id, locale_id,
			text_buf);
	}
	if (selected_item != MOCK_FIXED) {
		sprintf(text_buf, "  selected_item of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].selected_item,
			selected_item, text_buf);
	}
	if (disabled_item_mask != MOCK_FIXED) {
		sprintf(text_buf, "  disabled_item_mask of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].disabled_item_mask,
			disabled_item_mask, text_buf);
	}
	mock_displayed_i++;
}
/* Reset mock data (for use before each test) */
static void reset_common_data(void)
{
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	memset(&gbb, 0, sizeof(gbb));

	vb2_nv_init(ctx);

	/* For common data in vb2_ui_test_common.h */
	reset_ui_common_data();

	/* For shutdown_required */
	power_button = POWER_BUTTON_HELD_SINCE_BOOT;
	mock_shutdown_request = MOCK_FIXED;

	/* For input actions */
	state.screen = VB2_SCREEN_BLANK;
	state.locale_id = 0;
	state.selected_item = 0;
	state.disabled_item_mask = 0;
}

/* Mock functions */
struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_shutdown_request != MOCK_FIXED)
		return mock_shutdown_request;

	return 0;
}

const struct vb2_screen_info *vb2_get_screen_info(enum vb2_screen screen)
{
	switch ((int)screen) {
	case MOCK_SCREEN1:
		return &mock_screen1;
	case MOCK_SCREEN2:
		return &mock_screen2;
	default:
		return NULL;
	}
}

/* Tests */
static void shutdown_required_tests(void)
{
	VB2_DEBUG("Testing shutdown_required...\n");

	/* Release, press, hold, and release */
	if (!DETACHABLE) {
		reset_common_data();
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"release, press, hold, and release");
		TEST_EQ(power_button, POWER_BUTTON_RELEASED,
			"  state: released");
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "  press");
		TEST_EQ(power_button, POWER_BUTTON_PRESSED,
			"  state: pressed");
		TEST_EQ(shutdown_required(ctx, 0), 0, "  hold");
		TEST_EQ(power_button, POWER_BUTTON_PRESSED,
			"  state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  release");
		TEST_EQ(power_button, POWER_BUTTON_RELEASED,
			"  state: released");
	}

	/* Press is ignored because we may held since boot */
	if (!DETACHABLE) {
		reset_common_data();
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "press is ignored");
		TEST_NEQ(power_button, POWER_BUTTON_PRESSED,
			 "  state is not pressed");
	}

	/* Power button short press from key */
	if (!DETACHABLE) {
		reset_common_data();
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
			"power button short press");
	}

	/* Lid closure = shutdown request anyway */
	reset_common_data();
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_required(ctx, 0), 1, "lid closure");
	TEST_EQ(shutdown_required(ctx, 'A'), 1, "  lidsw + random key");

	/* Lid ignored by GBB flags */
	reset_common_data();
	gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
	mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
	TEST_EQ(shutdown_required(ctx, 0), 0, "lid ignored");
	if (!DETACHABLE) {  /* Power button works for non DETACHABLE */
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED |
					VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "  lidsw + pwdsw");
		TEST_EQ(power_button, POWER_BUTTON_PRESSED,
			"  state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  pwdsw release");
		TEST_EQ(power_button, POWER_BUTTON_RELEASED,
			"  state: released");
	}

	/* Lid ignored; power button short pressed */
	if (!DETACHABLE) {
		reset_common_data();
		gbb.flags |= VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN;
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_LID_CLOSED;
		TEST_EQ(shutdown_required(ctx, VB_BUTTON_POWER_SHORT_PRESS), 1,
			"lid ignored; power button short pressed");
	}

	/* DETACHABLE ignore power button */
	if (DETACHABLE) {
		/* Flag pwdsw */
		reset_common_data();
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"DETACHABLE: ignore pwdsw");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 0,
			"  ignore on release");

		/* Power button short press */
		reset_common_data();
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(
		    ctx, VB_BUTTON_POWER_SHORT_PRESS), 0,
		    "DETACHABLE: ignore power button short press");
	}

	VB2_DEBUG("...done.\n");
}

static void input_action_tests(void)
{
	VB2_DEBUG("Testing input actions...\n");

	/* Valid menu_up_action */
	reset_common_data();
	state.screen = MOCK_SCREEN2;
	state.selected_item = 2;
	mock_new_screen = MOCK_SCREEN2;
	TEST_EQ(menu_up_action(ctx, &mock_screen2, &state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "valid menu_up_action");
	TEST_EQ(mock_new_screen, MOCK_SCREEN2, "   new_screen");
	TEST_EQ(state.screen, MOCK_SCREEN2, "  state.screen");
	TEST_EQ(state.locale_id, 0, "  state.locale"


	/* Valid menu_up_action with mask */
	/* Invalid menu_up_action */


	/* menu_down_action */

	/* menu_select_action */

	/* menu_back_action */

	/* lookup */

	VB2_DEBUG("...done.\n");
}

static void screen_related_tests(void)
{
	VB2_DEBUG("Testing screen related utility functions...\n");

	VB2_DEBUG("...done.\n");
}

int main(void)
{
	shutdown_required_tests();
	input_action_tests();
	screen_related_tests();

	return gTestSuccess ? 0 : 255;
}
