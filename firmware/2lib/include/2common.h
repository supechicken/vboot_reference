/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 */

#ifndef VBOOT_REFERENCE_VBOOT_2COMMON_H_
#define VBOOT_REFERENCE_VBOOT_2COMMON_H_

#include "2api.h"
#include "2return_codes.h"
#include "2struct.h"

struct vb2_public_key;

/* Return the greater of A and B */
#ifndef VB2_MAX
#define VB2_MAX(A, B) ((A) > (B) ? (A) : (B))
#endif

/* Debug output */
#if defined(VBOOT_DEBUG) && !defined(VB2_DEBUG)
#include <stdio.h>
#define VB2_DEBUG(format, args...) printf(format, ## args)
#else
#define VB2_DEBUG(format, args...)
#endif

/* Check if a pointer is aligned on an align-byte boundary */
#define vb_aligned(ptr, align) (!(((size_t)(ptr)) & ((align) - 1)))

/**
 * Align a buffer and check its size.
 *
 * @param **ptr		Pointer to pointer to align
 * @param *size		Points to size of buffer pointed to by *ptr
 * @param align		Required alignment (must be power of 2)
 * @param want_size	Required size
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_align(uint8_t **ptr,
	      uint32_t *size,
	      uint32_t align,
	      uint32_t want_size);

/**
 * Get the shared data pointer from the vboot context
 *
 * @param ctx		Vboot context
 * @return The shared data pointer.
 */
static __inline struct vb2_shared_data *vb2_get_sd(struct vb2_context *ctx) {
	return (struct vb2_shared_data *)ctx->workbuf;
}

#endif  /* VBOOT_REFERENCE_VBOOT_2COMMON_H_ */
