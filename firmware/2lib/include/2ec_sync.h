/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * EC software sync for verified boot
 */

#ifndef VBOOT_REFERENCE_2EC_SYNC_H_
#define VBOOT_REFERENCE_2EC_SYNC_H_

#include "2api.h"
#include "vboot_api.h"

/**
 * Sync the Embedded Controller device to the expected version.
 *
 * This is a high-level function which calls the functions above.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or non-zero if error.
 */
vb2_error_t ec_sync(struct vb2_context *ctx);

/**
 * Sync all auxiliary firmware to the expected versions
 *
 * Invokes the functions above to do this.
 *
 * @param ctx           Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_2EC_SYNC_H_ */
