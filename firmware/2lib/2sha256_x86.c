/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SHA256 implementation using x86 SHA extension.
 * Mainly from https://github.com/noloader/SHA-Intrinsics/blob/master/sha256-x86.c,
 * Written and place in public domain by Jeffrey Walton
 * Based on code from Intel, and by Sean Gulley for
 * the miTLS project.
 */
#include "2common.h"
#include "2sha.h"
#include "2sha256.h"
#include "2api.h"

/* immintrin.h needs EINVAL to be defined */
#define EINVAL 22
#include <immintrin.h>

static struct vb2_sha256_context sha_ctx;

#define SHA256_X86_PUT_STATE1(j, i) 					\
	{								\
		msgtmp[j] = _mm_loadu_si128((__m128i *)			\
				(message + (i << 6) + (j * 16)));	\
		msgtmp[j] = _mm_shuffle_epi8(msgtmp[j], shuf_mask);	\
		msg = _mm_add_epi32(msgtmp[j],				\
			_mm_loadu_si128((__m128i *)&sha256_k[j * 4]));	\
		state1 = _mm_sha256rnds2_epu32(state1, state0, msg);	\
	}

#define SHA256_X86_PUT_STATE0()						\
	{								\
		msg    = _mm_shuffle_epi32(msg, 0x0E);			\
		state0 = _mm_sha256rnds2_epu32(state0, state1, msg);	\
	}

#define SHA256_X86_LOOP(j)						\
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
		SHA256_X86_PUT_STATE0();				\
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
		SHA256_X86_PUT_STATE0();

		SHA256_X86_PUT_STATE1(1, i);
		SHA256_X86_PUT_STATE0();
		msgtmp[0] = _mm_sha256msg1_epu32(msgtmp[0], msgtmp[1]);

		SHA256_X86_PUT_STATE1(2, i);
		SHA256_X86_PUT_STATE0();
		msgtmp[1] = _mm_sha256msg1_epu32(msgtmp[1], msgtmp[2]);

		SHA256_X86_PUT_STATE1(3, i);
		tmp = _mm_alignr_epi8(msgtmp[3], msgtmp[2], 4);
		msgtmp[0] = _mm_add_epi32(msgtmp[0], tmp);
		msgtmp[0] = _mm_sha256msg2_epu32(msgtmp[0], msgtmp[3]);
		SHA256_X86_PUT_STATE0();
		msgtmp[2] = _mm_sha256msg1_epu32(msgtmp[2], msgtmp[3]);

		SHA256_X86_LOOP(4);
		SHA256_X86_LOOP(5);
		SHA256_X86_LOOP(6);
		SHA256_X86_LOOP(7);
		SHA256_X86_LOOP(8);
		SHA256_X86_LOOP(9);
		SHA256_X86_LOOP(10);
		SHA256_X86_LOOP(11);
		SHA256_X86_LOOP(12);
		SHA256_X86_LOOP(13);
		SHA256_X86_LOOP(14);

		msg = _mm_add_epi32(msgtmp[3],
			_mm_loadu_si128((__m128i *)&sha256_k[15 * 4]));
		state1 = _mm_sha256rnds2_epu32(state1, state0, msg);
		SHA256_X86_PUT_STATE0();

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
