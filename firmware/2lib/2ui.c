/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#include "2api.h"
#include "2common.h"
#include "2ui.h"
#include "vboot_api.h"

const static struct vb2_ui_state blank_menu = {
	.locale = 0,
	.screen = VB_SCREEN_BLANK,
};

/*****************************************************************************/
/* Entry points */

vb2_error_t vb2_developer_menu(struct vb2_context *ctx)
{
	/* TODO(roccochen): Init, wait for user, and boot. */
	vb2ex_display_menu(&blank_menu);

	while (1);

	return VB2_SUCCESS;

}

vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx)
{
	/* TODO(roccochen): Init and wait for user to reset or shutdown. */
	vb2ex_display_menu(&blank_menu);

	while (1);

	return VB2_SUCCESS;
}

vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx)
{
	/* TODO(roccochen): Init and wait for user. */
	vb2ex_display_menu(&blank_menu);

	while (1);

	return VB2_SUCCESS;
}
