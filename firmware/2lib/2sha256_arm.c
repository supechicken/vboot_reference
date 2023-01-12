/*
 *  FIPS-180-2 compliant SHA-256 implementation
 *
 *  Copyright The Mbed TLS Contributors
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */
/*
 *  The SHA-256 Secure Hash Standard was published by NIST in 2002.
 *
 *  http://csrc.nist.gov/publications/fips/fips180-2/fips180-2.pdf
 */

#include "2common.h"
#include "2sha.h"
#include "2sha_private.h"
#include "2api.h"

static struct vb2_sha256_context sha_ctx;

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

static void vb2_sha256_transform_arm(const uint8_t *message,
				     unsigned int block_nb)
{
	sha256_ce_transform(sha_ctx.h, message, block_nb);
}

vb2_error_t vb2ex_hwcrypto_digest_extend(const uint8_t *buf, uint32_t size)
{
	unsigned int remaining_blocks;
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
	remaining_blocks = new_size / VB2_SHA256_BLOCK_SIZE;

	shifted_data = buf + rem_size;

	vb2_sha256_transform_arm(sha_ctx.block, 1);
	vb2_sha256_transform_arm(shifted_data, remaining_blocks);

	rem_size = new_size % VB2_SHA256_BLOCK_SIZE;

	memcpy(sha_ctx.block, &shifted_data[remaining_blocks * VB2_SHA256_BLOCK_SIZE],
	       rem_size);

	sha_ctx.size = rem_size;
	sha_ctx.total_size += (remaining_blocks + 1) * VB2_SHA256_BLOCK_SIZE;
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
		return VB2_ERROR_SHA_FINALIZE_DIGEST_SIZE;
	}

	block_nb = (1 + ((VB2_SHA256_BLOCK_SIZE - 9)
			 < (sha_ctx.size % VB2_SHA256_BLOCK_SIZE)));

	size_b = (sha_ctx.total_size + sha_ctx.size) << 3;
	pm_size = block_nb << 6;

	memset(sha_ctx.block + sha_ctx.size, 0, pm_size - sha_ctx.size);
	sha_ctx.block[sha_ctx.size] = 0x80;
	UNPACK32(size_b, sha_ctx.block + pm_size - 4);

	vb2_sha256_transform_arm(sha_ctx.block, block_nb);

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
