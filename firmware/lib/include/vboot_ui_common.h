/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common code used by both vboot_ui and vboot_ui_menu.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_COMMON_H_
#define VBOOT_REFERENCE_VBOOT_UI_COMMON_H_

enum vb2_beep_type {
	VB_BEEP_FAILED,		/* Permitted but the operation failed */
	VB_BEEP_NOT_ALLOWED,	/* Operation disabled by user setting */
};

/**
 * Emit beeps to indicate an error.
 *
 * @param beep		Type of beep sound.
 */
void vb2_error_beep(enum vb2_beep_type beep);

/**
 * Prints a message to screen, logs a possibly different message to console,
 * and beeps to notify user.
 *
 * @param print_msg	Display message.  NULL message will be ignored.
 * @param log_msg	Log message.  If NULL, uses print_msg (if available).
 * @param beep		Type of beep sound.
 */
void vb2_error_notify(const char *print_msg,
		      const char *log_msg,
		      enum vb2_beep_type beep);

/**
 * Run alternative firmware.
 *
 * This will only return if the bootloader cannot be found or fails to start.
 *
 * @param ctx		Vboot context
 * @param altfw_num	Number of bootloader to start (see VbAltFwIndex_t).
 * @return VB2_SUCCESS, or error code on error.
 */
vb2_error_t vb2_run_altfw(struct vb2_context *ctx,
			  enum VbAltFwIndex_t altfw_num);

/**
 * Display an error and beep to indicate that altfw is not available.
 *
 * @return VB2_SUCCESS, or error code on error.
 */
vb2_error_t vb2_error_no_altfw(void);

/**
 * Jump to a bootloader if allowed.
 *
 * This checks if the operation is permitted.  If it is, then it jumps to the
 * selected bootloader and execution continues there, never returning.
 *
 * If the operation is not permitted, or it is permitted but the bootloader
 * cannot be found, it beeps and returns.
 *
 * @param ctx		Vboot context
 * @param allowed	1 if allowed, 0 if not allowed
 * @param altfw_num	Number of bootloader to start (see VbAltFwIndex_t).
 * @return VB2_SUCCESS, or error code on error.
 */
vb2_error_t vb2_try_alt_fw(struct vb2_context *ctx, int allowed,
			   enum VbAltFwIndex_t altfw_num);

#endif  /* VBOOT_REFERENCE_VBOOT_UI_COMMON_H_ */
