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
static enum vb2_screen mock_new_screen;

/* Mocks for testing screen utility functions. */
const struct vb2_menu_item mock_empty_menu[] = {};
struct vb2_screen_info mock_screen_blank = {
	.id = VB2_SCREEN_BLANK,
	.name = "mock blank",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
struct vb2_screen_info mock_screen1 =
{
	.id = MOCK_SCREEN1,
	.name = "mock_screen1: menuless screen",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
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
		.text = "option 4 (no target)",
	},
};
const struct vb2_screen_info mock_screen2 =
{
	.id = MOCK_SCREEN2,
	.name = "mock_screen2: menu screen",
	.num_items = ARRAY_SIZE(mock_screen2_items),
	.items = mock_screen2_items,
};

static void screen_state_eq(const struct vb2_screen_state *state,
		     enum vb2_screen screen,
		     uint32_t locale_id,
		     uint32_t selected_item,
		     uint32_t disabled_item_mask)
{
	if (screen != MOCK_FIXED)
		TEST_EQ(state->screen->id, screen,
			"  state.screen");
	if (locale_id != MOCK_FIXED)
		TEST_EQ(state->locale_id, locale_id,
			"  state.locale_id");
	if (selected_item != MOCK_FIXED)
		TEST_EQ(state->selected_item,
			selected_item, "  state.selected_item");
	if (disabled_item_mask != MOCK_FIXED)
		TEST_EQ(state->disabled_item_mask,
			disabled_item_mask, "  state.disabled_item_mask");
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
	mock_state.screen = vb2_get_screen_info(VB2_SCREEN_BLANK);
	mock_state.locale_id = 0;
	mock_state.selected_item = 0;
	mock_state.disabled_item_mask = 0;
	mock_new_screen = VB2_SCREEN_BLANK;
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
	case VB2_SCREEN_BLANK:
		return &mock_screen_blank;
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
			"  power button state: released");
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "  press");
		TEST_EQ(power_button, POWER_BUTTON_PRESSED,
			"  power button state: pressed");
		TEST_EQ(shutdown_required(ctx, 0), 0, "  hold");
		TEST_EQ(power_button, POWER_BUTTON_PRESSED,
			"  power button state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  release");
		TEST_EQ(power_button, POWER_BUTTON_RELEASED,
			"  power button state: released");
	}

	/* Press is ignored because we may held since boot */
	if (!DETACHABLE) {
		reset_common_data();
		mock_shutdown_request = VB_SHUTDOWN_REQUEST_POWER_BUTTON;
		TEST_EQ(shutdown_required(ctx, 0), 0, "press is ignored");
		TEST_NEQ(power_button, POWER_BUTTON_PRESSED,
			 "  power button state is not pressed");
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
			"  power button state: pressed");
		mock_shutdown_request = 0;
		TEST_EQ(shutdown_required(ctx, 0), 1, "  pwdsw release");
		TEST_EQ(power_button, POWER_BUTTON_RELEASED,
			"  power button state: released");
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
	int i;
	char test_name[256];

	VB2_DEBUG("Testing input actions...\n");

	/* Valid menu_up_action */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	TEST_EQ(menu_up_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "valid menu_up_action");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 1, 0);

	/* Valid menu_up_action with mask */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	mock_state.disabled_item_mask = 0x0a;  /* 0b01010 */
	TEST_EQ(menu_up_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "valid menu_up_action with mask");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 0, 0x0a);

	/* Invalid menu_up_action (blocked) */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 0;
	TEST_EQ(menu_up_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "invalid menu_up_action (blocked)");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 0, 0);

	/* Invalid menu_up_action (blocked by mask) */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	mock_state.disabled_item_mask = 0x0b;  /* 0b01011 */
	TEST_EQ(menu_up_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING,
		"invalid menu_up_action (blocked by mask)");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 2, 0x0b);

	/* Valid menu_down_action */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	TEST_EQ(menu_down_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "valid menu_down_action");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 3, 0);

	/* Valid menu_down_action with mask */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	mock_state.disabled_item_mask = 0x0a;  /* 0b01010 */
	TEST_EQ(menu_down_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "valid menu_down_action with mask");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 4, 0x0a);

	/* Invalid menu_down_action (blocked) */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 4;
	TEST_EQ(menu_down_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "invalid menu_down_action (blocked)");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 4, 0);

	/* Invalid menu_down_action (blocked by mask) */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 2;
	mock_state.disabled_item_mask = 0x1a;  /* 0b11010 */
	TEST_EQ(menu_down_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING,
		"invalid menu_down_action (blocked by mask)");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "   new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 2, 0x1a);

	/* menu_select_action with no item screen */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN1);
	TEST_EQ(menu_select_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "menu_select_action with no item screen");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "  new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN1, 0, 0, 0);

	/* Try to select target 0..3 */
	for (i = 0; i < 3; i++) {
		sprintf(test_name, "select target %d", i);
		reset_common_data();
		mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
		mock_state.selected_item = i;
		TEST_EQ(menu_select_action(ctx, &mock_state, &mock_new_screen),
			VBERROR_KEEP_LOOPING, test_name);
		TEST_EQ(mock_new_screen, MOCK_SCREEN_TARGET0 + i,
			"  new_screen");
		screen_state_eq(&mock_state, MOCK_SCREEN2, 0, i, 0);
	}

	/* Try to select no target item */
	reset_common_data();
	mock_state.screen = vb2_get_screen_info(MOCK_SCREEN2);
	mock_state.selected_item = 4;
	TEST_EQ(menu_select_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "select no target");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BLANK, "  new_screen");
	screen_state_eq(&mock_state, MOCK_SCREEN2, 0, 4, 0);

	/* menu_back_action */
	reset_common_data();
	TEST_EQ(menu_back_action(ctx, &mock_state, &mock_new_screen),
		VBERROR_KEEP_LOOPING, "menu_back_action");
	TEST_EQ(mock_new_screen, VB2_SCREEN_BACK, "  new_screen: back");
	screen_state_eq(&mock_state, VB2_SCREEN_BLANK, 0, 0, 0);

	/* lookup */

	VB2_DEBUG("...done.\n");
}

static void validate_selection_tests(void)
{
	VB2_DEBUG("Testing validate_selection...\n");

	VB2_DEBUG("...done.\n");
}

int main(void)
{
	shutdown_required_tests();
	input_action_tests();
	validate_selection_tests();

	return gTestSuccess ? 0 : 255;
}
