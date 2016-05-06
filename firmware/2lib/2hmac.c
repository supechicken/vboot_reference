/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2sysincludes.h"
#include "2sha.h"
#include "2hmac.h"

int hmac_sha(const void *key, uint32_t key_size,
	     const void *msg, uint32_t msg_size,
	     enum vb2_hash_algorithm alg,
	     uint8_t *mac, uint32_t mac_size)
{
	const uint32_t max_block_size = VB2_SHA512_BLOCK_SIZE;
	const uint32_t max_digest_size = VB2_SHA512_DIGEST_SIZE;
	uint32_t block_size;
	uint32_t digest_size;
	uint8_t k[max_block_size];
	uint8_t o_pad[max_block_size];
	uint8_t i_pad[max_block_size];
	uint8_t b[max_digest_size];
	struct vb2_digest_context dc;
	int i;

	if (!key | !msg | !mac)
		return -1;

	switch (alg) {
	case VB2_HASH_SHA1:
		block_size = VB2_SHA1_BLOCK_SIZE;
		digest_size = VB2_SHA1_DIGEST_SIZE;
		break;
	case VB2_HASH_SHA256:
		block_size = VB2_SHA256_BLOCK_SIZE;
		digest_size = VB2_SHA256_DIGEST_SIZE;
		break;
	case VB2_HASH_SHA512:
		block_size = VB2_SHA512_BLOCK_SIZE;
		digest_size = VB2_SHA512_DIGEST_SIZE;
		break;
	default:
		return -1;
	}

	if (mac_size < digest_size)
		return -1;

	if (key_size > block_size) {
		vb2_digest_buffer((uint8_t *)key, key_size, alg, k, block_size);
		key_size = digest_size;
	} else {
		memcpy(k, key, key_size);
	}
	if (key_size < block_size)
		memset(k + key_size, 0, block_size - key_size);

	for (i = 0; i < block_size; i++) {
		o_pad[i] = 0x5c ^ k[i];
		i_pad[i] = 0x36 ^ k[i];
	}

	vb2_digest_init(&dc, alg);
	vb2_digest_extend(&dc, i_pad, block_size);
	vb2_digest_extend(&dc, msg, msg_size);
	vb2_digest_finalize(&dc, b, digest_size);

	vb2_digest_init(&dc, alg);
	vb2_digest_extend(&dc, o_pad, block_size);
	vb2_digest_extend(&dc, b, digest_size);
	vb2_digest_finalize(&dc, mac, mac_size);

	return 0;
}

int hmac_sha1(const void *key, uint32_t key_size,
	      const void *msg, uint32_t msg_size,
	      uint8_t *mac, uint32_t mac_size)
{
	return hmac_sha(key, key_size, msg, msg_size, VB2_HASH_SHA1,
			mac, mac_size);
}

int hmac_sha256(const void *key, uint32_t key_size,
		const void *msg, uint32_t msg_size,
		uint8_t *mac, uint32_t mac_size)
{
	return hmac_sha(key, key_size, msg, msg_size, VB2_HASH_SHA256,
			mac, mac_size);
}

int hmac_sha512(const void *key, uint32_t key_size,
		const void *msg, uint32_t msg_size,
		uint8_t *mac, uint32_t mac_size)
{
	return hmac_sha(key, key_size, msg, msg_size, VB2_HASH_SHA512,
			mac, mac_size);
}
