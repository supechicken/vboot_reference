/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common mock functions and helpers of UI tests.
 */

#ifndef VBOOT_REFERENCE_VB2_UI_TEST_COMMON_H_
#define VBOOT_REFERENCE_VB2_UI_TEST_COMMON_H_

#include "2api.h"

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

/*****************************************************************************/
/* Mock vb2ex_display_ui */

/* Help function for check vb2ex_display_ui calls.
 * Arguments which equal to MOCK_FIXED should be ignored. */
void displayed_eq(const char *text,
		  enum vb2_screen screen,
		  uint32_t locale_id,
		  uint32_t selected_item,
		  uint32_t disabled_item_mask);

/* Help function for check no extra screen displayed.
 * Call this after checking all screens expected by displayed_eq. */
void displayed_no_extra(void);

/*****************************************************************************/
/* Initialization */

/* Call this first in reset_common_data.*/
void reset_ui_common_data(void);

#endif
