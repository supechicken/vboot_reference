/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 */

#ifndef VBOOT_REFERENCE_VB20_MISC_H_
#define VBOOT_REFERENCE_VB20_MISC_H_

#include "2api.h"

/**
 * Verify the firmware keyblock using the root key.
 *
 * After this call, the data key is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb20_load_fw_keyblock(struct vb2_context *ctx);

/**
 * Verify the firmware preamble using the data subkey from the keyblock.
 *
 * After this call, the preamble is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb20_load_fw_preamble(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_VB20_MISC_H_ */
