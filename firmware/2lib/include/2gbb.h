/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * GBB accessor functions.
 */

#ifndef VBOOT_REFERENCE_VBOOT_2GBB_H_
#define VBOOT_REFERENCE_VBOOT_2GBB_H_

#include "2common.h"

struct vb2_packed_key;
struct vb2_workbuf;

/**
 * Read the root key from the GBB, and store it onto the given workbuf.
 *
 * @param ctx		Vboot context
 * @param keyp		Returns a pointer to the key. The caller may discard
 *			workbuf state if it wants to free the key.
 * @param wb            Workbuf for data storage
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_gbb_read_root_key(struct vb2_context *ctx,
			  struct vb2_packed_key **keyp,
			  struct vb2_workbuf *wb);

/**
 * Read the recovery key from the GBB, and store it onto the given workbuf.
 *
 * @param ctx		Vboot context
 * @param keyp		Returns a pointer to the key. The caller may discard
 *			workbuf state if it wants to free the key.
 * @param wb            Workbuf for data storage
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_gbb_read_recovery_key(struct vb2_context *ctx,
			      struct vb2_packed_key **keyp,
			      struct vb2_workbuf *wb);

/**
 * Read the hardware ID from the GBB, and store it onto the given workbuf.
 *
 * @param ctx		Vboot context
 * @param hwid		Returns a pointer to the HWID string,
 * 			which will be null-terminated
 * @param size		If pointer is non-NULL, returns length of string,
 * 			including null terminator
 * @param wb            Workbuf for data storage
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_gbb_read_hwid(struct vb2_context *ctx, char **hwid, uint32_t *size,
		      struct vb2_workbuf *wb);

#endif  /* VBOOT_REFERENCE_VBOOT_2GBB_H_ */
