/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Misc functions which need access to vb2_context but are not public APIs
 */

#ifndef VBOOT_REFERENCE_VBOOT_2MISC_H_
#define VBOOT_REFERENCE_VBOOT_2MISC_H_

struct vb2_context;

/**
 * Set up the verified boot context data.
 *
 * @param ctx		Vboot context to initialize
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_init_context(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_VBOOT_2MISC_H_ */
