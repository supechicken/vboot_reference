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

/* Default alignment of work buffer allocations */
#define VB2_WORKBUF_ALIGN 8

/* Work buffer */
struct vb2_workbuf {
	uint8_t *buf;
	uint32_t size;
};

/**
 * Initialize a work buffer.
 *
 * @param wb		Work buffer to init
 * @param buf		Pointer to work buffer data
 * @param size		Size of work buffer data in bytes
 */
void vb2_workbuf_init(struct vb2_workbuf *wb, uint8_t *buf, uint32_t size);

/**
 * Allocate space in a work buffer.
 *
 * Note that the returned buffer will always be aligned to VB2_WORKBUF_ALIGN.
 *
 * @param wb		Work buffer
 * @param size		Requested size in bytes
 * @return A pointer to the allocated space, or NULL if error.
 */
void *vb2_workbuf_alloc(struct vb2_workbuf *wb, uint32_t size);

/**
 * Reallocate space in a work buffer.
 *
 * Note that the returned buffer will always be aligned to VB2_WORKBUF_ALIGN.
 *
 * @param wb		Work buffer
 * @param oldsize	Old allocation size in bytes
 * @param newsize	Requested size in bytes
 * @return A pointer to the allocated space, or NULL if error.
 */
void *vb2_workbuf_realloc(struct vb2_workbuf *wb,
			  uint32_t oldsize,
			  uint32_t newsize);

/**
 * Free a previous allocation.
 *
 * Note that detailed tracking of allocs and frees is NOT done.  The caller
 * is responsible for pairing allocs and frees.
 *
 * @param wb		Work buffer
 * @param size		Size of data to free
 */
void vb2_workbuf_free(struct vb2_workbuf *wb, uint32_t size);

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
 * Return offset of ptr from base.
 *
 * @param base		Base pointer
 * @param ptr		Pointer at some offset from base
 * @return The offset of ptr from base.
 */
uintptr_t vb2_offset_of(const void *base, const void *ptr);

#endif  /* VBOOT_REFERENCE_VBOOT_2COMMON_H_ */
