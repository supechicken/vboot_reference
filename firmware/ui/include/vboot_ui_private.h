/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * UI functions used by unit tests.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_PRIVATE_H_
#define VBOOT_REFERENCE_VBOOT_UI_PRIVATE_H_

/* Flags for VbUserConfirms() */
#define VB_CONFIRM_MUST_TRUST_KEYBOARD (1 << 0)
#define VB_CONFIRM_SPACE_MEANS_NO      (1 << 1)

int VbUserConfirms(struct vb2_context *ctx, uint32_t confirm_flags);

void vb2_init_ui(void);

#endif  /* VBOOT_REFERENCE_VBOOT_UI_PRIVATE_H_ */
