/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <stdint.h>
#include <stdio.h>

#define _STUB_IMPLEMENTATION_

#include "cryptolib.h"
#include "file_keys.h"
#include "rsa_padding_test.h"
#include "test_common.h"
#include "utility.h"
#include "vboot_api.h"

#include "2api.h"
#include "2rsa.h"

/**
 * Test RSA utility funcs
 */
static void test_utils(void)
{
	/* Verify old and new algorithm count constants match */
	TEST_EQ(kNumAlgorithms, VB2_ALG_COUNT, "Algorithm counts");

	/* Sig size */
	TEST_EQ(vb2_rsa_sig_size(VB2_ALG_RSA1024_SHA1), RSA1024NUMBYTES,
		"Sig size VB2_ALG_RSA1024_SHA1");
	TEST_EQ(vb2_rsa_sig_size(VB2_ALG_RSA2048_SHA1), RSA2048NUMBYTES,
		"Sig size VB2_ALG_RSA2048_SHA1");
	TEST_EQ(vb2_rsa_sig_size(VB2_ALG_RSA4096_SHA256), RSA4096NUMBYTES,
		"Sig size VB2_ALG_RSA4096_SHA256");
	TEST_EQ(vb2_rsa_sig_size(VB2_ALG_RSA8192_SHA512), RSA8192NUMBYTES,
		"Sig size VB2_ALG_RSA8192_SHA512");
	TEST_EQ(vb2_rsa_sig_size(VB2_ALG_COUNT), 0,
		"Sig size invalid algorithm");

	/* Packed key size */
	TEST_EQ(vb2_packed_key_size(VB2_ALG_RSA1024_SHA1),
		RSA1024NUMBYTES * 2 + sizeof(uint32_t) * 2,
		"Packed key size VB2_ALG_RSA1024_SHA1");
	TEST_EQ(vb2_packed_key_size(VB2_ALG_RSA2048_SHA1),
		RSA2048NUMBYTES * 2 + sizeof(uint32_t) * 2,
		"Packed key size VB2_ALG_RSA2048_SHA1");
	TEST_EQ(vb2_packed_key_size(VB2_ALG_RSA4096_SHA256),
		RSA4096NUMBYTES * 2 + sizeof(uint32_t) * 2,
		"Packed key size VB2_ALG_RSA4096_SHA256");
	TEST_EQ(vb2_packed_key_size(VB2_ALG_RSA8192_SHA512),
		RSA8192NUMBYTES * 2 + sizeof(uint32_t) * 2,
		"Packed key size VB2_ALG_RSA8192_SHA512");
	TEST_EQ(vb2_packed_key_size(VB2_ALG_COUNT), 0,
		"Packed key size invalid algorithm");
}

int main(int argc, char* argv[])
{
	/* Run tests */
	test_utils();

	return gTestSuccess ? 0 : 255;
}
