/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Non-volatile storage routines
 */

#ifndef VBOOT_REFERENCE_VBOOT_2NVSTORAGE_H_
#define VBOOT_REFERENCE_VBOOT_2NVSTORAGE_H_

// TODO: only needed for VbNvParam enum
#include "vboot_nvstorage.h"

/**
 * Initialize the non-volatile storage context and verify its CRC.
 *
 * @param ctx		Context pointer
 */
void vb2_nv_init(struct vb2_context *ctx);

// TODO: find/replace VbNvParam enum

/**
 * Read a non-volatile value.
 *
 * @param ctx		Context pointer
 * @param param		Parameter to read
 * @return The value of the parameter.  If you somehow force an invalid
 *         parameter number, returns 0.
 */
uint32_t vb2_nv_get(struct vb2_context *ctx, VbNvParam param);

/**
 * Write a non-volatile value.
 *
 * Ignores writes to unknown params.
 *
 * @param ctx		Context pointer
 * @param param		Parameter to write
 * @param value		New value
 */
void vb2_nv_set(struct vb2_context *ctx, VbNvParam param, uint32_t value);

#endif  /* VBOOT_REFERENCE_VBOOT_2NVSTORAGE_H_ */
