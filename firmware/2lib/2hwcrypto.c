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

	vb2_sha256_transform_hwcrypto(sha_ctx.block, 1);
	vb2_sha256_transform_hwcrypto(shifted_data, remaining_blocks);

	rem_size = new_size % VB2_SHA256_BLOCK_SIZE;

	memcpy(sha_ctx.block, &shifted_data[remaining_blocks * VB2_SHA256_BLOCK_SIZE],
	       rem_size);

	sha_ctx.size = rem_size;
	sha_ctx.total_size += (remaining_blocks + 1) * VB2_SHA256_BLOCK_SIZE;
	return VB2_SUCCESS;
}
