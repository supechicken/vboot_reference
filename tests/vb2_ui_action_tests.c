/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for UI related actions.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2ui.h"
#include "2ui_private.h"
#include "test_common.h"
#include "vboot_kernel.h"

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

/* Mock screen index for testing screen utility functions. */
#define MOCK_NO_SCREEN 0xef00
#define MOCK_SCREEN_BASE 0xef10
#define MOCK_SCREEN_MENU 0xef11
#define MOCK_SCREEN_TARGET0 0xef20
#define MOCK_SCREEN_TARGET1 0xef21
#define MOCK_SCREEN_TARGET2 0xef22
#define MOCK_SCREEN_INIT 0xef30
#define MOCK_SCREEN_ACTION 0xef31
#define MOCK_SCREEN_ALL_ACTION 0xef32
#define MOCK_SCREEN_ROOT 0xefff

/* Mock data */
struct display_call {
	const struct vb2_screen_info *screen;
	uint32_t locale_id;
	uint32_t selected_item;
	uint32_t disabled_item_mask;
};

static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;

static int mock_calls_until_shutdown;

static struct vb2_ui_context mock_ui_context;
static struct vb2_screen_state *mock_state;

static struct display_call mock_displayed[64];
static int mock_displayed_count;
static int mock_displayed_i;

static uint32_t mock_key[64];
static int mock_key_trusted[64];
static int mock_key_count;
static int mock_key_total;

static enum vb2_screen mock_get_screen_info_called;

static vb2_error_t mock_vbtlk_retval;
static uint32_t mock_vbtlk_expected_flag;

static int mock_allow_recovery;

static int mock_physical_presence_pressed;

static int mock_enable_dev_mode;

/* Mock actions */
static uint32_t mock_action_called;
static vb2_error_t mock_action_countdown(struct vb2_ui_context *ui)
{
	if (++mock_action_called >= 10)
		return VB2_SUCCESS;
	return VB2_REQUEST_UI_CONTINUE;
}

static vb2_error_t mock_action_change_screen(struct vb2_ui_context *ui)
{
	return vb2_ui_change_screen(ui, MOCK_SCREEN_BASE);
}

static vb2_error_t mock_action_base(struct vb2_ui_context *ui)
{
	mock_action_called++;
	return VB2_SUCCESS;
}

static int mock_action_flags;
static vb2_error_t mock_action_flag0(struct vb2_ui_context *ui)
{
	if ((1 << 0) & mock_action_flags)
		return VB2_SUCCESS;
	return VB2_REQUEST_UI_CONTINUE;
}

static vb2_error_t mock_action_flag1(struct vb2_ui_context *ui)
{
	if ((1 << 1) & mock_action_flags)
		return VB2_SUCCESS;
	return VB2_REQUEST_UI_CONTINUE;
}

static vb2_error_t mock_action_flag2(struct vb2_ui_context *ui)
{
	if ((1 << 2) & mock_action_flags)
		return VB2_SUCCESS;
	return VB2_REQUEST_UI_CONTINUE;
}

static vb2_error_t mock_action_flag3(struct vb2_ui_context *ui)
{
	if ((1 << 3) & mock_action_flags)
		return VB2_SUCCESS;
	return VB2_REQUEST_UI_CONTINUE;
}

/* Mock screens */
struct vb2_screen_info mock_screen_temp;
const struct vb2_screen_info *mock_screen_temp_ptr;
const struct vb2_menu_item mock_empty_menu[] = {};
const struct vb2_screen_info mock_screen_blank = {
	.id = VB2_SCREEN_BLANK,
	.name = "mock_screen_blank",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_screen_info mock_screen_base = {
	.id = MOCK_SCREEN_BASE,
	.name = "mock_screen_base: menuless screen",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_menu_item mock_screen_menu_items[] = {
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
		.action = mock_action_base,
	},
	{
		.text = "option 4 (no target)",
	},
};
const struct vb2_screen_info mock_screen_menu = {
	.id = MOCK_SCREEN_MENU,
	.name = "mock_screen_menu: screen with 4 options",
	.num_items = ARRAY_SIZE(mock_screen_menu_items),
	.items = mock_screen_menu_items,
};
const struct vb2_screen_info mock_screen_target0 = {
	.id = MOCK_SCREEN_TARGET0,
	.name = "mock_screen_target0",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_screen_info mock_screen_target1 = {
	.id = MOCK_SCREEN_TARGET1,
	.name = "mock_screen_target1",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_screen_info mock_screen_target2 = {
	.id = MOCK_SCREEN_TARGET2,
	.name = "mock_screen_target2",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_screen_info mock_screen_init = {
	.id = MOCK_SCREEN_INIT,
	.name = "mock_screen_init",
	.init = mock_action_base,
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_screen_info mock_screen_action = {
	.id = MOCK_SCREEN_ACTION,
	.name = "mock_screen_action",
	.action = mock_action_countdown,
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};
const struct vb2_menu_item mock_screen_all_action_items[] = {
	{
		.text = "all_action_screen_option",
		.action = mock_action_flag2,
	},
};
const struct vb2_screen_info mock_screen_all_action = {
	.id = MOCK_SCREEN_ALL_ACTION,
	.name = "mock_screen_all_action",
	.init = mock_action_flag0,
	.action = mock_action_flag1,
	.num_items = ARRAY_SIZE(mock_screen_all_action_items),
	.items = mock_screen_all_action_items,
};
const struct vb2_screen_info mock_screen_root = {
	.id = MOCK_SCREEN_ROOT,
	.name = "mock_screen_root",
	.num_items = ARRAY_SIZE(mock_empty_menu),
	.items = mock_empty_menu,
};

static void screen_state_eq(const struct vb2_screen_state *state,
			    enum vb2_screen screen,
			    uint32_t selected_item,
			    uint32_t disabled_item_mask)
{
	if (screen != MOCK_IGNORE) {
		if (state->screen == NULL)
			TEST_TRUE(0, "  state.screen does not exist");
		else
			TEST_EQ(state->screen->id, screen, "  state.screen");
	}
	if (selected_item != MOCK_IGNORE)
		TEST_EQ(state->selected_item,
			selected_item, "  state.selected_item");
	if (disabled_item_mask != MOCK_IGNORE)
		TEST_EQ(state->disabled_item_mask,
			disabled_item_mask, "  state.disabled_item_mask");
}

static void add_mock_key(uint32_t press, int trusted)
{
	if (mock_key_total >= ARRAY_SIZE(mock_key) ||
	    mock_key_total >= ARRAY_SIZE(mock_key_trusted)) {
		TEST_TRUE(0, "  mock_key ran out of entries!");
		return;
	}

	mock_key[mock_key_total] = press;
	mock_key_trusted[mock_key_total] = trusted;
	mock_key_total++;
}

static void add_mock_keypress(uint32_t press)
{
	add_mock_key(press, 0);
}


static void set_mock_vbtlk(vb2_error_t retval, uint32_t get_info_flags)
{
	mock_vbtlk_retval = retval;
	mock_vbtlk_expected_flag = get_info_flags;
}

static void displayed_eq(const char *text,
			 enum vb2_screen screen,
			 uint32_t locale_id,
			 uint32_t selected_item,
			 uint32_t disabled_item_mask)
{
	char text_buf[256];

	if (mock_displayed_i >= mock_displayed_count) {
		sprintf(text_buf, "  missing screen %s", text);
		TEST_TRUE(0, text_buf);
		return;
	}

	if (screen != MOCK_IGNORE) {
		sprintf(text_buf, "  screen of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].screen->id, screen,
			text_buf);
	}
	if (locale_id != MOCK_IGNORE) {
		sprintf(text_buf, "  locale_id of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].locale_id, locale_id,
			text_buf);
	}
	if (selected_item != MOCK_IGNORE) {
		sprintf(text_buf, "  selected_item of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].selected_item,
			selected_item, text_buf);
	}
	if (disabled_item_mask != MOCK_IGNORE) {
		sprintf(text_buf, "  disabled_item_mask of %s", text);
		TEST_EQ(mock_displayed[mock_displayed_i].disabled_item_mask,
			disabled_item_mask, text_buf);
	}
	mock_displayed_i++;
}

static void displayed_no_extra(void)
{
	if (mock_displayed_i == 0)
		TEST_EQ(mock_displayed_count, 0, "  no screen");
	else
		TEST_EQ(mock_displayed_count, mock_displayed_i,
			"  no extra screens");
}

/* Reset mock data (for use before each test) */
static void reset_common_data(void)
{
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	memset(&gbb, 0, sizeof(gbb));

	vb2_nv_init(ctx);

	sd = vb2_get_sd(ctx);

	/* For check_shutdown_request */
	mock_calls_until_shutdown = 10;

	/* For try_recovery_action */
	invalid_disk_last = -1;

	/* Reset mock_screen_temp for test by test temporary screen_info */
	mock_screen_temp = (struct vb2_screen_info){
	      .id = MOCK_NO_SCREEN,
	      .name = "mock_screen_temp",
	      .num_items = ARRAY_SIZE(mock_empty_menu),
	      .items = mock_empty_menu,
	};
	mock_screen_temp_ptr = &mock_screen_temp;

	/* Mock ui_context based on mock screens */
	mock_ui_context = (struct vb2_ui_context){
		.ctx = ctx,
		.root_screen = &mock_screen_root,
		.state = (struct vb2_screen_state){
			.screen = mock_screen_temp_ptr,
			.selected_item = 0,
			.disabled_item_mask = 0,
		},
		.locale_id = 0,
		.key = 0,
		.key_trusted = 0,
	};
	mock_state = &mock_ui_context.state;

	/* For vb2ex_display_ui */
	memset(mock_displayed, 0, sizeof(mock_displayed));
	mock_displayed_count = 0;
	mock_displayed_i = 0;

	/* For VbExKeyboardRead */
	memset(mock_key, 0, sizeof(mock_key));
	memset(mock_key_trusted, 0, sizeof(mock_key_trusted));
	mock_key_count = 0;
	mock_key_total = 0;

	/* For mock actions */
	mock_action_called = 0;
	mock_action_flags = 0;

	/* For chagen_screen and vb2_get_screen_info */
	mock_get_screen_info_called = 0;

	/* For VbTryLoadKernel */
	mock_vbtlk_retval = VB2_ERROR_MOCK;
	mock_vbtlk_expected_flag = MOCK_IGNORE;

	/* For vb2_allow_recovery */
	mock_allow_recovery = 0;

	/* For vb2ex_physical_presence_pressed */
	mock_physical_presence_pressed = 0;

	/* For vb2_enable_developer_mode */
	mock_enable_dev_mode = 0;
}

/* Mock functions */
struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_calls_until_shutdown < 0)  /* Never request shutdown */
		return 0;
	if (mock_calls_until_shutdown == 0)
		return 1;
	mock_calls_until_shutdown--;

	return 0;
}

const struct vb2_screen_info *vb2_get_screen_info(enum vb2_screen screen)
{
	mock_get_screen_info_called++;

	switch ((int)screen) {
	case VB2_SCREEN_BLANK:
		return &mock_screen_blank;
	case MOCK_SCREEN_BASE:
		return &mock_screen_base;
	case MOCK_SCREEN_MENU:
		return &mock_screen_menu;
	case MOCK_SCREEN_TARGET0:
		return &mock_screen_target0;
	case MOCK_SCREEN_TARGET1:
		return &mock_screen_target1;
	case MOCK_SCREEN_TARGET2:
		return &mock_screen_target2;
	case MOCK_SCREEN_INIT:
		return &mock_screen_init;
	case MOCK_SCREEN_ACTION:
		return &mock_screen_action;
	case MOCK_SCREEN_ALL_ACTION:
		return &mock_screen_all_action;
	case MOCK_NO_SCREEN:
		return NULL;
	default:
		mock_screen_temp.id = screen;
		return mock_screen_temp_ptr;
	}
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
		TEST_TRUE(0, "  mock vb2ex_display_ui ran out of entries!");
		return VB2_ERROR_MOCK;
	}

	mock_displayed[mock_displayed_count] = (struct display_call){
		.screen = vb2_get_screen_info(screen),
		.locale_id = locale_id,
		.selected_item = selected_item,
		.disabled_item_mask = disabled_item_mask,
	};
	mock_displayed_count++;

	return VB2_SUCCESS;
}

uint32_t VbExKeyboardRead(void)
{
	return VbExKeyboardReadWithFlags(NULL);
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *key_flags)
{
	if (mock_key_count < mock_key_total) {
		if (key_flags != NULL) {
			if (mock_key_trusted[mock_key_count])
				*key_flags = VB_KEY_FLAG_TRUSTED_KEYBOARD;
			else
				*key_flags = 0;
		}
		return mock_key[mock_key_count++];
	}

	return 0;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	TEST_EQ(mock_vbtlk_expected_flag, get_info_flags,
		"  unexpected get_info_flags");

	return mock_vbtlk_retval;
}

int vb2_allow_recovery(struct vb2_context *c)
{
	return mock_allow_recovery;
}

int vb2ex_physical_presence_pressed(void)
{
	return mock_physical_presence_pressed;
}

void vb2_enable_developer_mode(struct vb2_context *c)
{
	mock_enable_dev_mode = 1;
}

/* Tests */
static void menu_up_action_tests(void)
{
	VB2_DEBUG("Testing menu_up_action...\n");

	/* Valid action */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_ui_context.key = VB_KEY_UP;
	TEST_EQ(menu_up_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"valid action");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 1, MOCK_IGNORE);

	/* Valid action with mask */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_state->disabled_item_mask = 0x0a;  /* 0b01010 */
	mock_ui_context.key = VB_KEY_UP;
	TEST_EQ(menu_up_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"valid action with mask");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 0, MOCK_IGNORE);

	/* Invalid action (blocked) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 0;
	mock_ui_context.key = VB_KEY_UP;
	TEST_EQ(menu_up_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid action (blocked)");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 0, MOCK_IGNORE);

	/* Invalid action (blocked by mask) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_state->disabled_item_mask = 0x0b;  /* 0b01011 */
	mock_ui_context.key = VB_KEY_UP;
	TEST_EQ(menu_up_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid action (blocked by mask)");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);

	/* Ignore volume-up when not DETACHABLE */
	if (!DETACHABLE) {
		reset_common_data();
		mock_state->screen = &mock_screen_menu;
		mock_state->selected_item = 2;
		mock_ui_context.key = VB_BUTTON_VOL_UP_SHORT_PRESS;
		TEST_EQ(menu_up_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"ignore volume-up when not DETACHABLE");
		screen_state_eq(mock_state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
	}

	VB2_DEBUG("...done.\n");
}

static void menu_down_action_tests(void)
{
	VB2_DEBUG("Testing menu_down_action...\n");

	/* Valid action */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_ui_context.key = VB_KEY_DOWN;
	TEST_EQ(menu_down_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"valid action");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 3, MOCK_IGNORE);

	/* Valid action with mask */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_state->disabled_item_mask = 0x0a;  /* 0b01010 */
	mock_ui_context.key = VB_KEY_DOWN;
	TEST_EQ(menu_down_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"valid action with mask");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);

	/* Invalid action (blocked) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 4;
	mock_ui_context.key = VB_KEY_DOWN;
	TEST_EQ(menu_down_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid action (blocked)");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);

	/* Invalid action (blocked by mask) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 2;
	mock_state->disabled_item_mask = 0x1a;  /* 0b11010 */
	mock_ui_context.key = VB_KEY_DOWN;
	TEST_EQ(menu_down_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid action (blocked by mask)");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);

	/* Ignore volume-down when not DETACHABLE */
	if (!DETACHABLE) {
		reset_common_data();
		mock_state->screen = &mock_screen_menu;
		mock_state->selected_item = 2;
		mock_ui_context.key = VB_BUTTON_VOL_DOWN_SHORT_PRESS;
		TEST_EQ(menu_down_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"ignore volume-down when not DETACHABLE");
		screen_state_eq(mock_state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
	}

	VB2_DEBUG("...done.\n");
}

static void menu_select_action_tests(void)
{
	int i, target_id;
	char test_name[256];

	VB2_DEBUG("Testing menu_select_action...\n");

	/* select action with no item screen */
	reset_common_data();
	mock_state->screen = &mock_screen_base;
	mock_ui_context.key = VB_KEY_ENTER;
	TEST_EQ(vb2_ui_menu_select_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"menu_select_action with no item screen");
	screen_state_eq(mock_state, MOCK_SCREEN_BASE, 0, MOCK_IGNORE);

	/* Try to select target 0..2 */
	for (i = 0; i <= 2; i++) {
		sprintf(test_name, "select target %d", i);
		target_id = MOCK_SCREEN_TARGET0 + i;
		reset_common_data();
		mock_state->screen = &mock_screen_menu;
		mock_state->selected_item = i;
		mock_ui_context.key = VB_KEY_ENTER;
		TEST_EQ(vb2_ui_menu_select_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE, test_name);
		screen_state_eq(mock_state, target_id, 0, MOCK_IGNORE);
	}

	/* Try to select target with action (target 3) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 3;
	mock_ui_context.key = VB_KEY_ENTER;
	TEST_EQ(vb2_ui_menu_select_action(&mock_ui_context),
		VB2_SUCCESS, "select target with action");
	TEST_EQ(mock_action_called, 1, "  action called once");

	/* Try to select no target item (target 4) */
	reset_common_data();
	mock_state->screen = &mock_screen_menu;
	mock_state->selected_item = 4;
	mock_ui_context.key = VB_KEY_ENTER;
	TEST_EQ(vb2_ui_menu_select_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"select no target");
	screen_state_eq(mock_state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);

	/* Ignore power button short press when not DETACHABLE */
	if (!DETACHABLE) {
		reset_common_data();
		mock_state->screen = &mock_screen_menu;
		mock_state->selected_item = 1;
		mock_ui_context.key = VB_BUTTON_POWER_SHORT_PRESS;
		TEST_EQ(vb2_ui_menu_select_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"ignore power button short press when not DETACHABLE");
		screen_state_eq(mock_state, MOCK_SCREEN_MENU, 1, MOCK_IGNORE);
	}

	VB2_DEBUG("...done.\n");
}

static void vb2_ui_back_action_tests(void)
{
	VB2_DEBUG("Testing vb2_ui_back_action...\n");

	/* back to root screen */
	reset_common_data();
	mock_ui_context.key = VB_KEY_ESC;
	TEST_EQ(vb2_ui_back_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"back to root screen");
	screen_state_eq(mock_state, MOCK_SCREEN_ROOT, MOCK_IGNORE, MOCK_IGNORE);

	VB2_DEBUG("...done.\n");
}

static void ctrl_d_action_tests(void)
{
	VB2_DEBUG("Testing ctrl_d_action...\n");

	/* Allow recovery: to_dev */
	reset_common_data();
	mock_allow_recovery = 1;
	TEST_EQ(ctrl_d_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"allow recovery, to_dev");
	screen_state_eq(mock_state, VB2_SCREEN_RECOVERY_TO_DEV,
			MOCK_IGNORE, MOCK_IGNORE);

	/* Recovery not allowed, do nothing */
	reset_common_data();
	TEST_EQ(ctrl_d_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"recovery not allowed");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	VB2_DEBUG("...done.\n");
}

static void try_recovery_action_tests(void)
{
	VB2_DEBUG("Testing try recovery action...\n");

	/* Success on the first try */
	reset_common_data();
	set_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_SUCCESS,
		"success on the first try");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	/* No disk found on the first try */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"no disk found on the first try");
	screen_state_eq(mock_state, VB2_SCREEN_RECOVERY_SELECT,
			MOCK_IGNORE, MOCK_IGNORE);

	/* Invalid disk on the first try */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid on the first try");
	screen_state_eq(mock_state, VB2_SCREEN_RECOVERY_INVALID,
			MOCK_IGNORE, MOCK_IGNORE);

	/* Success, last == 0 */
	reset_common_data();
	set_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 0;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_SUCCESS,
		"success, last == 0");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	/* No disk found, last == 0 */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 0;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"no disk found, last == 0");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	/* Invalid disk, last == 0 */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 0;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid, last == 0");
	screen_state_eq(mock_state, VB2_SCREEN_RECOVERY_INVALID,
			MOCK_IGNORE, MOCK_IGNORE);

	/* Success, last == 1 */
	reset_common_data();
	set_mock_vbtlk(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 1;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_SUCCESS,
		"success, last == 1");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	/* No disk found, last == 1 */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_LK_NO_DISK_FOUND, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 1;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"no disk found, last == 1");
	screen_state_eq(mock_state, VB2_SCREEN_RECOVERY_SELECT,
			MOCK_IGNORE, MOCK_IGNORE);

	/* Invalid disk, last == 1 */
	reset_common_data();
	set_mock_vbtlk(VB2_ERROR_MOCK, VB_DISK_FLAG_REMOVABLE);
	invalid_disk_last = 1;
	TEST_EQ(try_recovery_action(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"invalid, last == 1");
	TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");

	VB2_DEBUG("...done.\n");
}

static void recovery_to_dev_init_tests(void)
{
	VB2_DEBUG("Testing recovery_to_dev_init...\n");

	/* Dev mode already enabled: back */
	reset_common_data();
	sd->flags |= VB2_SD_FLAG_DEV_MODE_ENABLED;
	TEST_EQ(recovery_to_dev_init(&mock_ui_context), VB2_REQUEST_UI_CONTINUE,
		"dev mode already enabled: back");
	screen_state_eq(mock_state, MOCK_SCREEN_ROOT, MOCK_IGNORE, MOCK_IGNORE);

	/* Press button stuck: back */
	if (!PHYSICAL_PRESENCE_KEYBOARD) {
		reset_common_data();
		mock_physical_presence_pressed = 1;
		TEST_EQ(recovery_to_dev_init(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"press button stuck: back");
		screen_state_eq(mock_state, MOCK_SCREEN_ROOT, MOCK_IGNORE,
				MOCK_IGNORE);
	}

	/* Disable confirm for other physical presence types */
	if (!PHYSICAL_PRESENCE_KEYBOARD) {
		reset_common_data();
		TEST_EQ(recovery_to_dev_init(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"disable confirm for other physical presence type");
		TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");
		screen_state_eq(mock_state, MOCK_IGNORE, MOCK_IGNORE, 1);
	}

	/* Do nothing otherwise */
	if (PHYSICAL_PRESENCE_KEYBOARD) {
		reset_common_data();
		TEST_EQ(recovery_to_dev_init(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE, "do nothing otherwise");
		TEST_EQ(mock_get_screen_info_called, 0, "  no change_screen");
		screen_state_eq(mock_state, MOCK_IGNORE, MOCK_IGNORE, 0);
	}

	VB2_DEBUG("...done.\n");
}

static void vb2_ui_recovery_to_dev_action_tests(void)
{
	VB2_DEBUG("Testing vb2_ui_recovery_to_dev_action...\n");

	/* Action need RECOVERY_TO_DEV screen */
	reset_common_data();
	mock_allow_recovery = 1;
	mock_ui_context.key = VB_KEY_ENTER;
	mock_ui_context.key_trusted = 1;
	TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"action need RECOVERY_TO_DEV screen");
	TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");

	/* Space means cancel dev mode transition */
	reset_common_data();
	mock_allow_recovery = 1;
	mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
	mock_ui_context.key = ' ';
	TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"space means cancel dev mode transition");
	TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");
	screen_state_eq(mock_state, MOCK_SCREEN_ROOT, MOCK_IGNORE, MOCK_IGNORE);

	/* Physical presence button */
	if (!PHYSICAL_PRESENCE_KEYBOARD) {
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_physical_presence_pressed = 1;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"physical presence button pressed, await");
	      	TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");
		mock_physical_presence_pressed = 0;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_REBOOT_EC_TO_RO,
			"physical presence button released");
	      	TEST_EQ(mock_enable_dev_mode, 1, "  dev mode enabled");
	}

	/* Physical presence keyboard */
	if (PHYSICAL_PRESENCE_KEYBOARD) {
		/* VB_KEY_ENTER */
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_ui_context.key = VB_KEY_ENTER;
		mock_ui_context.key_trusted = 1;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_REBOOT_EC_TO_RO,
			"physical presence keyboard, VB_KEY_ENTER");
		TEST_EQ(mock_enable_dev_mode, 1, "  dev mode enabled");

		/* VB_BUTTON_POWER_SHORT_PRESS */
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_ui_context.key = VB_BUTTON_POWER_SHORT_PRESS;
		mock_ui_context.key_trusted = 1;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_REBOOT_EC_TO_RO,
			"physical presence keyboard,"
			" VB_BUTTON_POWER_SHORT_PRESS");
		TEST_EQ(mock_enable_dev_mode, 1, "  dev mode enabled");

		/* Not a confirmation key */
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_ui_context.key = 'A';
		mock_ui_context.key_trusted = 1;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"not a confirmation key");
		TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");

		/* Not a trusted key */
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_ui_context.key = VB_KEY_ENTER;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"VB_KEY_ENTER not trusted");
		TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");
		reset_common_data();
		mock_allow_recovery = 1;
		mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
		mock_ui_context.key = VB_BUTTON_POWER_SHORT_PRESS;
		TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
			VB2_REQUEST_UI_CONTINUE,
			"VB_BUTTON_POWER_SHORT_PRESS not trusted");
		TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");
	}

	/* Sanity check */
	reset_common_data();
	mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
	mock_ui_context.key = VB_KEY_ENTER;
	mock_ui_context.key_trusted = 1;
	TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"sanity check: recovery not allowed");
	TEST_EQ(mock_enable_dev_mode, 0, "  dev mode not enabled");
	reset_common_data();
	sd->flags |= VB2_SD_FLAG_DEV_MODE_ENABLED;
	mock_allow_recovery = 1;
	mock_screen_temp.id = VB2_SCREEN_RECOVERY_TO_DEV;
	mock_ui_context.key = VB_KEY_ENTER;
	mock_ui_context.key_trusted = 1;
	TEST_EQ(vb2_ui_recovery_to_dev_action(&mock_ui_context),
		VB2_REQUEST_UI_CONTINUE,
		"sanity check: dev mode already enabled");
	TEST_EQ(mock_enable_dev_mode, 0, "  no enable dev mode call");

	VB2_DEBUG("...done.\n");
}

static void ui_loop_tests(void)
{
	int i;
	const char *action_interfere_test_names[] = {
		"action interfere test: root screen init",
		"action interfere test: screen action",
		"action interfere test: target option hooked",
		"action interfere test: global action",
	};

	VB2_DEBUG("Testing ui_loop...\n");

	/* Die if no root screen */
	reset_common_data();
	TEST_ABORT(ui_loop(ctx, MOCK_NO_SCREEN, NULL),
		   "die if no root screen");
	displayed_no_extra();

	/* Shutdown if requested */
	reset_common_data();
	TEST_EQ(ui_loop(ctx, MOCK_SCREEN_BASE, NULL),
		VB2_REQUEST_SHUTDOWN, "shutdown if requested");
	TEST_EQ(mock_calls_until_shutdown, 0, "  used up shutdown request");
	displayed_eq("mock_screen_base", MOCK_SCREEN_BASE, MOCK_IGNORE,
		     MOCK_IGNORE, MOCK_IGNORE);
	displayed_no_extra();

	/* Root screen init */
	reset_common_data();
	TEST_EQ(ui_loop(ctx, MOCK_SCREEN_INIT, NULL),
		VB2_SUCCESS, "root screen init");
	TEST_EQ(mock_action_called, 1, "  action called");

	/* Screen action */
	reset_common_data();
	mock_calls_until_shutdown = -1;
	TEST_EQ(ui_loop(ctx, MOCK_SCREEN_ACTION, NULL),
		VB2_SUCCESS, "screen action");
	TEST_EQ(mock_action_called, 10, "  action called");

	/* Global action */
	reset_common_data();
	mock_calls_until_shutdown = -1;
	TEST_EQ(ui_loop(ctx, VB2_SCREEN_BLANK, mock_action_countdown),
		VB2_SUCCESS, "global action");
	TEST_EQ(mock_action_called, 10, "  action called");

	/* Global action can change screen */
	reset_common_data();
	TEST_EQ(ui_loop(ctx, VB2_SCREEN_BLANK, mock_action_change_screen),
		VB2_REQUEST_SHUTDOWN, "global action can change screen");
	displayed_eq("pass", MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
		     MOCK_IGNORE);
	displayed_eq("change to mock_screen_base", MOCK_IGNORE, MOCK_IGNORE,
		     MOCK_IGNORE, MOCK_IGNORE);

	/* All actions does not interfere each other. */
	for (i = 0; i <= 3; i++) {
		reset_common_data();
		add_mock_keypress(VB_KEY_ENTER);
		mock_calls_until_shutdown = -1;
		mock_action_flags |= (1 << i);
		TEST_EQ(ui_loop(ctx, MOCK_SCREEN_ALL_ACTION, mock_action_flag3),
			VB2_SUCCESS, action_interfere_test_names[i]);
	}

	/* KEY_UP, KEY_DOWN, and KEY_ENTER navigation */
	reset_common_data();
	add_mock_keypress(VB_KEY_UP);  /* (blocked) */
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);
	add_mock_keypress(VB_KEY_DOWN);  /* (blocked) */
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_UP);
	add_mock_keypress(VB_KEY_ENTER);
	TEST_EQ(ui_loop(ctx, MOCK_SCREEN_MENU, NULL),
		VB2_REQUEST_SHUTDOWN, "KEY_UP, KEY_DOWN, and KEY_ENTER");
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 0,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 1,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 2,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 3,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 4,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 3,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE, 2,
		     MOCK_IGNORE);
	displayed_eq("mock_screen_target_2", MOCK_SCREEN_TARGET2, MOCK_IGNORE,
		     MOCK_IGNORE, MOCK_IGNORE);
	displayed_no_extra();

	/* For DETACHABLE */
	if (DETACHABLE) {
		reset_common_data();
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_DOWN_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_VOL_UP_SHORT_PRESS);
		add_mock_keypress(VB_BUTTON_POWER_SHORT_PRESS);
		TEST_EQ(ui_loop(ctx, MOCK_SCREEN_MENU, NULL),
			VB2_REQUEST_SHUTDOWN, "DETACHABLE");
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     0, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     1, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     2, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     3, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     4, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     3, MOCK_IGNORE);
		displayed_eq("mock_screen_menu", MOCK_SCREEN_MENU, MOCK_IGNORE,
			     2, MOCK_IGNORE);
		displayed_eq("mock_screen_target_2", MOCK_SCREEN_TARGET2,
			     MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE);
		displayed_no_extra();
	}

	VB2_DEBUG("...done.\n");
}

int main(void)
{
	/* Input actions */
	menu_up_action_tests();
	menu_down_action_tests();
	menu_select_action_tests();
	vb2_ui_back_action_tests();
	ctrl_d_action_tests();

	/* Global actions */
	try_recovery_action_tests();

	/* Recovery to dev mode */
	recovery_to_dev_init_tests();
	vb2_ui_recovery_to_dev_action_tests();

	/* Core UI loop */
	ui_loop_tests();

	return gTestSuccess ? 0 : 255;
}
