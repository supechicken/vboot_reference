/* Copyright (C) 2023 Intel Corporation
 * Authors: Muhammad Monir Hossain <muhammad.monir.hossain@intel.com>
 *          Jeremy Compostella <jeremy.compostella@intel.com>
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * The algorithm implemented below is described in Montgomery Multiplication
 * Using Vector Instructions document from August 20, 2013
 * (cf. https://eprint.iacr.org/2013/519.pdf).
 *
 * This implementation leverages SSE2 instructions to perform arithmetic
 * operations in parallel.
 *
 * This algorithm uses the modulus positive inverse (1 / N mod 2^32) which can
 * be easily computed from the modulus negative inverse provided by the public
 * key data structure `n0inv' field.
 */

#include <emmintrin.h>

#include "2return_codes.h"
#include "2rsa.h"
#include "2rsa_private.h"

#define ALIGN(x, a)             __ALIGN_MASK(x, (__typeof__(x))(a)-1UL)
#define __ALIGN_MASK(x, mask)   (((x)+(mask))&~(mask))
#define ALIGN_UP(x, a)          ALIGN((x), (a))

/**
 * Montgomery c[] = d[] - e[] if d[] > e[], c[] = d[] - e[] + mod[] otherwise.
 *
 * de[] has d[] in lower 64 bits (effectively lower 32 bits) and e[] in upper
 * 64 bits (effectively lower 32 bits)
 * de[] is used as a temporary buffer and therefore its content will be lost.
 */
static void subMod(const struct vb2_public_key *key, __m128i *de, uint32_t *c)
{
	uint32_t i, borrow = 0, carry = 0, d, e;
	uint64_t sum, *de_i;

	for (i = 0; i < key->arrsize; i++) {
		de_i = (uint64_t *)&de[i];
		d = (uint32_t)de_i[1];
		e = (uint32_t)de_i[0];

		/* Use de_i[0] as temporary storage of d[] - e[]. */
		de_i[0] = (uint32_t)d - e - borrow;

		borrow = d ^ ((d ^ e) | (d ^ (uint32_t)de_i[0]));
		borrow >>= 31;
	}

	/* To keep the code running in constant-time for side-channel
	 * resistance, D − E + mod is systematically computed even if we do not
	 * need it. */
	for (i = 0; i < key->arrsize; i++) {
		de_i = (uint64_t *)&de[i];
		sum = de_i[0] + key->n[i] + carry;
		carry = sum >> 32;

		/* Use de_i[1] as temporary storage. */
		de_i[1] = (uint32_t)sum;
	}

	int index = borrow ? 1 : 0;
	for (i = 0; i < key->arrsize; i++) {
		de_i = (uint64_t *)&de[i];
		c[i] = (uint32_t)de_i[index];
	}
}

/**
 * Montgomery c[] = a[] * b[] / R % mod
 */
static void montMul(const struct vb2_public_key *key,
		    uint32_t *c,
		    const uint32_t *a,
		    const uint32_t *b,
		    const uint32_t mu,
		    __m128i *de,
		    __m128i *b_modulus)
{
	const uint32_t mub0 = mu * b[0];
	const __m128i mask = _mm_set_epi32(0,  0xffffffff, 0, 0xffffffff);
	const uint64_t *de0 = (uint64_t *)de;
	uint32_t i, j, q, muc0;
	__m128i p01, t01, mul;

	for (i = 0; i < key->arrsize; i++) {
		b_modulus[i] = _mm_set_epi32(0, b[i], 0, key->n[i]);
		de[i] = _mm_setzero_si128();
	}

	for (j = 0; j < key->arrsize; j++) {
		c[0] = (uint32_t)de0[1] - de0[0];
		muc0 = mu * c[0];

		q = muc0 + mub0 * a[j];

		mul = _mm_set_epi32(0, a[j], 0, q);

		p01 = _mm_add_epi64(de[0], _mm_mul_epu32(mul, b_modulus[0]));

		t01 = _mm_srli_epi64(p01, 32);

		for (i = 1; i < key->arrsize; i++) {
			p01 = _mm_add_epi64(_mm_add_epi64(t01, de[i]),
					    _mm_mul_epu32(mul, b_modulus[i]));

			t01 = _mm_srli_epi64(p01, 32);

			de[i - 1] = _mm_and_si128(mask, p01);
		}

		de[key->arrsize - 1] = t01;
	}

	subMod(key, de, c);
}

static uint32_t swap_uint32(uint32_t val)
{
	val = ((val << 8) & 0xff00ff00) | ((val >> 8) & 0xff00ff);
	return (val << 16) | (val >> 16);
}

static void swap_endianess(const uint32_t *in, uint32_t *out, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		out[i] = swap_uint32(in[size - 1 - i]);
}

/**
 * In-place public exponentiation.
 *
 * @param key		Key to use in signing
 * @param inout		Input and output big-endian byte array
 * @param workbuf32	Work buffer; caller must verify this is
 *			(12 * key->arrsize) elements long.
 * @param exp		RSA public exponent: either 65537 (F4) or 3
 */
void modpow(const struct vb2_public_key *key, uint8_t *inout,
	    uint32_t *workbuf32, int exp)
{
	const uint32_t mu = (uint32_t)(1ULL << 32) - key->n0inv;
	uint32_t *a = workbuf32;
	uint32_t *aR = a + key->arrsize;
	uint32_t *aaR = aR + key->arrsize;
	uint32_t *aaa = aaR;  /* Re-use location. */
	__m128i *de = (__m128i *)ALIGN_UP((uintptr_t)(aaa + key->arrsize),
					  sizeof(__m128i));
	__m128i *b_modulus = de + key->arrsize;
	size_t i;

	/* Convert big endian to little endian. */
	swap_endianess((uint32_t *)inout, a, key->arrsize);

	/* aR = a * RR / R mod M  */
	montMul(key, aR, a, key->rr, mu, de, b_modulus);
	if (exp == 3) {
		/* aaR = aR * aR / R mod M */
		montMul(key, aaR, aR, aR, mu, de, b_modulus);
		/* a = aaR * aR / R mod M */
		montMul(key, a, aaR, aR, mu, de, b_modulus);

		/* To multiply with 1, prepare aR with first element 1 and
		 * others as 0. */
		aR[0] = 1;
		for (i = 1; i < key->arrsize; i++)
			aR[i] = 0;

		/* aaa = a * aR / R mod M = a * 1 / R mod M*/
		montMul(key, aaa, a, aR, mu, de, b_modulus);
	} else {
		/* Exponent 65537 */
		for (i = 0; i < 16; i += 2) {
			/* aaR = aR * aR / R mod M */
			montMul(key, aaR, aR, aR, mu, de, b_modulus);
			/* aR = aaR * aaR / R mod M */
			montMul(key, aR, aaR, aaR, mu, de, b_modulus);
		}
		/* aaa = aR * a / R mod M */
		montMul(key, aaa, aR, a, mu, de, b_modulus);
	}

	/* Convert little endian to big endian. */
	swap_endianess(aaa, (uint32_t *)inout, key->arrsize);
}
