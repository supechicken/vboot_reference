/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Access to portions of the GBB using the region API.
 */

#ifndef VBOOT_REFERENCE_GBB_ACCESS_H_
#define VBOOT_REFERENCE_GBB_ACCESS_H_

#include "vboot_api.h"

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

struct vb2_context;
struct VbPublicKey;

/**
 * Read the root key from the GBB
 *
 * @param ctx		Vboot context
 * @param keyp		Returns a pointer to the key. The caller must call
 *			free() on the key when finished with it.
 * @param wb            Workbuf in which to store data
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2api_gbb_read_root_key(struct vb2_context *ctx,
			     struct VbPublicKey **keyp,
			     struct vb2_workbuf *wb, int save_on_wb);

/**
 * Read the recovery key from the GBB
 *
 * @param ctx		Vboot context
 * @param keyp		Returns a pointer to the key. The caller must call
 *			free() on the key when finished with it.
 * @param wb            Workbuf in which to store data
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2api_gbb_read_recovery_key(struct vb2_context *ctx,
				 struct VbPublicKey **keyp,
				 struct vb2_workbuf *wb, int save_on_wb);

/**
 * Read the hardware ID from the GBB
 *
 * @param ctx		Vboot context
 * @param hwid		Returns a pointer to the HWID string,
 * 			which will be null-terminated
 * @param wb            Workbuf in which to store data
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2api_gbb_read_hwid(struct vb2_context *ctx, char **hwid,
			 struct vb2_workbuf *wb, int save_on_wb);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
