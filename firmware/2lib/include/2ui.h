/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#ifndef VBOOT_REFERENCE_2UI_H_
#define VBOOT_REFERENCE_2UI_H_

/* Entry points */

/**
 * UI for a developer-mode boot.
 *
 * Enter the developer menu, which provides options to use legacy bootloader,
 * switch out of developer mode, or by default, continue booting Chrome OS.
 *
 * This menu support users to insert external media directly, to press certain
 * combo key sets, and to choose between menu options via arrow keys and
 * the enter key (or volume key/power key in detachable devices).
 *
 * If a timeout occurs, leave the waiting loop and boot from the default option.
 *
 * TODO(roccochen): all menu functionalities
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_developer_menu(struct vb2_context *ctx);

/**
 * UI for a non-manual recovery. (BROKEN)
 *
 * Enter the recovery menu, which shows that an unrecoverable error was
 * encountered last boot. Wait for the user to reset or shut down.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx);

/**
 * UI for a a recovery-mode boot.
 *
 * Enter the recovery menu, which prompts the user to insert recovery media or
 * navigate the step-by-step recovery.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_2UI_H_ */
