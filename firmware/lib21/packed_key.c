/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Key unpacking functions
 */

#include "2common.h"
#include "2rsa.h"
#include "2sysincludes.h"
#include "vb2_common.h"
#include "vb21_common.h"

vb2_error_t vb21_unpack_key(struct vb2_public_key *key, const uint8_t *buf,
			    uint32_t size)
{
	const struct vb21_packed_key *pkey =
		(const struct vb21_packed_key *)buf;
	uint32_t sig_size;
	uint32_t min_offset = 0;
	vb2_error_t rv;

	/* Check magic number */
	if (pkey->c.magic != VB21_MAGIC_PACKED_KEY)
		return VB2_ERROR_UNPACK_KEY_MAGIC;

	rv = vb21_verify_common_header(buf, size);
	if (rv)
		return rv;

	/* Make sure key data is inside */
	rv = vb21_verify_common_member(pkey, &min_offset,
				       pkey->key_offset, pkey->key_size);
	if (rv)
		return rv;

	/*
	 * Check for compatible version.  No need to check minor version, since
	 * that's compatible across readers matching the major version, and we
	 * haven't added any new fields.
	 */
	if (pkey->c.struct_version_major != VB21_PACKED_KEY_VERSION_MAJOR)
		return VB2_ERROR_UNPACK_KEY_STRUCT_VERSION;

	/* Copy key algorithms */
	key->hash_alg = pkey->hash_alg;
	if (!vb2_digest_size(key->hash_alg))
		return VB2_ERROR_UNPACK_KEY_HASH_ALGORITHM;

	key->sig_alg = pkey->sig_alg;
	if (key->sig_alg != VB2_SIG_NONE) {
		sig_size = vb2_rsa_sig_size(key->sig_alg);
		if (!sig_size)
			return VB2_ERROR_UNPACK_KEY_SIG_ALGORITHM;
		rv = vb2_unpack_key_buffer(
				key,
				(const uint8_t *)pkey + pkey->key_offset,
				pkey->key_size);
		if (rv)
			return rv;
	}

	/* Key description */
	key->desc = vb21_common_desc(pkey);
	key->version = pkey->key_version;
	key->id = &pkey->id;

	return VB2_SUCCESS;
}
