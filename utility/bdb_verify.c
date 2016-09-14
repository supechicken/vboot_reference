/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "2sha.h"
#include "bdb.h"
#include "bdb_api.h"
#include "host.h"

static f_extend extend = vb2_sha256_extend;

static void help(void)
{
	fprintf(stderr,
		"Usage: bdb_verify [-d bdb_key_digest_file] "
		"[-s bds_file] [-m] <bdb_file>\n\n"
		"  Verify a BDB with a given key digest and output secrets "
		"derived from a given BDS. When '-m' is given, a different "
		"sha256_extend algorithm will be used for secret creation.\n");
}

static int read_bds(const char *filename, uint8_t *buf)
{
	char *line = NULL;
	size_t len = 0;
	FILE *fp;

	fp = fopen(filename, "r");
	if (!fp) {
		fprintf(stderr, "ERROR: Failed to open %s\n", filename);
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
		fprintf(stderr,
			"ERROR: %s does not contain expected length of data\n",
			filename);
		return -1;
	}

	return 0;
}

/*
 * xxx's implementation of sha256 extend
 */
static void sha256_extendish(const uint8_t *from, const uint8_t *by,
			     uint8_t *to)
{
	struct vb2_sha256_context dc;

	vb2_sha256_init(&dc);
	memcpy((uint8_t *)dc.h, from, VB2_SHA256_DIGEST_SIZE);
	vb2_sha256_update(&dc, by, VB2_SHA256_BLOCK_SIZE);
	vb2_sha256_finalize(&dc, to);
}

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

static void dump_secrets(struct vba_context *ctx, const uint8_t *wsr)
{
	dump_secret(ctx->secrets->bdb, "bdb");
	dump_secret(ctx->secrets->boot_path, "boot_path");
	dump_secret(ctx->secrets->boot_verified, "boot_verified");
	dump_secret(ctx->secrets->nvm_wp, "nvm_wp");
	dump_secret(ctx->secrets->nvm_rw, "nvm_rw");
	dump_secret(wsr, "wsr");
}

static int derive_secrets(struct vba_context *ctx,
			  const uint8_t *bdb, const char *bds_file)
{
	uint8_t wsr[BDB_SECRET_SIZE];
	struct bdb_secrets secrets;

	memset(wsr, 0, sizeof(wsr));
	if (read_bds(bds_file, wsr))
		return -1;

	memset(&secrets, 0, sizeof(secrets));
	ctx->secrets = &secrets;
	if (vba_extend_secrets_ro(ctx, bdb, wsr, extend)) {
		fprintf(stderr, "ERROR: Failed to derive secrets\n");
		return -1;
	}

	fprintf(stderr, "LOG: Secrets are derived as follows\n");
	dump_secrets(ctx, wsr);

	return 0;
}

int main(int argc, char *argv[])
{
	struct vba_context ctx;
	uint8_t *bdb;
	uint8_t *bdb_key_digest = NULL;
	uint32_t bdb_size;
	uint32_t digest_size;
	const char *bdb_file = NULL;
	const char *bdb_key_digest_file = NULL;
	const char *bds_file = NULL;
	int rv;
	int opt;

	while ((opt = getopt(argc, argv, "d:hms:")) != -1) {
		switch(opt) {
		case 'd':
			bdb_key_digest_file = optarg;
			break;
		case 'h':
			help();
			return 0;
		case 'm':
			extend = sha256_extendish;
			break;
		case 's':
			bds_file = optarg;
			break;
		default:
			help();
			return -1;
		}
	}
	bdb_file = argv[optind];

	if (!bdb_file) {
		fprintf(stderr, "ERROR: BDB file has to be specified\n\n");
		help();
		return -1;
	}

	bdb = read_file(bdb_file, &bdb_size);
	if (!bdb) {
		fprintf(stderr, "ERROR: Unable to read %s\n", bdb_file);
		return -1;
	}

	if (bdb_key_digest_file) {
		bdb_key_digest = read_file(bdb_key_digest_file, &digest_size);
		if (!bdb_key_digest) {
			fprintf(stderr, "ERROR: Unable to read %s\n",
				bdb_key_digest_file);
			return -1;
		}
	}

	memset(&ctx, 0, sizeof(ctx));
	rv = bdb_verify(bdb, bdb_size, bdb_key_digest);
	if (rv == BDB_SUCCESS) {
		ctx.flags |= VBA_CONTEXT_FLAG_BDB_KEY_EFUSED;
		fprintf(stderr, "LOG: BDB is verified by eFused key\n");
	} else {
		if (rv != BDB_GOOD_OTHER_THAN_KEY) {
			fprintf(stderr, "ERROR: Failed to verify BDB for error "
				"0x%08x\n", rv);
			return -1;
		}
		fprintf(stderr, "LOG: BDB is verified by unidentified key\n");
	}

	if (bds_file)
		derive_secrets(&ctx, bdb, bds_file);

	return 0;
}
