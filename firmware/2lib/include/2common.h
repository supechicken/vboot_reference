/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 */

#ifndef VBOOT_REFERENCE_VBOOT_2COMMON_H_
#define VBOOT_REFERENCE_VBOOT_2COMMON_H_

#include "2return_codes.h"

struct vb2_public_key;

/* Return true if pointer is 32-bit aligned */
#define is_aligned_32(ptr) (!(((size_t)(ptr)) & 3))

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

#endif  /* VBOOT_REFERENCE_VBOOT_2COMMON_H_ */
