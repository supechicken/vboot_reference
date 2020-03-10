/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * User interfaces for developer and recovery mode menus.
 */

#ifndef VBOOT_REFERENCE_2UI_H_
#define VBOOT_REFERENCE_2UI_H_

/**
 * UI for a developer-mode boot.
 *
 * Enter the developer menu, which provides options to switch out of developer
 * mode, boot from external media, use legacy bootloader, or boot Chrome OS from
 * disk.
 *
 * If a timeout occurs, take the default boot action.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_developer_menu(struct vb2_context *ctx);

/**
 * UI for a non-manual recovery ("BROKEN").
 *
 * Enter the recovery menu, which shows that an unrecoverable error was
 * encountered last boot. Wait for the user to physically reset or shut down.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_broken_recovery_menu(struct vb2_context *ctx);

/**
 * UI for a manual recovery-mode boot.
 *
 * Enter the recovery menu, which prompts the user to insert recovery media,
 * navigate the step-by-step recovery, or enter developer mode if allowed.
 *
 * @param ctx		Vboot context
 * @returns VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t vb2_manual_recovery_menu(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_2UI_H_ */

/* Data for looping timer */
struct vb2_timer_data {
	/* Times timer has been opened */
	int open_count;
	/* Use short delay? */
	int use_short;
	/* Time of last open */
	uint64_t open_time;
	/* TODO(roccochen): Beep */
};

/* Initialization the timer data */
void vb2_timer_init(struct vb2_timer_data *td);

/* Activate the timer */
void vb2_timer_start(struct vb2_context *ctx, struct vb2_timer_data *td);

/* Caller should loop without extra delay until this returns false. */
int vb2_timer_looping(struct vb2_timer_data *td);
