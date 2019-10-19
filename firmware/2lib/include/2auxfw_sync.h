/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Auxiliary firmware software sync for verified boot
 */

#ifndef VBOOT_REFERENCE_AUXFW_SYNC_H_
#define VBOOT_REFERENCE_AUXFW_SYNC_H_

#include "vboot_api.h"

/* TODO(docs) */
vb2_error_t auxfw_sync(struct vb2_context *ctx);

vb2_error_t auxfw_sync_phase1(struct vb2_context *ctx);

vb2_error_t auxfw_sync_check(struct vb2_context *ctx,
			     VbAuxFwUpdateSeverity_t *severity);

vb2_error_t auxfw_sync_phase2(struct vb2_context *ctx);

vb2_error_t auxfw_sync_phase3(struct vb2_context *ctx);

#endif /* VBOOT_REFERENCE_AUXFW_SYNC_H_ */
