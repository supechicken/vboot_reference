/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware image library.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "file_keys.h"
#include "host_common.h"
#include "vboot_common.h"
#include "test_common.h"

#include "2common.h"
#include "2rsa.h"

static void test_unpack_key(const VbPublicKey *orig_key)
{
	struct vb2_public_key rsa;
	VbPublicKey *key = PublicKeyAlloc(orig_key->key_size, 0, 0);

	/* vb2_packed_key and VbPublicKey are bit-identical */
	struct vb2_packed_key *key2 = (struct vb2_packed_key *)key;
	uint8_t *buf = (uint8_t *)key;

	/*
	 * Key data follows the header for a newly allocated key, so we can
	 * calculate the buffer size by looking at how far the key data goes.
	 */
	uint32_t size = key2->key_offset + key2->key_size;

	PublicKeyCopy(key, orig_key);
	TEST_SUCC(vb2_unpack_key(&rsa, buf, size), "vb2_unpack_key() ok");

	TEST_EQ(rsa.algorithm, key2->algorithm, "vb2_unpack_key() algorithm");

	PublicKeyCopy(key, orig_key);
	key2->algorithm = VB2_ALG_INVALID;
	TEST_EQ(vb2_unpack_key(&rsa, buf, size),
		VB2_ERROR_UNPACK_KEY_ALGORITHM,
		"vb2_unpack_key() invalid algorithm");

	/* Unsigned SHA algorithms don't have keys */
	PublicKeyCopy(key, orig_key);
	key2->algorithm = VB2_ALG_SHA256;
	TEST_EQ(vb2_unpack_key(&rsa, buf, size),
		VB2_ERROR_UNPACK_KEY_ALGORITHM,
		"vb2_unpack_key() not valid for unsigned SHA");

	PublicKeyCopy(key, orig_key);
	key2->key_size--;
	TEST_EQ(vb2_unpack_key(&rsa, buf, size),
		VB2_ERROR_UNPACK_KEY_SIZE,
		"vb2_unpack_key() invalid size");
	key2->key_size++;

	PublicKeyCopy(key, orig_key);
	key2->key_offset++;
	TEST_EQ(vb2_unpack_key(&rsa, buf, size + 1),
		VB2_ERROR_UNPACK_KEY_ALIGN,
		"vb2_unpack_key() unaligned data");
	key2->key_offset--;

	PublicKeyCopy(key, orig_key);
	*(uint32_t *)(buf + key2->key_offset) /= 2;
	TEST_EQ(vb2_unpack_key(&rsa, buf, size),
		VB2_ERROR_UNPACK_KEY_ARRAY_SIZE,
		"vb2_unpack_key() invalid key array size");

	PublicKeyCopy(key, orig_key);
	TEST_EQ(vb2_unpack_key(&rsa, buf, size - 1),
		VB2_ERROR_INSIDE_DATA_OUTSIDE,
		"vb2_unpack_key() buffer too small");

	free(key);
}

static void test_verify_data_inner(const uint8_t *test_data,
				   uint32_t test_size,
				   struct vb2_signature *sig,
				   struct vb2_public_key *rsa,
				   uint32_t wrong_sig_error)
{
	uint8_t workbuf[VB2_VERIFY_DATA_WORKBUF_BYTES];
	struct vb2_workbuf wb;
	struct vb2_signature *sig2;
	uint32_t real_alg = rsa->algorithm;;

	vb2_workbuf_init(&wb, workbuf, sizeof(workbuf));

	/* Allocate signature copy for tests */
	sig2 = (struct vb2_signature *)SignatureAlloc(sig->sig_size, 0);

	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	rsa->algorithm = VB2_ALG_INVALID;
	TEST_EQ(vb2_verify_data(test_data, test_size, sig2, rsa, &wb),
		VB2_ERROR_VDATA_DIGEST_SIZE, "vb2_verify_data() bad key");
	rsa->algorithm = real_alg;

	vb2_workbuf_init(&wb, workbuf, 4);
	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	TEST_EQ(vb2_verify_data(test_data, test_size, sig2, rsa, &wb),
		VB2_ERROR_VDATA_WORKBUF_DIGEST,
		"vb2_verify_data() workbuf too small");
	vb2_workbuf_init(&wb, workbuf, sizeof(workbuf));

	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	TEST_EQ(vb2_verify_data(test_data, test_size, sig2, rsa, &wb),
		0, "vb2_verify_data() ok");

	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	sig2->sig_size -= 16;
	TEST_EQ(vb2_verify_data(test_data, test_size, sig2, rsa, &wb),
		VB2_ERROR_VDATA_SIG_SIZE, "vb2_verify_data() wrong sig size");

	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	TEST_EQ(vb2_verify_data(test_data, test_size - 1, sig2, rsa, &wb),
		VB2_ERROR_VDATA_NOT_ENOUGH_DATA,
		"vb2_verify_data() input buffer too small");

	memcpy(sig2, sig, sizeof(*sig) + sig->sig_size);
	vb2_signature_data(sig2)[0] ^= 0x5A;
	TEST_EQ(vb2_verify_data(test_data, test_size, sig2, rsa, &wb),
		wrong_sig_error, "vb2_verify_data() wrong sig");

	free(sig2);
}

static void test_verify_data(const VbPublicKey *public_key,
			     const VbPrivateKey *private_key)
{
	const uint8_t test_data[] = "This is some test data to sign.";
	struct vb2_signature *sig;
	struct vb2_public_key rsa;
	struct vb2_packed_key *public_key2;

	/* Vb2 structs are bit-identical to the old ones */
	public_key2 = (struct vb2_packed_key *)public_key;
	uint32_t pubkey_size = public_key2->key_offset + public_key2->key_size;

	/* Calculate good signature */
	sig = (struct vb2_signature *)CalculateSignature(
		 test_data, sizeof(test_data), private_key);
	TEST_PTR_NEQ(sig, 0, "VerifyData() calculate signature");
	if (!sig)
		return;

	TEST_EQ(vb2_unpack_key(&rsa, (uint8_t *)public_key2, pubkey_size),
		0, "vb2_verify_data() unpack key");

	test_verify_data_inner(test_data, sizeof(test_data), sig, &rsa,
			       VB2_ERROR_RSA_PADDING);

	free(sig);
}

static int test_algorithm(int key_algorithm, const char *keys_dir)
{
	char filename[1024];
	int rsa_len = vb2_rsa_sig_size(key_algorithm) * 8;

	VbPrivateKey *private_key = NULL;
	VbPublicKey *public_key = NULL;

	printf("***Testing algorithm: %s\n", algo_strings[key_algorithm]);

	sprintf(filename, "%s/key_rsa%d.pem", keys_dir, rsa_len);
	private_key = PrivateKeyReadPem(filename, key_algorithm);
	if (!private_key) {
		fprintf(stderr, "Error reading private_key: %s\n", filename);
		return 1;
	}

	sprintf(filename, "%s/key_rsa%d.keyb", keys_dir, rsa_len);
	public_key = PublicKeyReadKeyb(filename, key_algorithm, 1);
	if (!public_key) {
		fprintf(stderr, "Error reading public_key: %s\n", filename);
		return 1;
	}

	test_unpack_key(public_key);
	test_verify_data(public_key, private_key);

	if (public_key)
		free(public_key);
	if (private_key)
		free(private_key);

	return 0;
}

static void test_hash_algorithm(int algorithm)
{
	const uint8_t test_data[] = "This is some test data to sign.";
	uint8_t test_sig_data[sizeof(struct vb2_signature) +
			      VB2_SHA512_DIGEST_SIZE];
	struct vb2_signature *sig = (struct vb2_signature *)test_sig_data;
	struct vb2_public_key key;
	struct vb2_digest_context dc;

	/* Set up signature */
	printf("*** Testing hash algorithm: %d\n", algorithm);
	memset(&key, 0, sizeof(key));
	key.algorithm = algorithm;

	memset(test_sig_data, 0, sizeof(test_sig_data));
	sig->sig_offset = sizeof(*sig);
	sig->sig_size = vb2_digest_size(algorithm);
	TEST_TRUE(sig->sig_size + sizeof(sig) <= sizeof(test_sig_data),
		  "signature fits in test buffer");
	sig->data_size = sizeof(test_data);

	TEST_EQ(vb2_digest_init(&dc, algorithm), 0, "digest init");
	TEST_EQ(vb2_digest_extend(&dc, test_data, sizeof(test_data)), 0,
		"digest extend");
	TEST_EQ(vb2_digest_finalize(&dc, test_sig_data + sig->sig_offset,
				    sig->sig_size), 0, "digest finalize");

	test_verify_data_inner(test_data, sizeof(test_data), sig, &key,
			       VB2_ERROR_VDATA_BAD_DIGEST);
}

/* Test only the algorithms we use */
const int key_algs[] = {
	VB2_ALG_RSA2048_SHA256,
	VB2_ALG_RSA4096_SHA256,
	VB2_ALG_RSA8192_SHA512,
};

int main(int argc, char *argv[]) {
	int alg;

	if (argc == 2) {
		int i;

		/* Test commonly used RSA algorithms */
		for (i = 0; i < ARRAY_SIZE(key_algs); i++) {
			if (test_algorithm(key_algs[i], argv[1]))
				return 1;
		}

	} else if (argc == 3 && !strcasecmp(argv[2], "--all")) {
		/* Test all the RSA algorithms */
		for (alg = VB2_ALG_RSA1024_SHA1;
		     alg <= VB2_ALG_RSA8192_SHA512; alg++) {
			if (test_algorithm(alg, argv[1]))
				return 1;
		}

	} else {
		fprintf(stderr, "Usage: %s <keys_dir> [--all]", argv[0]);
		return -1;
	}

	/* Test bare hashes */
	for (alg = VB2_ALG_SHA1; alg <= VB2_ALG_SHA512; alg++)
		test_hash_algorithm(alg);

	return gTestSuccess ? 0 : 255;
}
