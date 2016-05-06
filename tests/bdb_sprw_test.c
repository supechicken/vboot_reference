/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Unit tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "2sha.h"
#include "2hmac.h"
#include "bdb.h"
#include "bdb_api.h"
#include "bdb_struct.h"
#include "host.h"
#include "test_common.h"
#include "vboot_register.h"

static struct bdb_header *bdb, *bdb0, *bdb1;
static uint32_t vboot_register;
static uint32_t vboot_register_persist;
static char slot_selected;
static uint8_t aprw_digest[BDB_SHA256_DIGEST_SIZE];
static uint8_t reset_count;

static uint8_t nvmrw1[sizeof(struct nvmrw)];
static uint8_t nvmrw2[sizeof(struct nvmrw)];

static struct bdb_header *create_bdb(const char *key_dir,
				     struct bdb_hash *hash, int num_hashes)
{
	struct bdb_header *b;
	uint8_t oem_area_0[32] = "Some OEM area.";
	uint8_t oem_area_1[64] = "Some other OEM area.";
	char filename[1024];

	struct bdb_create_params p = {
		.bdb_load_address = 0x11223344,
		.oem_area_0 = oem_area_0,
		.oem_area_0_size = sizeof(oem_area_0),
		.oem_area_1 = oem_area_1,
		.oem_area_1_size = sizeof(oem_area_1),
		.header_sig_description = "The header sig",
		.data_sig_description = "The data sig",
		.data_description = "Test BDB data",
		.data_version = 3,
		.hash = hash,
		.num_hashes = num_hashes,
	};

	uint8_t bdbkey_digest[BDB_SHA256_DIGEST_SIZE];

	/* Load keys */
	sprintf(filename, "%s/bdbkey.keyb", key_dir);
	p.bdbkey = bdb_create_key(filename, 100, "BDB key");
	sprintf(filename, "%s/datakey.keyb", key_dir);
	p.datakey = bdb_create_key(filename, 200, "datakey");
	sprintf(filename, "%s/bdbkey.pem", key_dir);
	p.private_bdbkey = read_pem(filename);
	sprintf(filename, "%s/datakey.pem", key_dir);
	p.private_datakey = read_pem(filename);
	if (!p.bdbkey || !p.datakey || !p.private_bdbkey || !p.private_datakey) {
		fprintf(stderr, "Unable to load test keys\n");
		exit(2);
	}

	vb2_digest_buffer((uint8_t *)p.bdbkey, p.bdbkey->struct_size,
			  VB2_HASH_SHA256,
			  bdbkey_digest, BDB_SHA256_DIGEST_SIZE);

	b = bdb_create(&p);
	if (!b) {
		fprintf(stderr, "Unable to create test BDB\n");
		exit(2);
	}

	/* Free keys and buffers */
	free(p.bdbkey);
	free(p.datakey);
	RSA_free(p.private_bdbkey);
	RSA_free(p.private_datakey);

	return b;
}

static void calculate_aprw_digest(const struct bdb_hash *hash, uint8_t *digest)
{
	/* Locate AP-RW */
	/* Calculate digest as loading AP-RW */
	memcpy(digest, aprw_digest, sizeof(aprw_digest));
}

static void verstage_main(void)
{
	struct vba_context ctx;
	const struct bdb_hash *hash;
	uint8_t digest[BDB_SHA256_DIGEST_SIZE];
	int rv;

	rv = vba_bdb_init(&ctx);
	if (rv) {
		fprintf(stderr, "Initializing context failed for (%d)\n", rv);
		vba_bdb_fail(&ctx);
		/* This return is needed for unit test. vba_bdb_fail calls
		 * vbe_reset, which calls verstage_main. If verstage_main
		 * successfully returns, we return here as well. */
		return;
	}
	fprintf(stderr, "Initialized context. Trying slot %c\n",
		ctx.slot ? 'B' : 'A');

	/* 1. Locate BDB */

	/* 2. Get bdb_hash structure for AP-RW */
	hash = bdb_get_hash(bdb, BDB_DATA_AP_RW);
	fprintf(stderr, "Got hash of AP-RW\n");

	/* 3. Load & calculate digest of AP-RW */
	calculate_aprw_digest(hash, digest);
	fprintf(stderr, "Calculated digest\n");

	/* 4. Compare digests */
	if (memcmp(hash->digest, digest, BDB_SHA256_DIGEST_SIZE)) {
		fprintf(stderr, "Digests do not match\n");
		vba_bdb_fail(&ctx);
		/* This return is needed for unit test. vba_bdb_fail calls
		 * vbe_reset, which calls verstage_main. If verstage_main
		 * successfully returns, we return here as well. */
		return;
	}

	/* 5. Record selected slot. This depends on the firmware */
	slot_selected = ctx.slot ? 'B' : 'A';
	fprintf(stderr, "Selected AP-RW in slot %c\n", slot_selected);

	/* X. This should be done upon AP-RW's request after everything is
	 * successful. We do it here for the unit test. */
	vba_bdb_finalize(&ctx);
}

uint32_t vbe_get_vboot_register(enum vboot_register type)
{
	switch (type) {
	case VBOOT_REGISTER:
		return vboot_register;
	case VBOOT_REGISTER_PERSIST:
		return vboot_register_persist;
	default:
		fprintf(stderr, "Invalid vboot register type (%d)\n", type);
		exit(2);
	}
}

void vbe_set_vboot_register(enum vboot_register type, uint32_t val)
{
	switch (type) {
	case VBOOT_REGISTER:
		vboot_register = val;
		break;
	case VBOOT_REGISTER_PERSIST:
		vboot_register_persist = val;
		break;
	default:
		fprintf(stderr, "Invalid vboot register type (%d)\n", type);
		exit(2);
	}
}

void vbe_reset(void)
{
	uint32_t val = vbe_get_vboot_register(VBOOT_REGISTER_PERSIST);

	fprintf(stderr, "Booting ...\n");

	if (++reset_count > 5) {
		fprintf(stderr, "Reset counter exceeded maximum value\n");
		exit(2);
	}

	/* Emulate warm reset */
	vboot_register = 0;
	if (val & VBOOT_REGISTER_RECOVERY_REQUEST) {
		fprintf(stderr, "Recovery requested\n");
		return;
	}
	/* Selected by SP-RO */
	bdb = (val & VBOOT_REGISTER_TRY_SECONDARY_BDB) ? bdb1 : bdb0;
	verstage_main();
}

static void test_verify_aprw(const char *key_dir)
{
	struct bdb_hash hash0 = {
		.offset = 0x28000,
		.size = 0x20000,
		.partition = 1,
		.type = BDB_DATA_AP_RW,
		.load_address = 0x200000,
		.digest = {0x11, 0x11, 0x11, 0x11},
	};
	struct bdb_hash hash1 = {
		.offset = 0x28000,
		.size = 0x20000,
		.partition = 1,
		.type = BDB_DATA_AP_RW,
		.load_address = 0x200000,
		.digest = {0x22, 0x22, 0x22, 0x22},
	};

	bdb0 = create_bdb(key_dir, &hash0, 1);
	bdb1 = create_bdb(key_dir, &hash1, 1);
	memset(aprw_digest, 0, BDB_SHA256_DIGEST_SIZE);

	/* (slotA, slotB) = (good, bad) */
	reset_count = 0;
	vboot_register_persist = 0;
	slot_selected = 'X';
	memcpy(aprw_digest, hash0.digest, 4);
	vbe_reset();
	TEST_EQ(reset_count, 1, NULL);
	TEST_EQ(slot_selected, 'A', NULL);
	TEST_FALSE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_PRIMARY,
		   NULL);
	TEST_FALSE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_SECONDARY,
		   NULL);

	/* (slotA, slotB) = (bad, good) */
	reset_count = 0;
	vboot_register_persist = 0;
	slot_selected = 'X';
	memcpy(aprw_digest, hash1.digest, 4);
	vbe_reset();
	TEST_EQ(reset_count, 3, NULL);
	TEST_EQ(slot_selected, 'B', NULL);
	TEST_TRUE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_PRIMARY,
		  NULL);
	TEST_FALSE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_SECONDARY,
		   NULL);

	/* (slotA, slotB) = (bad, bad) */
	reset_count = 0;
	vboot_register_persist = 0;
	slot_selected = 'X';
	memset(aprw_digest, 0, BDB_SHA256_DIGEST_SIZE);
	vbe_reset();
	TEST_EQ(reset_count, 5, NULL);
	TEST_EQ(slot_selected, 'X', NULL);
	TEST_TRUE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_PRIMARY,
		  NULL);
	TEST_TRUE(vboot_register_persist & VBOOT_REGISTER_FAILED_RW_SECONDARY,
		  NULL);
	TEST_TRUE(vboot_register_persist & VBOOT_REGISTER_RECOVERY_REQUEST,
		  NULL);

	/* Clean up */
	free(bdb0);
	free(bdb1);
}

int vbe_read_nvm(enum nvm_type type, uint8_t **buf, uint32_t *size)
{
	/* Read NVM-RW contents (from EEPROM for example) */
	switch (type) {
	case NVM_TYPE_RW_PRIMARY:
		*buf = nvmrw1;
		*size = sizeof(nvmrw1);
		break;
	case NVM_TYPE_RW_SECONDARY:
		*buf = nvmrw2;
		*size = sizeof(nvmrw2);
		break;
	default:
		return -1;
	}
	return BDB_SUCCESS;
}

int vbe_write_nvm(enum nvm_type type, void *buf, uint32_t size)
{
	/* Write NVM-RW contents (to EEPROM for example) */
	switch (type) {
	case NVM_TYPE_RW_PRIMARY:
		memcpy(nvmrw1, buf, size);
		break;
	case NVM_TYPE_RW_SECONDARY:
		memcpy(nvmrw2, buf, size);
		break;
	default:
		return -1;
	}
	return BDB_SUCCESS;
}

static void test_update_kernel_version(void)
{
	struct bdb_secret secret = {
		.nvm_rw_secret = {0x00, },
	};
	struct vba_context ctx = {
		.slot = 0,
		.bdb = NULL,
		.secrets = &secret,
		.nvmrw = NULL,
	};
	struct nvmrw nvm1 = {
		.struct_size = sizeof(struct nvmrw),
		.min_kernel_data_key_version = 0,
		.min_kernel_version = 0,
		.update_count = 0,
	};
	struct nvmrw nvm2 = {
		.struct_size = sizeof(struct nvmrw),
		.min_kernel_data_key_version = 0,
		.min_kernel_version = 0,
		.update_count = 0,
	};
	struct nvmrw *nvm;

	/* Compute HMAC */
	hmac(VB2_HASH_SHA256, secret.nvm_rw_secret, BDB_SECRET_SIZE,
	     &nvm1, nvm1.struct_size - sizeof(nvm1.hmac),
	     nvm1.hmac, sizeof(nvm1.hmac));
	hmac(VB2_HASH_SHA256, secret.nvm_rw_secret, BDB_SECRET_SIZE,
	     &nvm2, nvm2.struct_size - sizeof(nvm2.hmac),
	     nvm2.hmac, sizeof(nvm2.hmac));

	/* Install NVM-RWs (in EEPROM for example) */
	memcpy(nvmrw1, &nvm1, sizeof(nvm1));
	memcpy(nvmrw2, &nvm2, sizeof(nvm2));

	TEST_SUCC(vba_update_kernel_version(&ctx, 1, 1), NULL);

	nvm = (struct nvmrw *)nvmrw1;
	TEST_EQ(nvm->min_kernel_data_key_version, 1, NULL);
	TEST_EQ(nvm->min_kernel_version, 1, NULL);
//	TEST_EQ(nvm->update_count, 1, NULL);

	nvm = (struct nvmrw *)nvmrw2;
	TEST_EQ(nvm->min_kernel_data_key_version, 1, NULL);
	TEST_EQ(nvm->min_kernel_version, 1, NULL);
//	TEST_EQ(nvm->update_count, 1, NULL);

	/* Test secondary copy is synced with primary */
	/* Test primary copy is synced with secondary */
	/* Test write failure */
}

/*****************************************************************************/

int main(int argc, char *argv[])
{
	if (argc != 2) {
		fprintf(stderr, "Usage: %s <keys_dir>", argv[0]);
		return -1;
	}
	printf("Running BDB SP-RW tests...\n");

	test_verify_aprw(argv[1]);
	test_update_kernel_version();

	return gTestSuccess ? 0 : 255;
}
