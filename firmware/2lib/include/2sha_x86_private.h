/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Inline functions for x86 intrinsics.
 */

#ifndef VBOOT_REFERENCE_2SHA_X86_PRIVATE_H_
#define VBOOT_REFERENCE_2SHA_X86_PRIVATE_H_

typedef int vb2_m128i __attribute__ ((vector_size(16)));

static inline vb2_m128i vb2_loadu_si128(vb2_m128i *ptr)
{
	vb2_m128i result;
	asm volatile ("movups %1, %0" : "=x"(result) : "m"(*ptr));
	return result;
}

static inline void vb2_storeu_si128(vb2_m128i *to, vb2_m128i from)
{
	asm volatile ("movups %1, %0" : "=m"(*to) : "x"(from));
}

static inline vb2_m128i vb2_add_epi32(vb2_m128i a, vb2_m128i b)
{
	return a + b;
}

static inline vb2_m128i vb2_shuffle_epi8(vb2_m128i value, vb2_m128i mask)
{
	asm ("pshufb %1, %0" : "+x"(value) : "xm"(mask));
	return value;
}

static inline vb2_m128i vb2_shuffle_epi32(vb2_m128i value, int mask)
{
	vb2_m128i result;
	asm ("pshufd %2, %1, %0" : "=x"(result) : "xm"(value), "i" (mask));
	return result;
}

static inline vb2_m128i vb2_alignr_epi8(vb2_m128i a, vb2_m128i b, int imm8)
{
	asm ("palignr %2, %1, %0" : "+x"(a) : "xm"(b), "i"(imm8));
	return a;
}

static inline vb2_m128i vb2_sha256msg1_epu32(vb2_m128i a, vb2_m128i b)
{
	asm ("sha256msg1 %1, %0" : "+x"(a) : "xm"(b));
	return a;
}

static inline vb2_m128i vb2_sha256msg2_epu32(vb2_m128i a, vb2_m128i b)
{
	asm ("sha256msg2 %1, %0" : "+x"(a) : "xm"(b));
	return a;
}

static inline vb2_m128i vb2_sha256rnds2_epu32(vb2_m128i a, vb2_m128i b,
                                              vb2_m128i k)
{
	asm ("sha256rnds2 %1, %0" : "+x"(a) : "xm"(b), "Yz"(k));
	return a;
}

#endif	/* VBOOT_REFERENCE_2SHA_X86_PRIVATE_H_ */
