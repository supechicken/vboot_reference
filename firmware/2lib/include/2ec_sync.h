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
 * EC sync, phase 1
 *
 * This checks whether the EC is running the correct image to do EC sync, and
 * whether any updates are necessary.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, VBERROR_EC_REBOOT_TO_RO_REQUIRED if the EC must
 * reboot back to its RO code to continue EC sync, or other non-zero error
 * code.
 */
vb2_error_t ec_sync_phase1(struct vb2_context *ctx);

/**
 * Returns non-zero if the EC will perform a slow update during phase 2.
 *
 * This is only valid after calling ec_sync_phase1(), before calling
 * ec_sync_phase2().
 *
 * @param ctx		Vboot2 context
 * @return non-zero if a slow update will be done; zero if no update or a
 * fast update.
 */
int ec_will_update_slowly(struct vb2_context *ctx);

/**
 * EC sync, phase 2
 *
 * This updates the EC if necessary, makes sure it has protected its image(s),
 * and makes sure it has jumped to the correct image.
 *
 * If ec_will_update_slowly(), it is suggested that the caller display a
 * warning screen before calling phase 2.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS, VBERROR_EC_REBOOT_TO_RO_REQUIRED if the EC must
 * reboot back to its RO code to continue EC sync, or other non-zero error
 * code.
 */
vb2_error_t ec_sync_phase2(struct vb2_context *ctx);

/**
 * EC sync, phase 3
 *
 * This completes EC sync and handles battery cutoff if needed.
 *
 * @param ctx		Vboot2 context
 * @return VB2_SUCCESS or non-zero error code.
 */
vb2_error_t ec_sync_phase3(struct vb2_context *ctx);

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
 * Decides if auxfw sync is allowed to be performed
 *
 * If sync is allowed, invokes the external callback,
 * vb2ex_auxfw_check() to allow the client to decide on the auxfw
 * update severity.
 *
 * @param ctx           Vboot2 context
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
 * @param ctx           Vboot2 context
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
 * @param ctx           Vboot2 context
 * @return VB2_SUCCESS, or non-zero error code.
 */
vb2_error_t auxfw_sync_finalize(struct vb2_context *ctx);

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
