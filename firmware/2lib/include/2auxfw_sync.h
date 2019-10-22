/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Auxiliary firmware software sync for verified boot
 */

#ifndef VBOOT_REFERENCE_AUXFW_SYNC_H_
#define VBOOT_REFERENCE_AUXFW_SYNC_H_

#include "2api.h"

/**
 * Sync all auxiliary firmware to the expected versions
 *
 * Invokes the functions above to do this.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync(struct vb2_context *ctx);

#endif /* VBOOT_REFERENCE_AUXFW_SYNC_H_ */
