/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * SHA256 implementation using ARMv8 Cryptography Extension.
 */

#include "2common.h"
#include "2sha.h"
#include "2sha_private.h"
#include "2api.h"

struct vb2_sha256_context sha_ctx;

vb2_error_t vb2ex_hwcrypto_digest_init(enum vb2_hash_algorithm hash_alg,
				       uint32_t data_size)
{
	if (hash_alg != VB2_HASH_SHA256)
		return VB2_ERROR_EX_HWCRYPTO_UNSUPPORTED;

	sha_ctx.h[0] = vb2_sha256_h0[0];
	sha_ctx.h[1] = vb2_sha256_h0[1];
	sha_ctx.h[2] = vb2_sha256_h0[2];
	sha_ctx.h[3] = vb2_sha256_h0[3];
	sha_ctx.h[4] = vb2_sha256_h0[4];
	sha_ctx.h[5] = vb2_sha256_h0[5];
	sha_ctx.h[6] = vb2_sha256_h0[6];
	sha_ctx.h[7] = vb2_sha256_h0[7];
	sha_ctx.size = 0;
	sha_ctx.total_size = 0;
	memset(sha_ctx.block, 0, sizeof(sha_ctx.block));

	return VB2_SUCCESS;
}

int sha256_ce_transform(uint32_t *state, const unsigned char *buf, int blocks);

void vb2_sha256_transform_hwcrypto(const uint8_t *message,
				   unsigned int block_nb)
{
	sha256_ce_transform(sha_ctx.h, message, block_nb);
}

vb2_error_t vb2ex_hwcrypto_digest_finalize(uint8_t *digest,
					   uint32_t digest_size)
{
	unsigned int block_nb;
	unsigned int pm_size;
	unsigned int size_b;

	if (digest_size != VB2_SHA256_DIGEST_SIZE) {
		VB2_DEBUG("ERROR: Digest size does not match expected length.\n");
		return VB2_ERROR_SHA_FINALIZE_DIGEST_SIZE;
	}

	block_nb = (1 + ((VB2_SHA256_BLOCK_SIZE - SHA256_MIN_PAD_LEN)
			 < (sha_ctx.size % VB2_SHA256_BLOCK_SIZE)));

	size_b = (sha_ctx.total_size + sha_ctx.size) * 8;
	pm_size = block_nb * VB2_SHA256_BLOCK_SIZE;

	memset(sha_ctx.block + sha_ctx.size, 0, pm_size - sha_ctx.size);
	sha_ctx.block[sha_ctx.size] = SHA256_PAD_BEGIN;
	UNPACK32(size_b, sha_ctx.block + pm_size - 4);

	vb2_sha256_transform_hwcrypto(sha_ctx.block, block_nb);

	UNPACK32(sha_ctx.h[0], &digest[ 0]);
	UNPACK32(sha_ctx.h[1], &digest[ 4]);
	UNPACK32(sha_ctx.h[2], &digest[ 8]);
	UNPACK32(sha_ctx.h[3], &digest[12]);
	UNPACK32(sha_ctx.h[4], &digest[16]);
	UNPACK32(sha_ctx.h[5], &digest[20]);
	UNPACK32(sha_ctx.h[6], &digest[24]);
	UNPACK32(sha_ctx.h[7], &digest[28]);
	return VB2_SUCCESS;
}
