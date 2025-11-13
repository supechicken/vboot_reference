/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for keyblock hash verification
 */

#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <stdio.h>

#include "2avb.h"
#include "2common.h"
#include "2struct.h"
#include "common/tests.h"
#include "host_key.h"

static const char * const key_len[] = {"1024", "2048", "4096", "8192"};
static int key_alg[] = {VB2_ALG_RSA1024_SHA256, VB2_ALG_RSA2048_SHA256, VB2_ALG_RSA4096_SHA256,
			VB2_ALG_RSA8192_SHA256};

static uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *vb2_ctx;
static struct vb2_shared_data *sd;
static uint8_t *avb_key_data;
static size_t avb_key_len;
static char *keys_dir;

static int prepare_cros_key(const char *filename, uint32_t alg)
{
	struct vb2_packed_key *test_key;

	test_key = vb2_read_packed_keyb(filename, alg, 1);
	if (!test_key) {
		fprintf(stderr, "Error reading test key\n");
		return -1;
	}
	sd->kernel_key_offset = sizeof(*sd);
	sd->kernel_key_size =
		sizeof(*test_key) + vb2_packed_key_size(vb2_crypto_to_signature(alg));
	memcpy(sd + 1, test_key, sd->kernel_key_size);
	free(test_key);

	return 0;
}

static int prepare_avb_key(const char *filename)
{
	FILE *fp;
	RSA *key;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Couldn't open file %s!\n", filename);
		return -1;
	}

	EVP_PKEY *pkey = PEM_read_PrivateKey(fp, NULL, NULL, NULL);
	if (!pkey) {
		key = PEM_read_RSA_PUBKEY(fp, NULL, NULL, NULL);
		if (!key) {
			fprintf(stderr, "Couldn't read public key file.\n");
			fclose(fp);
			return -1;
		}
	} else
		key = EVP_PKEY_get1_RSA(pkey);

	fclose(fp);

	BN_CTX *bn_ctx = BN_CTX_new();

	const BIGNUM *N = RSA_get0_n(key);
	BIGNUM *N0inv = BN_new();
	BIGNUM *B = BN_new();
	BIGNUM *Big2 = BN_new();
	BIGNUM *R = BN_new();
	BIGNUM *RR = BN_new();
	BIGNUM *RRTemp = BN_new();
	BIGNUM *NnumBits = BN_new();

	/* Calculate and output N0inv = -1 / N[0] mod 2^32 */
	BN_dec2bn(&B, "4294967296"); // 2^32
	BN_mod_inverse(N0inv, N, B, bn_ctx);
	BN_sub(N0inv, B, N0inv);

	/* Calculate R = 2^(# of key bits) */
	BN_set_word(Big2, 2L);
	BN_set_word(NnumBits, BN_num_bits(N));
	BN_exp(R, Big2, NnumBits, bn_ctx);

	/* Calculate RR = R^2 mod N */
	BN_copy(RR, R);
	BN_mul(RRTemp, RR, R, bn_ctx);
	BN_mod(RR, RRTemp, N, bn_ctx);

	/* Create avb public keRSA_get0_ny structure */
	uint32_t n0inv_word = BN_get_word(N0inv);
	int num_bits = BN_num_bits(N);
	int num_bytes = num_bits / 8;
	uint32_t num_bits_be = htobe32(num_bits);
	uint32_t n0inv_be = htobe32(n0inv_word);

	avb_key_len = 4 + 4 + num_bytes + num_bytes;
	avb_key_data = malloc(avb_key_len);

	memcpy(avb_key_data, &num_bits_be, 4);
	memcpy(avb_key_data + 4, &n0inv_be, 4);

	BN_bn2bin(N, avb_key_data + 8);
	BN_bn2bin(RR, avb_key_data + 8 + num_bytes);

	BN_free(B);
	BN_free(N0inv);
	BN_free(Big2);
	BN_free(R);
	BN_free(RR);
	BN_free(RRTemp);
	BN_free(NnumBits);
	RSA_free(key);

	return 0;
}

static int Setup(int key_num)
{
	char filename[256];

	snprintf(filename, sizeof(filename), "%s/key_rsa%s.keyb", keys_dir, key_len[key_num]);
	if (prepare_cros_key(filename, key_alg[key_num])) {
		fprintf(stderr, "Error preparing AVB key\n");
		return -1;
	}

	snprintf(filename, sizeof(filename), "%s/key_rsa%s.pem", keys_dir, key_len[key_num]);
	if (prepare_avb_key(filename)) {
		fprintf(stderr, "Error preparing AVB key\n");
		return -1;
	}

	return 0;
}

static void Clean(void)
{
	if (avb_key_data) {
		free(avb_key_data);
		avb_key_data = NULL;
	}

	avb_key_len = 0;
}

/* mocks */
bool avb_rsa_public_key_header_validate_and_byteswap(const AvbRSAPublicKeyHeader *src,
						     AvbRSAPublicKeyHeader *dest)
{
	memcpy(dest, src, sizeof(AvbRSAPublicKeyHeader));
	dest->key_num_bits = be32toh(dest->key_num_bits);
	dest->n0inv = be32toh(dest->n0inv);

	return true;
}

int main(int argc, char *argv[])
{
	AvbIOResult ret;
	bool key_is_trusted;
	AvbOps *avb_ops;
	int i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <keys_dir>", argv[0]);
		return -1;
	}

	keys_dir = argv[1];

	/* Set up context */
	if (vb2api_init(workbuf, sizeof(workbuf), &vb2_ctx)) {
		printf("Failed to initialize workbuf.\n");
		return -1;
	}

	sd = vb2_get_sd(vb2_ctx);
	avb_ops = vboot_avb_ops_new(vb2_ctx, NULL, NULL, NULL, NULL);

	for (i = 0; i < ARRAY_SIZE(key_len); i++) {

		// Successful validation
		TEST_EQ_S(Setup(i), 0);
		ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len,
							  NULL, 0, &key_is_trusted);
		TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
		TEST_EQ(key_is_trusted, true, "Key is trusted");
		Clean();

		// Key size lesser than required
		TEST_EQ_S(Setup(i), 0);
		avb_key_len = sizeof(AvbRSAPublicKeyHeader) - 1;
		ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len,
							  NULL, 0, &key_is_trusted);
		TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
		TEST_EQ(key_is_trusted, false, "Key rejected - incorrect key size");
		Clean();

		// n0inv corruption
		TEST_EQ_S(Setup(i), 0);
		avb_key_data[4] ^= avb_key_data[4];
		ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len,
							  NULL, 0, &key_is_trusted);
		TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
		TEST_EQ(key_is_trusted, false, "Key rejected - n0inv corrupted");
		Clean();

		// rr corruption
		TEST_EQ_S(Setup(i), 0);
		avb_key_data[avb_key_len - 1] ^= avb_key_data[avb_key_len - 1];
		ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len,
							  NULL, 0, &key_is_trusted);
		TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
		TEST_EQ(key_is_trusted, false, "Key rejected - rr corrupted");
		Clean();

		// n corruption
		TEST_EQ_S(Setup(i), 0);
		avb_key_data[sizeof(AvbRSAPublicKeyHeader)] ^=
			avb_key_data[sizeof(AvbRSAPublicKeyHeader)];
		ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len,
							  NULL, 0, &key_is_trusted);
		TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
		TEST_EQ(key_is_trusted, false, "Key rejected - n corrupted");
		Clean();
	}

	// Try 2 different keys of the same length
	char filename[256];
	snprintf(filename, sizeof(filename), "%s/key_rsa2048.keyb", keys_dir);
	TEST_EQ(prepare_cros_key(filename, VB2_ALG_RSA2048_SHA256), 0, "Prepare cros key");
	snprintf(filename, sizeof(filename), "%s/key_rsa2048_exp3.pem", keys_dir);
	TEST_EQ(prepare_avb_key(filename), 0, "Prepare avb key");
	ret = avb_ops->validate_vbmeta_public_key(avb_ops, avb_key_data, avb_key_len, NULL, 0,
						  &key_is_trusted);
	TEST_EQ(ret, AVB_IO_RESULT_OK, "validate_vbmeta_public_key - successful");
	TEST_EQ(key_is_trusted, false, "Key rejected - different keys");

	vboot_avb_ops_free(avb_ops);

	return gTestSuccess ? 0 : 255;
}
