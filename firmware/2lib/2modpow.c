/* Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of Montgomery multiplication using negative inverse modulus.
 * n0inv = -1 / N[0] mod 2^32
 */

#include "2return_codes.h"
#include "2rsa.h"
#include "2rsa_private.h"

/**
 * a[] -= mod
 */
static void subM(const struct vb2_public_key *key, uint32_t *a)
{
	int64_t A = 0;
	uint32_t i;
	for (i = 0; i < key->arrsize; ++i) {
		A += (uint64_t)a[i] - key->n[i];
		a[i] = (uint32_t)A;
		A >>= 32;
	}
}

/**
 * Montgomery c[] += a * b[] / R % mod
 */
static void montMulAdd(const struct vb2_public_key *key,
		       uint32_t *c,
		       const uint32_t a,
		       const uint32_t *b)
{
	uint64_t A = (uint64_t)a * b[0] + c[0];
	uint32_t d0 = (uint32_t)A * key->n0inv;
	uint64_t B = (uint64_t)d0 * key->n[0] + (uint32_t)A;
	uint32_t i;

	for (i = 1; i < key->arrsize; ++i) {
		A = (A >> 32) + (uint64_t)a * b[i] + c[i];
		B = (B >> 32) + (uint64_t)d0 * key->n[i] + (uint32_t)A;
		c[i - 1] = (uint32_t)B;
	}

	A = (A >> 32) + (B >> 32);

	c[i - 1] = (uint32_t)A;

	if (A >> 32) {
		subM(key, c);
	}
}

/**
 * Montgomery c[] += 0 * b[] / R % mod
 */
static void montMulAdd0(const struct vb2_public_key *key,
			uint32_t *c,
			const uint32_t *b)
{
	uint32_t d0 = c[0] * key->n0inv;
	uint64_t B = (uint64_t)d0 * key->n[0] + c[0];
	uint32_t i;

	for (i = 1; i < key->arrsize; ++i) {
		B = (B >> 32) + (uint64_t)d0 * key->n[i] + c[i];
		c[i - 1] = (uint32_t)B;
	}

	c[i - 1] = B >> 32;
}

/**
 * Montgomery c[] = a[] * b[] / R % mod
 */
static void montMul(const struct vb2_public_key *key,
		    uint32_t *c,
		    const uint32_t *a,
		    const uint32_t *b)
{
	uint32_t i;
	for (i = 0; i < key->arrsize; ++i) {
		c[i] = 0;
	}
	for (i = 0; i < key->arrsize; ++i) {
		montMulAdd(key, c, a[i], b);
	}
}

/* Montgomery c[] = a[] * 1 / R % key. */
static void montMul1(const struct vb2_public_key *key,
		     uint32_t *c,
		     const uint32_t *a)
{
	int i;

	for (i = 0; i < key->arrsize; ++i)
		c[i] = 0;

	montMulAdd(key, c, 1, a);
	for (i = 1; i < key->arrsize; ++i)
		montMulAdd0(key, c, a);
}

/**
 * In-place public exponentiation.
 *
 * @param key		Key to use in signing
 * @param inout		Input and output big-endian byte array
 * @param workbuf32	Work buffer; caller must verify this is
 *			(3 * key->arrsize) elements long.
 * @param exp		RSA public exponent: either 65537 (F4) or 3
 */
void modpow(const struct vb2_public_key *key, uint8_t *inout,
	    uint32_t *workbuf32, int exp)
{
	uint32_t *a = workbuf32;
	uint32_t *aR = a + key->arrsize;
	uint32_t *aaR = aR + key->arrsize;
	uint32_t *aaa = aaR;  /* Re-use location. */
	int i;

	/* Convert from big endian byte array to little endian word array. */
	for (i = 0; i < (int)key->arrsize; ++i) {
		uint32_t tmp =
			((uint32_t)inout[((key->arrsize - 1 - i) * 4) + 0]
			 << 24) |
			(inout[((key->arrsize - 1 - i) * 4) + 1] << 16) |
			(inout[((key->arrsize - 1 - i) * 4) + 2] << 8) |
			(inout[((key->arrsize - 1 - i) * 4) + 3] << 0);
		a[i] = tmp;
	}

	montMul(key, aR, a, key->rr);  /* aR = a * RR / R mod M   */
	if (exp == 3) {
		montMul(key, aaR, aR, aR); /* aaR = aR * aR / R mod M */
		montMul(key, a, aaR, aR); /* a = aaR * aR / R mod M */
		montMul1(key, aaa, a); /* aaa = a * 1 / R mod M */
	} else {
		/* Exponent 65537 */
		for (i = 0; i < 16; i+=2) {
			montMul(key, aaR, aR, aR);  /* aaR = aR * aR / R mod M */
			montMul(key, aR, aaR, aaR);  /* aR = aaR * aaR / R mod M */
		}
		montMul(key, aaa, aR, a);  /* aaa = aR * a / R mod M */
	}

	/* Make sure aaa < mod; aaa is at most 1x mod too large. */
	if (vb2_mont_ge(key, aaa)) {
		subM(key, aaa);
	}

	/* Convert to bigendian byte array */
	for (i = (int)key->arrsize - 1; i >= 0; --i) {
		uint32_t tmp = aaa[i];
		*inout++ = (uint8_t)(tmp >> 24);
		*inout++ = (uint8_t)(tmp >> 16);
		*inout++ = (uint8_t)(tmp >>  8);
		*inout++ = (uint8_t)(tmp >>  0);
	}
}
