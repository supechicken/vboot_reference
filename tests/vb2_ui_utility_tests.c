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

/* Mocks for testing screen utility functions. */
const struct vb2_menu_item mock_screen1_items[] = {};
struct vb2_screen_info mock_screen1 =
{
	.screen = MOCK_SCREEN1,
	.name = "mock_screen1: menuless screen",
	.size = ARRAY_SIZE(mock_screen1_items),
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
	.screen = MOCK_SCREEN2,
	.name = "mock_screen2: menu screen",
	.size = ARRAY_SIZE(mock_screen2_items),
	.items = mock_screen2_items,
};

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

	/* For screen related utility functions */
	current_screen = NULL;
	selected_item = 0;
	disabled_item_mask = 0;
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

static void screen_related_tests(void)
{
	VB2_DEBUG("Testing screen related utility functions...\n");

	/* Change to a menuless screen */
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN1);
	TEST_TRUE(current_screen == &mock_screen1,
		  "change_screen to a menuless screen");
	TEST_EQ(selected_item, 0, "  selected item");
	TEST_EQ(disabled_item_mask, 0, "  disabled_item_mask");
	displayed_eq("mock screen1", MOCK_SCREEN1, MOCK_FIXED, 0, 0);
	displayed_no_extra();

	/* Change to a screen with menus */
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen ==  &mock_screen2,
		  "change_screen to screen with menu");
	TEST_EQ(selected_item, 0, "  selected item");
	TEST_EQ(disabled_item_mask, 0, "  disabled_item_mask");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, 0, 0);
	displayed_no_extra();

	/* New screen does not exist */
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN_TARGET1);  /* Does not exist */
	TEST_TRUE(current_screen == NULL, "new screen does not exist");
	displayed_no_extra();

	/* Change screen will reset selected_item */
	reset_common_data();
	selected_item = 2;
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2,
		  "change screen will reset selected_item");
	TEST_EQ(selected_item, 0, "  selected_item reset to 0");

	/* Change screen with disabled mask - 1 */
	reset_common_data();
	disabled_item_mask = 0x17;  /* 0b10111 */
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2,
		  "change screen with mask #1: mask=0b10111");
	TEST_EQ(selected_item, 3, "  selected_item == 3");
	TEST_EQ(disabled_item_mask, 0x17, "  disabled_item_mask == 0b10111");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, 3, 0x17);

	/* Change screen with disabled mask - 2 */
	reset_common_data();
	selected_item = 2;
	disabled_item_mask = 0x16;  /* 0b10110 */
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2,
		  "change screen with mask #2: mask=0b10110");
	TEST_EQ(selected_item, 0, "  selected_item == 0");
	TEST_EQ(disabled_item_mask, 0x16, "  disabled_item_mask == 0b10110");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, 0, 0x16);

	/* Change screen with disabled mask - 2 */
	reset_common_data();
	selected_item = 0;
	disabled_item_mask = 0x1f;  /* 0b11111 */
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2,
		  "change screen with mask #3: mask=0b11111");
	TEST_EQ(selected_item, 0, "  selected_item == 0");
	TEST_EQ(disabled_item_mask, 0x1f, "  disabled_item_mask == 0b11111");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, 0, 0x1f);

	/* Change screen do nothing with size is 0 */
	reset_common_data();
	disabled_item_mask = 0x17;  /* 0b10111 */
	change_screen(ctx, MOCK_SCREEN1);
	TEST_TRUE(current_screen == &mock_screen1,
		  "change screen with mask while size is 0");
	TEST_EQ(selected_item, 0, "  selected_item == 0");
	TEST_EQ(disabled_item_mask, 0x17, "  disabled_item_mask == 0b10111");
	displayed_eq("mock screen1", MOCK_SCREEN1, MOCK_FIXED, 0, MOCK_FIXED);

	/* Update selection: up */
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2, "update selection: up");
	selected_item = 2;
	update_selection(0);
	TEST_TRUE(current_screen == &mock_screen2, "  step #1: move up");
	TEST_EQ(selected_item, 1, "  selected_item == 1");
	update_selection(0);
	TEST_TRUE(current_screen == &mock_screen2, "  step #2: move up");
	TEST_EQ(selected_item, 0, "  selected_item == 0");
	update_selection(0);
	TEST_TRUE(current_screen == &mock_screen2, "  step #3: move up");
	TEST_EQ(selected_item, 0, "  selected_item == 0 (blocked)");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, MOCK_FIXED, 0);
	displayed_eq("mock screen2 #1", MOCK_SCREEN2, MOCK_FIXED, 1, 0);
	displayed_eq("mock screen2 #2", MOCK_SCREEN2, MOCK_FIXED, 0, 0);
	displayed_eq("mock screen2 #3", MOCK_SCREEN2, MOCK_FIXED, 0, 0);
	displayed_no_extra();

	/* Update selection: down */
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN2);
	TEST_TRUE(current_screen == &mock_screen2, "update selection: down");
	selected_item = 2;
	update_selection(1);
	TEST_TRUE(current_screen == &mock_screen2, "  step #1: move down");
	TEST_EQ(selected_item, 3, "  selected_item == 3");
	update_selection(1);
	TEST_TRUE(current_screen == &mock_screen2, "  step #2: move down");
	TEST_EQ(selected_item, 4, "  selected_item == 4");
	update_selection(1);
	TEST_TRUE(current_screen == &mock_screen2, "  step #3: move down");
	TEST_EQ(selected_item, 4, "  selected_item == 4 (blocked)");
	displayed_eq("mock screen2", MOCK_SCREEN2, MOCK_FIXED, MOCK_FIXED, 0);
	displayed_eq("mock screen2 #1", MOCK_SCREEN2, MOCK_FIXED, 3, 0);
	displayed_eq("mock screen2 #2", MOCK_SCREEN2, MOCK_FIXED, 4, 0);
	displayed_eq("mock screen2 #3", MOCK_SCREEN2, MOCK_FIXED, 4, 0);
	displayed_no_extra();

	/* Update selection: mixed directions */
	{
		int i;
		const int directions[] = {0, 1, 1, 1, 0, 1, 1, 1, 0, 1};
		const int selections[] = {0, 1, 2, 3, 2, 3, 4, 4, 3, 4};
		const char *direction_text[] = {"up", "down"};
		const char *blocked_text[] = {"", " (blocked)"};
		char text_buf[256];
		int prev_selection = 0;

		reset_common_data();
		change_screen(ctx, MOCK_SCREEN2);
		TEST_TRUE(current_screen == &mock_screen2, "mixed directions");
		for (i = 0; i < ARRAY_SIZE(directions); i++) {
			update_selection(directions[i]);
			sprintf(text_buf, "  step #%d: move %s", i + 1,
				direction_text[directions[i]]);
			TEST_TRUE(current_screen == &mock_screen2, text_buf);
			sprintf(text_buf, "  selected_item == %d%s",
				selections[i],
				blocked_text[selections[i] == prev_selection]);
			TEST_EQ(selected_item, selections[i], text_buf);
			prev_selection = selections[i];
		}
		displayed_eq("mock_screen2", MOCK_SCREEN2, MOCK_FIXED, 0, 0);
		for (i = 0; i < ARRAY_SIZE(directions); i++) {
			sprintf(text_buf, "mock screen2 #%d", i + 1);
			displayed_eq(text_buf, MOCK_SCREEN2, MOCK_FIXED,
				     selections[i], 0);
		}
		displayed_no_extra();
	}

	/* Update selection: up with mask */
	VB2_DEBUG("move up with mask\n");
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN2);
	selected_item = 4;
	disabled_item_mask = 0x0a;  /* 0b01010 */
	update_selection(0);
	TEST_EQ(selected_item, 2, "  from 4 to 2 with mask 0b01010");
	selected_item = 3;
	disabled_item_mask = 0x04;  /* 0b00100 */
	update_selection(0);
	TEST_EQ(selected_item, 1, "  from 3 to 1 with mask 0b00100");
	selected_item = 4;
	disabled_item_mask = 0x0e;  /* 0b01110 */
	update_selection(0);
	TEST_EQ(selected_item, 0, "  from 4 to 0 with mask 0b01110");
	selected_item = 4;
	disabled_item_mask = 0x0f;  /* 0b01111 */
	update_selection(0);
	TEST_EQ(selected_item, 4, "  stay at 4 with mask 0b01111");
	selected_item = 0;
	disabled_item_mask = 0x0e;  /* 0b01110 */
	TEST_EQ(selected_item, 0, "  stay at 0 (blocked) with mask 0b01110");

	/* Update selection: down with mask */
	VB2_DEBUG("move down with mask\n");
	reset_common_data();
	change_screen(ctx, MOCK_SCREEN2);
	selected_item = 0;
	disabled_item_mask = 0x0a;  /* 0b01010 */
	update_selection(1);
	TEST_EQ(selected_item, 2, "  from 0 to 2 with mask 0b01010");
	selected_item = 1;
	disabled_item_mask = 0x04;  /* 0b00100 */
	update_selection(1);
	TEST_EQ(selected_item, 3, "  from 1 to 3 with mask 0b00100");
	selected_item = 0;
	disabled_item_mask = 0x0e;  /* 0b01110 */
	update_selection(1);
	TEST_EQ(selected_item, 4, "  from 0 to 4 with mask 0b01110");
	selected_item = 0;
	disabled_item_mask = 0x1e;  /* 0b11110 */
	update_selection(1);
	TEST_EQ(selected_item, 0, "  stay at 0 with mask 0b11110");
	selected_item = 4;
	disabled_item_mask = 0x0e;  /* 0b01110 */
	TEST_EQ(selected_item, 4, "  stay at 4 (blocked) with mask 0b01110");

	VB2_DEBUG("...done.\n");
}

int main(void)
{
	shutdown_required_tests();
	screen_related_tests();

	return gTestSuccess ? 0 : 255;
}
