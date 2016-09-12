/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/aes.h>
#include "bdb_api.h"
#include "host.h"
#include "secrets.h"
#include "test_common.h"

const uint8_t expected_bdb[] = {
		0xb7, 0x4f, 0x67, 0xbe, 0xf6, 0x3f, 0x0b, 0xd2,
		0xa2, 0x81, 0xe5, 0xf3, 0x62, 0x81, 0x13, 0xa9,
		0x7b, 0x60, 0xdf, 0xf6, 0x74, 0x9c, 0x79, 0x08,
		0x38, 0xaf, 0xfe, 0x36, 0x4c, 0xbd, 0x07, 0xe7,
};
const uint8_t expected_boot_path[] = {
		0xb7, 0x4f, 0x67, 0xbe, 0xf6, 0x3f, 0x0b, 0xd2,
		0xa2, 0x81, 0xe5, 0xf3, 0x62, 0x81, 0x13, 0xa9,
		0x7b, 0x60, 0xdf, 0xf6, 0x74, 0x9c, 0x79, 0x08,
		0x38, 0xaf, 0xfe, 0x36, 0x4c, 0xbd, 0x07, 0xe7,
};
const uint8_t expected_boot_verified_fv1[] = {
		0x25, 0xb8, 0x37, 0x31, 0xdb, 0xa4, 0x3e, 0x6f,
		0xe6, 0x1f, 0x53, 0x97, 0x4c, 0x8b, 0x5f, 0x17,
		0x7c, 0xf4, 0x9e, 0x87, 0x2a, 0xc1, 0xcf, 0xe1,
		0x93, 0x92, 0x1e, 0x39, 0xe6, 0xb7, 0x77, 0xad,
};
const uint8_t expected_boot_verified_fv0[] = {
		0xb4, 0x83, 0x2d, 0x95, 0x48, 0xe0, 0x9e, 0x6a,
		0xf7, 0xa1, 0x42, 0xd2, 0x1a, 0xb4, 0x6c, 0x97,
		0xd0, 0xc2, 0x63, 0x82, 0xe7, 0xbb, 0x20, 0x37,
		0xe3, 0x9a, 0x4b, 0x4c, 0x6a, 0xc8, 0x0a, 0xd7,
};
const uint8_t expected_nvm_wp[] = {
		0xd2, 0x1f, 0x72, 0xe2, 0xe0, 0xe8, 0xd4, 0xd5,
		0x67, 0xab, 0x77, 0x3b, 0xea, 0x51, 0x44, 0xa4,
		0xc8, 0xb1, 0xdd, 0x4d, 0xb0, 0x46, 0xa9, 0x68,
		0x4f, 0xd8, 0xea, 0xcf, 0xee, 0xb9, 0xa7, 0xc5,
};
const uint8_t expected_nvm_rw[] = {
		0x11, 0xa7, 0xd7, 0x19, 0xf6, 0x8e, 0x4d, 0xbd,
		0x0f, 0x47, 0x1c, 0x1d, 0x68, 0x10, 0xce, 0xef,
		0x11, 0x43, 0x47, 0x0e, 0x9d, 0xd6, 0xb1, 0x8a,
		0x8b, 0x56, 0x63, 0x09, 0x7e, 0x4b, 0x5d, 0x7e,
};
const uint8_t expected_wsr[] = {
		0x8e, 0x7f, 0x2b, 0xe7, 0xed, 0x9f, 0x8a, 0x69,
		0x6d, 0x67, 0x2a, 0x59, 0xf0, 0x2b, 0x7d, 0x0b,
		0x6f, 0xbb, 0x96, 0x2b, 0x16, 0x75, 0xce, 0x8f,
		0x13, 0x71, 0xd7, 0x63, 0xf9, 0x97, 0x25, 0x38,
};

static void dump_secret(const uint8_t *secret, const char *label)
{
	int i;
	printf("%s = {", label);
	for (i = 0; i < BDB_SECRET_SIZE; i++) {
		if (i % 8 == 0)
			printf("\n\t");
		else
			printf(" ");
		printf("0x%02x,", secret[i]);
	}
	printf("\n}\n");
}

static int read_bds(const char *filename, uint8_t *buf)
{
	char *line = NULL;
	size_t len = 0;
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "Failed to read %s\n", filename);
		return -1;
	}

	int i = 0;
	char *l;
	while (getline(&line, &len, fp) > 0) {
		l = line;
		while (i < BDB_SECRET_SIZE) {
			char *token = strtok(l, " ");
			if (!token)
				break;
			buf[i] = strtol(token, NULL, 16);
			l = NULL;
			i++;
		}
	}

	free(line);
	fclose(fp);

	if (i != BDB_SECRET_SIZE) {
		fprintf(stderr, "%s does not contain expected length of data\n",
			filename);
		return -1;
	}

	return 0;
}

static void test_secret_bdb(struct vba_context *ctx,
			    const uint8_t *wsr, const char *key_file)
{
	struct bdb_key *key;

	key = bdb_create_key(key_file, 0, "test bdb key");
	if (!key) {
		fprintf(stderr, "Failed to read BDB key from %s", key_file);
		gTestSuccess = 0;
		return;
	}

	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_BDB, (uint8_t *)wsr,
				       (const uint8_t *)key, key->struct_size),
		  __func__);
	if (memcmp(ctx->secrets->bdb, expected_bdb, BDB_SECRET_SIZE))
		/*
		 * Dumps secret on failure. This allows the test bin to work as
		 * a tool as well.
		 */
		dump_secret(ctx->secrets->bdb, "expected_bdb");
	else
		TEST_SUCC(0, "bdb matched");
}

static void test_secret_boot_path(struct vba_context *ctx,
				  const uint8_t *wsr, const char *key_file)
{
	struct bdb_key *key;

	key = bdb_create_key(key_file, 0, "test sub key");
	if (!key) {
		fprintf(stderr, "Failed to read sub key from %s", key_file);
		gTestSuccess = 0;
		return;
	}

	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_BDB, (uint8_t *)wsr,
				       (const uint8_t *)key, key->struct_size),
		  __func__);
	if (memcmp(ctx->secrets->bdb, expected_boot_path, BDB_SECRET_SIZE))
		dump_secret(ctx->secrets->bdb, "expected_boot_path");
	else
		TEST_SUCC(0, "boot_path matched");
}

static void test_secret_boot_verified(struct vba_context *ctx,
				      const uint8_t *wsr)
{
	int bdb_key_fused = ctx->flags & VBA_CONTEXT_FLAG_BDB_KEY_EFUSED;
	const uint8_t *expected;

	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_BOOT_VERIFIED,
				       (uint8_t *)wsr, NULL, 0),
		  __func__);
	if (bdb_key_fused)
		expected = expected_boot_verified_fv0;
	else
		expected = expected_boot_verified_fv1;

	if (memcmp(ctx->secrets->boot_verified, expected, BDB_SECRET_SIZE)) {
		printf("expected_boot_verified_");
		dump_secret(ctx->secrets->boot_verified,
			    bdb_key_fused ? "fv0" : "fv1");
	} else {
		TEST_SUCC(0, "boot_verified matched");
	}
}

static void test_secret_nvm_wp(struct vba_context *ctx, const uint8_t *wsr)
{
	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_NVM_WP,
				       (uint8_t *)wsr, NULL, 0),
		  __func__);
	if (memcmp(ctx->secrets->nvm_wp, expected_nvm_wp, BDB_SECRET_SIZE))
		dump_secret(ctx->secrets->nvm_wp, "expected_nvm_wp");
	else
		TEST_SUCC(0, "nvm_wp matched");
}

static void test_secret_nvm_rw(struct vba_context *ctx, const uint8_t *wsr)
{
	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_NVM_RW,
				       (uint8_t *)wsr, NULL, 0),
		  __func__);
	if (memcmp(ctx->secrets->nvm_rw, expected_nvm_rw, BDB_SECRET_SIZE))
		dump_secret(ctx->secrets->nvm_rw, "expected_nvm_rw");
	else
		TEST_SUCC(0, "nvm_rw matched");
}

static void test_secret_wsr(struct vba_context *ctx, uint8_t *wsr)
{
	/* Derive WSR. This has to be done last because it'll change WSR. */
	TEST_SUCC(vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_WSR, wsr, NULL, 0),
		  __func__);
	if (memcmp(wsr, expected_wsr, BDB_SECRET_SIZE))
		dump_secret(wsr, "expected_wsr");
	else
		TEST_SUCC(0, "wsr matched");
}

static void test_derive_secret_ro(const char *bds_file,
				  const char *bdbkey_file,
				  const char *subkey_file)
{
	struct vba_context *ctx;
	struct bdb_secrets *secrets ;
	uint8_t *wsr;

	ctx = calloc(1, sizeof(*ctx));
	secrets = calloc(1, sizeof(*secrets));
	wsr = calloc(1, BDB_SECRET_SIZE);
	if (!ctx || !secrets || !wsr) {
		fprintf(stderr, "Failed to allocate memory");
		gTestSuccess = 0;
		return;
	}

	ctx->secrets = secrets;
	if (read_bds(bds_file, wsr)) {
		fprintf(stderr, "Failed to read bds from %s", bds_file);
		gTestSuccess = 0;
		return;
	}

	test_secret_bdb(ctx, wsr, bdbkey_file);
	test_secret_boot_path(ctx, wsr, subkey_file);
	test_secret_boot_verified(ctx, wsr);
	ctx->flags |= VBA_CONTEXT_FLAG_BDB_KEY_EFUSED;
	test_secret_boot_verified(ctx, wsr);
	test_secret_nvm_wp(ctx, wsr);
	/* Deriving NVM-RW has to be done after NVM-WP */
	test_secret_nvm_rw(ctx, wsr);
	/* Extending WSR has to be done last. */
	test_secret_wsr(ctx, wsr);
}

int main(int argc, char *argv[])
{
	if (argc != 4) {
		fprintf(stderr, "Usage: bdb_secret <bds.txt> <bdbkey.keyb> "
				"<subkey.keyb>\n\n");
		fprintf(stderr, "Derive BDB secrets from the given BDS\n\n");
		fprintf(stderr,
			"<bds.txt> should contain a list of 8-bit integers "
			"represented in hex delimited by spaces or newlines. "
			"If derived secrets do not match expected values, "
			"correct values will be printed.\n"
			"<bdbkey.keyb> and <subkey.keyb> should contain a BDB "
			"key and a sub key in keyb format, respectively.\n");
		return -1;
	}

	test_derive_secret_ro(argv[1], argv[2], argv[3]);

	return gTestSuccess ? 0 : 255;
}
