/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <openssl/hmac.h>

#include "2sha.h"
#include "2hmac.h"
#include "test_common.h"

static void test_hmac_sha1_by_openssl (const void *key, uint32_t key_size,
				       const void *msg, uint32_t msg_size)
{
	uint8_t mac[VB2_SHA1_DIGEST_SIZE];
	uint32_t mac_size = sizeof(mac);
	uint8_t md[VB2_SHA1_DIGEST_SIZE];
	uint32_t md_size = sizeof(md);

	TEST_SUCC(hmac_sha1(key, key_size, msg, msg_size, mac, mac_size),
		  "hmac_sha1 success");
	HMAC(EVP_sha1(), key, key_size, msg, msg_size, md, &md_size);
	TEST_SUCC(memcmp(mac, md, md_size), __func__);
}

static void test_hmac_sha256_by_openssl (const void *key, uint32_t key_size,
					 const void *msg, uint32_t msg_size)
{
	uint8_t mac[VB2_SHA256_DIGEST_SIZE];
	uint32_t mac_size = sizeof(mac);
	uint8_t md[VB2_SHA256_DIGEST_SIZE];
	uint32_t md_size = sizeof(md);

	TEST_SUCC(hmac_sha256(key, key_size, msg, msg_size, mac, mac_size),
		  "hmac_sha256 success");
	HMAC(EVP_sha256(), key, key_size, msg, msg_size, md, &md_size);
	TEST_SUCC(memcmp(mac, md, md_size), __func__);
}

static void test_hmac_sha512_by_openssl (const void *key, uint32_t key_size,
					 const void *msg, uint32_t msg_size)
{
	uint8_t mac[VB2_SHA512_DIGEST_SIZE];
	uint32_t mac_size = sizeof(mac);
	uint8_t md[VB2_SHA512_DIGEST_SIZE];
	uint32_t md_size = sizeof(md);

	TEST_SUCC(hmac_sha512(key, key_size, msg, msg_size, mac, mac_size),
		  "hmac_sha512 success");
	HMAC(EVP_sha512(), key, key_size, msg, msg_size, md, &md_size);
	TEST_SUCC(memcmp(mac, md, md_size), __func__);
}

static void test_hmac_sha(void)
{
	char *k, *m;
	uint8_t mac;
	enum vb2_hash_algorithm alg;

	k = "key";
	m = "The quick brown fox jumps over the lazy dog";
	alg = VB2_HASH_SHA1;
	TEST_TRUE(hmac_sha(NULL, 0, m, strlen(m), alg, &mac, 0), "key=NULL");
	TEST_TRUE(hmac_sha(k, strlen(k), NULL, 0, alg, &mac, 0), "msg=NULL");
	TEST_TRUE(hmac_sha(k, strlen(k), m, strlen(m), alg, NULL, 0), "mac=NULL");
	TEST_TRUE(hmac_sha(k, strlen(k), m, strlen(m), alg, &mac, 0),
		  "Buffer too small");
	alg = -1;
	TEST_TRUE(hmac_sha(k, strlen(k), m, strlen(m), alg, &mac, 0),
		  "Invalid algorithm");
}

static void test_hmac_sha1(void)
{
	char *k, *m;

	k = "key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha1_by_openssl(k, strlen(k), m, strlen(m));

	k = "loooooooooooooooooooooooooooooooooooooooooooonoooooooooooooog key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha1_by_openssl(k, strlen(k), m, strlen(m));

	k = "";
	m = "";
	test_hmac_sha1_by_openssl(k, strlen(k), m, strlen(m));
}

static void test_hmac_sha256(void)
{
	char *k, *m;

	k = "key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha256_by_openssl(k, strlen(k), m, strlen(m));

	k = "loooooooooooooooooooooooooooooooooooooooooooonoooooooooooooog key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha256_by_openssl(k, strlen(k), m, strlen(m));

	k = "";
	m = "";
	test_hmac_sha256_by_openssl(k, strlen(k), m, strlen(m));
}

static void test_hmac_sha512(void)
{
	char *k, *m;

	k = "key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha512_by_openssl(k, strlen(k), m, strlen(m));

	k = "loooooooooooooooooooooooooooooooooooooooooooonoooooooooooooog key";
	m = "The quick brown fox jumps over the lazy dog";
	test_hmac_sha512_by_openssl(k, strlen(k), m, strlen(m));

	k = "";
	m = "";
	test_hmac_sha512_by_openssl(k, strlen(k), m, strlen(m));
}

int main(void)
{
	test_hmac_sha();
	test_hmac_sha1();
	test_hmac_sha256();
	test_hmac_sha512();

	return 0;
}
