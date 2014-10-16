/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Utility functions for message digest functions.
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2rsa.h"
#include "2sha.h"

/**
 * Convert key algorithm to hash algorithm.
 */
static uint32_t vb2_hash_alg(uint32_t algorithm)
{
	switch(algorithm) {
	case VB2_ALG_RSA1024_SHA1:
	case VB2_ALG_RSA2048_SHA1:
	case VB2_ALG_RSA4096_SHA1:
	case VB2_ALG_RSA8192_SHA1:
	case VB2_ALG_SHA1:
		return VB2_ALG_SHA1;

	case VB2_ALG_RSA1024_SHA256:
	case VB2_ALG_RSA2048_SHA256:
	case VB2_ALG_RSA4096_SHA256:
	case VB2_ALG_RSA8192_SHA256:
	case VB2_ALG_SHA256:
		return VB2_ALG_SHA256;

	case VB2_ALG_RSA1024_SHA512:
	case VB2_ALG_RSA2048_SHA512:
	case VB2_ALG_RSA4096_SHA512:
	case VB2_ALG_RSA8192_SHA512:
	case VB2_ALG_SHA512:
		return VB2_ALG_SHA512;

	default:
		return VB2_ALG_INVALID;
	}
}

int vb2_digest_size(uint32_t algorithm)
{
	switch (vb2_hash_alg(algorithm)) {
#if VB2_SUPPORT_SHA1
	case VB2_ALG_SHA1:
		return VB2_SHA1_DIGEST_SIZE;
#endif
#if VB2_SUPPORT_SHA256
	case VB2_ALG_SHA256:
		return VB2_SHA256_DIGEST_SIZE;
#endif
#if VB2_SUPPORT_SHA512
	case VB2_ALG_SHA512:
		return VB2_SHA512_DIGEST_SIZE;
#endif
	default:
		return 0;
	}
}

int vb2_digest_init(struct vb2_digest_context *dc, uint32_t algorithm)
{
	dc->algorithm = algorithm;

	switch (vb2_hash_alg(dc->algorithm)) {
#if VB2_SUPPORT_SHA1
	case VB2_ALG_SHA1:
		vb2_sha1_init(&dc->sha1);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA256
	case VB2_ALG_SHA256:
		vb2_sha256_init(&dc->sha256);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA512
	case VB2_ALG_SHA512:
		vb2_sha512_init(&dc->sha512);
		return VB2_SUCCESS;
#endif
	default:
		return VB2_ERROR_SHA_INIT_ALGORITHM;
	}
}

int vb2_digest_extend(struct vb2_digest_context *dc,
		      const uint8_t *buf,
		      uint32_t size)
{
	switch (vb2_hash_alg(dc->algorithm)) {
#if VB2_SUPPORT_SHA1
	case VB2_ALG_SHA1:
		vb2_sha1_update(&dc->sha1, buf, size);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA256
	case VB2_ALG_SHA256:
		vb2_sha256_update(&dc->sha256, buf, size);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA512
	case VB2_ALG_SHA512:
		vb2_sha512_update(&dc->sha512, buf, size);
		return VB2_SUCCESS;
#endif
	default:
		return VB2_ERROR_SHA_EXTEND_ALGORITHM;
	}
}

int vb2_digest_finalize(struct vb2_digest_context *dc,
			uint8_t *digest,
			uint32_t digest_size)
{
	if (digest_size < vb2_digest_size(dc->algorithm))
		return VB2_ERROR_SHA_FINALIZE_DIGEST_SIZE;

	switch (vb2_hash_alg(dc->algorithm)) {
#if VB2_SUPPORT_SHA1
	case VB2_ALG_SHA1:
		vb2_sha1_finalize(&dc->sha1, digest);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA256
	case VB2_ALG_SHA256:
		vb2_sha256_finalize(&dc->sha256, digest);
		return VB2_SUCCESS;
#endif
#if VB2_SUPPORT_SHA512
	case VB2_ALG_SHA512:
		vb2_sha512_finalize(&dc->sha512, digest);
		return VB2_SUCCESS;
#endif
	default:
		return VB2_ERROR_SHA_FINALIZE_ALGORITHM;
	}
}
