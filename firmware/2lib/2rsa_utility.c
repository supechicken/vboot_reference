/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Implementation of RSA utility functions.
 */

#include "2sysincludes.h"
#include "2api.h"
#include "2common.h"
#include "2rsa.h"

uint32_t vb2_rsa_sig_size(uint32_t algorithm)
{
	switch (algorithm) {
	case VB2_ALG_RSA1024_SHA1:
	case VB2_ALG_RSA1024_SHA256:
	case VB2_ALG_RSA1024_SHA512:
		return 1024 / 8;
	case VB2_ALG_RSA2048_SHA1:
	case VB2_ALG_RSA2048_SHA256:
	case VB2_ALG_RSA2048_SHA512:
		return 2048 / 8;
	case VB2_ALG_RSA4096_SHA1:
	case VB2_ALG_RSA4096_SHA256:
	case VB2_ALG_RSA4096_SHA512:
		return 4096 / 8;
	case VB2_ALG_RSA8192_SHA1:
	case VB2_ALG_RSA8192_SHA256:
	case VB2_ALG_RSA8192_SHA512:
		return 8192 / 8;
	default:
		return 0;
	}
}

uint32_t vb2_packed_key_size(uint32_t algorithm)
{
	if (algorithm >= VB2_ALG_COUNT)
		return 0;

	/*
	 * Total size needed by a RSAPublicKey buffer is =
	 *  2 * key_len bytes for the n and rr arrays
	 *  + sizeof len + sizeof n0inv.
	 */
	return 2 * vb2_rsa_sig_size(algorithm) + 2 * sizeof(uint32_t);
}
