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

#include "bdb.h"
#include "bdb_api.h"
#include "bdb_struct.h"
#include "secrets.h"

/*
 * Input files each contain one BDB secret represented in text. Each line
 * consists of two bytes representing 8 bit integer ('00' ~ 'FF') followed
 * by 0x0d 0x0a.
 */
#define EXPECTED_LINE_LEN	4

static struct bdb_ro_secrets ro_secrets = {
	.nvm_wp = {0x00, },
	.nvm_rw = {0x00, },
	.bdb = {0x00, },
	.boot_verified = {0x00, },
	.boot_path = {0x00, },
};

struct bdb_rw_secrets rw_secrets = {
	.buc = {0x00, },
};

static void dump_secret(const uint8_t *secret, const char *label)
{
	int i;
	printf("%s", label);
	for (i = 0; i < BDB_SECRET_SIZE; i++)
		printf(" %02x", secret[i]);
	printf("\n");
}

static int read_bds(const char *filename, uint8_t *buf)
{
	char *line = NULL;
	size_t len = 0;
	FILE *fp;
	int i;

	fp = fopen(filename, "r");
	if (!fp) {
		printf("Failed to read %s\n", filename);
		return -1;
	}

	for (i = 0; i < BDB_SECRET_SIZE; i++) {
		int l = getline(&line, &len, fp);
		if (l < 0 || EXPECTED_LINE_LEN < l) {
			printf("Failed to read line %d or length (%d) does not "
			       " match expectation\n", i, l);
			free(line);
			fclose(fp);
			return -1;
		}
		buf[i] = strtol(line, NULL, 16);
	}
	free(line);
	fclose(fp);
	if (i != BDB_SECRET_SIZE) {
		printf("%s does not contain expected byte of data\n", filename);
		return -1;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	struct vba_context ctx = {
		.bdb = NULL,
		.ro_secrets = &ro_secrets,
		.rw_secrets = &rw_secrets,
	};
	uint8_t *wsr;

	if (argc != 2) {
		printf("Usage: bdb_secret <DUI.txt>\n");
		printf("Calculate BDB secrets derived from the given BDS\n");
		return -1;
	}

	wsr = calloc(1, BDB_SECRET_SIZE);
	if (!wsr) {
		printf("Unable to allocate buffer\n");
		return -1;
	}
	if (read_bds(argv[1], wsr))
		return -1;
	dump_secret(wsr, "bds:");

	vba_derive_secret_ro(&ctx, BDB_SECRET_TYPE_BOOT_VERIFIED, wsr, NULL, 0);
	dump_secret(ctx.ro_secrets->boot_verified, "boot_verified (FV1):");

	ctx.flags |= VBA_CONTEXT_FLAG_BDB_KEY_EFUSED;
	vba_derive_secret_ro(&ctx, BDB_SECRET_TYPE_BOOT_VERIFIED, wsr, NULL, 0);
	dump_secret(ctx.ro_secrets->boot_verified, "boot_verified (FV0):");

	/* Derive WSR. This has to be done last because it'll change WSR. */
	vba_derive_secret_ro(&ctx, BDB_SECRET_TYPE_WSR, wsr, NULL, 0);
	dump_secret(wsr, "wsr:");

	return 0;
}
