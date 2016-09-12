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
#include "secrets.h"
#include "test_common.h"

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

static void test_derive_secret_ro(const char *bds_file)
{
	struct vba_context *ctx;
	struct bdb_secrets *secrets ;
	uint8_t *wsr;
	uint8_t expected_boot_verified_fv0[] = {
		0x5c, 0x86, 0x20, 0x34, 0x6f, 0xd9, 0x55, 0x49,
		0xe4, 0xcd, 0x95, 0x04, 0x52, 0x27, 0x6b, 0x31,
		0xb6, 0xb0, 0x57, 0x08, 0xde, 0xcb, 0x5f, 0x84,
		0x8a, 0x0f, 0xf3, 0x6c, 0x11, 0x5c, 0x22, 0xf6,
	};
	uint8_t expected_boot_verified_fv1[] = {
		0x7b, 0x69, 0xef, 0x0a, 0x7c, 0x4f, 0xe6, 0x0b,
		0xfe, 0x35, 0xd4, 0xca, 0xd3, 0x0d, 0xb8, 0x3f,
		0xb0, 0x4b, 0x51, 0x18, 0xf2, 0x67, 0xa1, 0xb6,
		0x39, 0xb1, 0x1b, 0x97, 0x03, 0xcd, 0x65, 0x5d,
	};
	uint8_t expected_wsr[] = {
		0x6d, 0x69, 0x53, 0xd9, 0x5e, 0x9b, 0x4e, 0x6b,
		0xe0, 0xee, 0x75, 0x93, 0x14, 0x56, 0xb0, 0x0c,
		0xf9, 0xca, 0x38, 0xa2, 0xd1, 0x77, 0xd2, 0xf2,
		0xbb, 0x08, 0x69, 0x0a, 0xe6, 0xff, 0x07, 0xa7,
	};

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

	vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_BOOT_VERIFIED, wsr, NULL, 0);
	if (memcmp(ctx->secrets->boot_verified, expected_boot_verified_fv1,
		   BDB_SECRET_SIZE))
		/*
		 * Dumps secret on failure. This allows the test bin to work as
		 * a tool as well.
		 */
		dump_secret(ctx->secrets->boot_verified,
			    "expected_boot_verified_fv1");
	else
		TEST_SUCC(0, "boot_verified_fv1 matched");

	ctx->flags |= VBA_CONTEXT_FLAG_BDB_KEY_EFUSED;
	vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_BOOT_VERIFIED, wsr, NULL, 0);
	if (memcmp(ctx->secrets->boot_verified, expected_boot_verified_fv0,
		   BDB_SECRET_SIZE))
		dump_secret(ctx->secrets->boot_verified,
			    "expected_boot_verified_fv0");
	else
		TEST_SUCC(0, "boot_verified_fv0 matched");

	/* Derive WSR. This has to be done last because it'll change WSR. */
	vba_derive_secret_ro(ctx, BDB_SECRET_TYPE_WSR, wsr, NULL, 0);
	if (memcmp(wsr, expected_wsr, BDB_SECRET_SIZE))
		dump_secret(wsr, "expected_wsr");
	else
		TEST_SUCC(0, "wsr matched");
}

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: bdb_secret <bds.txt>\n");
		fprintf(stderr, "Derive BDB secrets from the given BDS\n");
		fprintf(stderr, "<bds.txt> should contain a list of 8-bit "
				"integers represented in hex delimited by "
				"spaces.\n");
		return -1;
	}

	test_derive_secret_ro(argv[1]);

	return gTestSuccess ? 0 : 255;
}
