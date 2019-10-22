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
 * Decides if auxfw sync is allowed to be performed
 *
 * If sync is allowed, invokes the external callback,
 * vb2ex_auxfw_check() to allow the client to decide on the auxfw
 * update severity.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync_check_update(struct vb2_context *ctx,
				    VbAuxFwUpdateSeverity_t *severity);

/**
 * Performs the auxfw update, if applicable
 *
 * Invokes the external callback, vb2ex_auxfw_update(), in order to
 * update all auxfw images.  Requests recovery if an error (besides a
 * required reboot) occurs.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync_perform_update(struct vb2_context *ctx);

/**
 * Finalize auxfw software sync
 *
 * Invokes vb2ex_auxfw_vboot_done(), passing in any applicable
 * recovery reason, to allow the client to perform any actions which
 * should be taken if no update was required or if it was successful.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync_finalize(struct vb2_context *ctx);

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
