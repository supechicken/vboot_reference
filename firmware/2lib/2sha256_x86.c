/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SHA256 implementation using x86 SHA extension.
 */
#include "2common.h"
#include "2sha.h"
#include "2api.h"

/* immintrin.h needs EINVAL to be defined */
#define EINVAL 22
#include <immintrin.h>

static struct vb2_sha256_context sha_ctx;

static const uint32_t sha256_h0[8] = {
	0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
	0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19
};

static const uint32_t sha256_k[64] = {
	0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
	0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
	0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
	0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
	0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
	0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
	0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
	0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
	0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
	0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
	0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
	0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
	0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
	0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
	0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
	0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define UNPACK32(x, str)				\
	{						\
		*((str) + 3) = (uint8_t) ((x)      );	\
		*((str) + 2) = (uint8_t) ((x) >>  8);	\
		*((str) + 1) = (uint8_t) ((x) >> 16);	\
		*((str) + 0) = (uint8_t) ((x) >> 24);	\
	}

#define SHA256_X86_PUT_STATE1(j, i) 					\
	{								\
		msgtmp[j] = _mm_loadu_si128((__m128i *)			\
				(message + (i << 6) + (j * 16)));	\
		msgtmp[j] = _mm_shuffle_epi8(msgtmp[j], shuf_mask);	\
		msg = _mm_add_epi32(msgtmp[j],				\
			_mm_loadu_si128((__m128i *)&sha256_k[j * 4]));	\
		state1 = _mm_sha256rnds2_epu32(state1, state0, msg);	\
	}

#define SHA256_X86_PUT_STATE0(j, i) 					\
	{								\
		msg    = _mm_shuffle_epi32(msg, 0x0E);			\
		state0 = _mm_sha256rnds2_epu32(state0, state1, msg);	\
	}

#define SHA256_X86_LOOP(j, i)						\
	{								\
		int k = j & 3;						\
		int prev_k = (k + 3) & 3;				\
		int next_k = (k + 1) & 3;				\
		msg = _mm_add_epi32(msgtmp[k],				\
			_mm_loadu_si128((__m128i *)&sha256_k[j * 4]));	\
		state1 = _mm_sha256rnds2_epu32(state1, state0, msg);	\
		tmp = _mm_alignr_epi8(msgtmp[k], msgtmp[prev_k], 4);	\
		msgtmp[next_k] = _mm_add_epi32(msgtmp[next_k], tmp);	\
		msgtmp[next_k] = _mm_sha256msg2_epu32(msgtmp[next_k],	\
					msgtmp[k]);			\
		SHA256_X86_PUT_STATE0(j, i);				\
		msgtmp[prev_k] = _mm_sha256msg1_epu32(msgtmp[prev_k],	\
				msgtmp[k]);				\
	}

static void vb2_sha256_transform_x86ext(const uint8_t *message,
					unsigned int block_nb)
{
	__m128i state0, state1, msg, abef_save, cdgh_save;
	__m128i msgtmp[4];
	__m128i tmp;
	int i;
	const __m128i shuf_mask = _mm_set_epi64x(0x0c0d0e0f08090a0bull,
			    			 0x0405060700010203ull);

	state0 = _mm_loadu_si128((__m128i *)&sha_ctx.h[0]);
	state1 = _mm_loadu_si128((__m128i *)&sha_ctx.h[4]);
	for (i = 0; i < (int) block_nb; i++) {
		abef_save = state0;
		cdgh_save = state1;

		SHA256_X86_PUT_STATE1(0, i);
		SHA256_X86_PUT_STATE0(0, i);

		SHA256_X86_PUT_STATE1(1, i);
		SHA256_X86_PUT_STATE0(1, i);
		msgtmp[0] = _mm_sha256msg1_epu32(msgtmp[0], msgtmp[1]);

		SHA256_X86_PUT_STATE1(2, i);
		SHA256_X86_PUT_STATE0(2, i);
		msgtmp[1] = _mm_sha256msg1_epu32(msgtmp[1], msgtmp[2]);

		SHA256_X86_PUT_STATE1(3, i);
		tmp = _mm_alignr_epi8(msgtmp[3], msgtmp[2], 4);
		msgtmp[0] = _mm_add_epi32(msgtmp[0], tmp);
		msgtmp[0] = _mm_sha256msg2_epu32(msgtmp[0], msgtmp[3]);
		SHA256_X86_PUT_STATE0(3, i);
		msgtmp[2] = _mm_sha256msg1_epu32(msgtmp[2], msgtmp[3]);

		SHA256_X86_LOOP(4, i);
		SHA256_X86_LOOP(5, i);
		SHA256_X86_LOOP(6, i);
		SHA256_X86_LOOP(7, i);
		SHA256_X86_LOOP(8, i);
		SHA256_X86_LOOP(9, i);
		SHA256_X86_LOOP(10, i);
		SHA256_X86_LOOP(11, i);
		SHA256_X86_LOOP(12, i);
		SHA256_X86_LOOP(13, i);
		SHA256_X86_LOOP(14, i);

		msg = _mm_add_epi32(msgtmp[3],
			_mm_loadu_si128((__m128i *)&sha256_k[15 * 4]));
		state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
		SHA256_X86_PUT_STATE0(15, i);

		state0 = _mm_add_epi32(state0, abef_save);
		state1 = _mm_add_epi32(state1, cdgh_save);

	}

	_mm_storeu_si128((__m128i *)&sha_ctx.h[0], state0);
	_mm_storeu_si128((__m128i *)&sha_ctx.h[4], state1);
}

vb2_error_t vb2ex_hwcrypto_digest_init(enum vb2_hash_algorithm hash_alg,
				       uint32_t data_size)
{
	if (hash_alg != VB2_HASH_SHA256)
		return VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED;

	sha_ctx.h[0] = sha256_h0[5];
	sha_ctx.h[1] = sha256_h0[4];
	sha_ctx.h[2] = sha256_h0[1];
	sha_ctx.h[3] = sha256_h0[0];
	sha_ctx.h[4] = sha256_h0[7];
	sha_ctx.h[5] = sha256_h0[6];
	sha_ctx.h[6] = sha256_h0[3];
	sha_ctx.h[7] = sha256_h0[2];

	return VB2_SUCCESS;
}

vb2_error_t vb2ex_hwcrypto_digest_extend(const uint8_t *buf, uint32_t size)
{
	unsigned int block_nb;
	unsigned int new_size, rem_size, tmp_size;
	const uint8_t *shifted_data;

	tmp_size = VB2_SHA256_BLOCK_SIZE - sha_ctx.size;
	rem_size = size < tmp_size ? size : tmp_size;

	memcpy(&sha_ctx.block[sha_ctx.size], buf, rem_size);

	if (sha_ctx.size + size < VB2_SHA256_BLOCK_SIZE) {
		sha_ctx.size += size;
		return VB2_SUCCESS;
	}

	new_size = size - rem_size;
	block_nb = new_size / VB2_SHA256_BLOCK_SIZE;

	shifted_data = buf + rem_size;

	vb2_sha256_transform_x86ext(sha_ctx.block, 1);
	vb2_sha256_transform_x86ext(shifted_data, block_nb);

	rem_size = new_size % VB2_SHA256_BLOCK_SIZE;

	memcpy(sha_ctx.block, &shifted_data[block_nb << 6],
	       rem_size);

	sha_ctx.size = rem_size;
	sha_ctx.total_size += (block_nb + 1) << 6;
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_hwcrypto_digest_finalize(uint8_t *digest,
					   uint32_t digest_size)
{
	unsigned int block_nb;
	unsigned int pm_size;
	unsigned int size_b;
	if (digest_size != VB2_SHA256_DIGEST_SIZE) {
		VB2_DEBUG("ERROR: Digest size does not match expected length.\n");
		return VB2_ERROR_UNKNOWN;
	}

	block_nb = (1 + ((VB2_SHA256_BLOCK_SIZE - 9)
			 < (sha_ctx.size % VB2_SHA256_BLOCK_SIZE)));

	size_b = (sha_ctx.total_size + sha_ctx.size) << 3;
	pm_size = block_nb << 6;

	memset(sha_ctx.block + sha_ctx.size, 0, pm_size - sha_ctx.size);
	sha_ctx.block[sha_ctx.size] = 0x80;
	UNPACK32(size_b, sha_ctx.block + pm_size - 4);

	vb2_sha256_transform_x86ext(sha_ctx.block, block_nb);

	UNPACK32(sha_ctx.h[3], &digest[ 0]);
	UNPACK32(sha_ctx.h[2], &digest[ 4]);
	UNPACK32(sha_ctx.h[7], &digest[ 8]);
	UNPACK32(sha_ctx.h[6], &digest[12]);
	UNPACK32(sha_ctx.h[1], &digest[16]);
	UNPACK32(sha_ctx.h[0], &digest[20]);
	UNPACK32(sha_ctx.h[5], &digest[24]);
	UNPACK32(sha_ctx.h[4], &digest[28]);
	return VB2_SUCCESS;
}
