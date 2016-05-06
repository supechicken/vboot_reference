/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_VBOOT_2HMAC_H_
#define VBOOT_REFERENCE_VBOOT_2HMAC_H_
#include <stdint.h>

int hmac_sha(const void *key, uint32_t key_size,
	     const void *msg, uint32_t msg_size,
	     enum vb2_hash_algorithm alg,
	     uint8_t *mac, uint32_t mac_size);

int hmac_sha1(const void *key, uint32_t key_size,
	      const void *msg, uint32_t msg_size,
	      uint8_t *mac, uint32_t mac_size);

int hmac_sha256(const void *key, uint32_t key_size,
		const void *msg, uint32_t msg_size,
		uint8_t *mac, uint32_t mac_size);

int hmac_sha512(const void *key, uint32_t key_size,
		const void *msg, uint32_t msg_size,
		uint8_t *mac, uint32_t mac_size);

#endif
