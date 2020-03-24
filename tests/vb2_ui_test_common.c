/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common mock functions and helpers of UI tests.
 */

#include "2common.h"
#include "2ui.h"
#include "test_common.h"
#include "vb2_ui_test_common.h"

/*****************************************************************************/
/* Mock vb2ex_display_ui */

static struct vb2_screen_state mock_displayed[64];
static uint32_t mock_displayed_count = 0;
static uint32_t mock_displayed_i;

/* Mock functions */
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

	mock_displayed[mock_displayed_count] = (struct vb2_screen_state){
		.screen = vb2_get_screen_info(screen),
		.locale_id = locale_id,
		.selected_item = selected_item,
		.disabled_item_mask = disabled_item_mask,
	};
	mock_displayed_count++;

	return VB2_SUCCESS;
}

/* Help functions */
void displayed_eq(const char *text,
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

void displayed_no_extra(void)
{
	if (mock_displayed_i == 0)
		TEST_EQ(mock_displayed_count, 0, "  no screen");
	else
		TEST_EQ(mock_displayed_count, mock_displayed_i,
			"  no extra screens");
}

/*****************************************************************************/
/* Initialization */

void reset_ui_common_data(void)
{
	/* For vb2ex_display_ui */
	memset(mock_displayed, 0, sizeof(mock_displayed));
	mock_displayed_count = 0;
	mock_displayed_i = 0;
}
