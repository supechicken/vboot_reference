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
 * Caller must fill in all of the fields at the top of the structure, then
 * call this function to initialize the context.  This must be done before
 * passing the context to any other vboot function, unless specifically
 * allowed for that function (for example, vb2_secdata_check_crc()).
 *
 * On error, always sets a recovery reason in the context.
 *
 * @param ctx		Vboot context to initialize
 * @return VB2_SUCCESS, or error code on error.
 */
void vb2_init_context(struct vb2_context *ctx);

#endif  /* VBOOT_REFERENCE_VBOOT_2MISC_H_ */
