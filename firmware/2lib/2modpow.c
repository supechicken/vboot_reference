/* Copyright 2014 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Montgomery multiplication implementation using positive inverse modulus,
 * p0inv = 1 / N[0] mod 2^32. This algorithm is described in Montgomery
 * Multiplication Using Vector Instructions from August 20, 2013
 * (cf. https://eprint.iacr.org/2013/519.pdf).
 *
 * This algorithm provides good performances (comparable to the original
 * algorithm) and is designed to be able to take advantage of vector
 * instructions.
 */

#include "2return_codes.h"
#include "2rsa.h"
#include "2rsa_private.h"

#include "key_positive_inverse_modulus.h"

/**
 * Montgomery c[] = d[] - e[] if d[] > e[], c[] = d[] - e[] + mod[] otherwise.
 *
 * d[] and e[] are used as temporary buffers and therefore the content will be
 * lost.
 */
static void subMod(const struct vb2_public_key *key,
		   uint32_t *d, uint32_t *e, uint32_t *c)
{
	uint32_t i, borrow = 0, carry = 0;
	uint64_t sum;

	for (i = 0; i < key->arrsize; i++) {
		uint32_t tmp = d[i] - e[i] - borrow;
		borrow = d[i] ^ ((d[i] ^ e[i]) | (d[i] ^ tmp));
		borrow >>= 31;
		/* Use d[] as temporary storage. */
		d[i] = tmp;
	}

	/* To keep the code running in constant-time for side-channel
	 * resistance, d[] − e[] + mod[] is systematically computed even if we
	 * do not need it. */
	for (i = 0; i < key->arrsize; i++) {
		sum = (uint64_t)d[i] + key->n[i] + carry;
		carry = sum >> 32;
		/* Use e[] as temporary storage. */
		e[i] = (uint32_t)sum;
	}

	memcpy(c, borrow ? e : d, key->arrsize * sizeof(*c));
}

/**
 * Montgomery c[] = a[] * b[] / R % mod
 */
static void montMul(const struct vb2_public_key *key,
		    uint32_t *c,
		    const uint32_t *a,
		    const uint32_t *b,
		    uint32_t *d,
		    uint32_t *e)
{
	const uint32_t mu = inv_mod;
	const uint32_t mub0 = mu * b[0];
	uint32_t i, j, q, muc0, t0, t1;
	uint64_t p0, p1;

	for (i = 0; i < key->arrsize; i++) {
		d[i] = 0;
		e[i] = 0;
	}

	for (j = 0; j < key->arrsize; j++) {
		c[0] = d[0] - e[0];
		muc0 = mu * c[0];
		q = muc0 + mub0 * a[j];

		p0 = (uint64_t)a[j] * b[0] + (uint64_t)d[0];
		t0 = (p0 >> 32);

		p1 = (uint64_t)q * key->n[0] + (uint64_t)e[0];
		t1 = (p1 >> 32);

		for (i = 1; i < key->arrsize; i++) {
			p0 = (uint64_t)a[j] * b[i] + (uint64_t)t0 + (uint64_t)d[i];
			t0 = p0 >> 32;
			d[i - 1] = (uint32_t)p0;

			p1 = (uint64_t)q * key->n[i] + (uint64_t)t1 + (uint64_t)e[i];
			t1 = p1 >> 32;
			e[i - 1] = (uint32_t)p1;
		}

		d[key->arrsize - 1] = t0;
		e[key->arrsize - 1] = t1;
	}

	subMod(key, d, e, c);
}

static uint32_t swap_uint32(uint32_t val)
{
	val = ((val << 8) & 0xff00ff00) | ((val >> 8) & 0xff00ff);
	return (val << 16) | (val >> 16);
}

static void swap_bignumber_endianess(const uint32_t *in, uint32_t *out,
				     size_t size)
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
 *			(5 * key->arrsize) elements long.
 * @param exp		RSA public exponent: either 65537 (F4) or 3
 */
void modpow(const struct vb2_public_key *key, uint8_t *inout,
	    uint32_t *workbuf32, int exp)
{
	uint32_t *a = workbuf32;
	uint32_t *aR = a + key->arrsize;
	uint32_t *aaR = aR + key->arrsize;
	uint32_t *aaa = aaR;  /* Re-use location. */
	uint32_t *d = aaa + key->arrsize;
	uint32_t *e = d + key->arrsize;
	uint32_t i;

	/* Convert from big endian byte array to little endian word array. */
	swap_bignumber_endianess((uint32_t *)inout, a, key->arrsize);

	inv_mod = get_inv_mod(key, 1);
	montMul(key, aR, a, key->rr, d, e);  /* aR = a * RR / R mod M   */
	if (exp == 3) {
		montMul(key, aaR, aR, aR, d, e); /* aaR = aR * aR / R mod M */
		montMul(key, a, aaR, aR, d, e); /* a = aaR * aR / R mod M */

		/* To multiply with 1, prepare aR with first element 1 and
		   others as 0. */
		aR[0] = 1;
		for (i = 1; i < key->arrsize; i++)
			aR[i] = 0;

		montMul(key, aaa, a, aR, d, e); /* aaa = a * aR / R mod M = a * 1 / R mod M*/
	} else {
		/* Exponent 65537 */
		for (i = 0; i < 16; i+=2) {
			montMul(key, aaR, aR, aR, d, e);  /* aaR = aR * aR / R mod M */
			montMul(key, aR, aaR, aaR, d, e);  /* aR = aaR * aaR / R mod M */
		}
		montMul(key, aaa, aR, a, d, e);  /* aaa = aR * a / R mod M */
	}

	/* Convert to bigendian byte array */
	swap_bignumber_endianess(aaa, (uint32_t *)inout, key->arrsize);
}
