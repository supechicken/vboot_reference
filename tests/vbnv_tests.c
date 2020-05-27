/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "2api.h"
#include "2common.h"
#include "2constants.h"
#include "2nvstorage.h"
#include "2return_codes.h"
#include "crossystem_vbnv.h"
#include "flashrom.h"
#include "test_common.h"

/* Mocked flashrom only supports host programmer, and RW_NVRAM
   region. */
static void assert_mock_params(const char *programmer, const char *region)
{
	TEST_STR_EQ(programmer, FLASHROM_PROGRAMMER_INTERNAL_AP,
		    "Using internal AP programmer");
	TEST_STR_EQ(region, "RW_NVRAM", "Using NVRAM region");
}

#define FAKE_FLASH_CHIP_ENTRIES 32
static uint8_t fake_flash_region[VB2_NVDATA_SIZE * FAKE_FLASH_CHIP_ENTRIES];

static void clear_flash(void)
{
	memset(fake_flash_region, 0xff, sizeof(fake_flash_region));
}

/* Mocked flashrom_read for tests. */
vb2_error_t flashrom_read(const char *programmer, const char *region,
			  uint8_t **data_out, uint32_t *size_out)
{
	assert_mock_params(programmer, region);

	*data_out = malloc(sizeof(fake_flash_region));
	*size_out = sizeof(fake_flash_region);
	memcpy(*data_out, fake_flash_region, sizeof(fake_flash_region));
	return VB2_SUCCESS;
}

/* Mocked flashrom_write for tests. */
vb2_error_t flashrom_write(const char *programmer, const char *region,
			   uint8_t *data, uint32_t data_size)
{
	assert_mock_params(programmer, region);

	TEST_EQ(data_size, sizeof(fake_flash_region),
		"The flash size is correct");
	memcpy(fake_flash_region, data, data_size);
	return VB2_SUCCESS;
}

static const uint8_t test_nvdata[] = {
	0x60, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x4e,
	0x00, 0xfe, 0xff, 0x00, 0x00, 0xff, 0xff, 0x5e,
};

static const uint8_t test_nvdata2[] = {
	0x60, 0x10, 0x00, 0x00, 0x00, 0x02, 0x00, 0x4c,
	0x00, 0xfe, 0xff, 0x00, 0x00, 0xff, 0xff, 0x78,
};

static void init_test_vbctx(struct vb2_context *ctx)
{
	ctx->flags = 0;
	memcpy(ctx->nvdata, test_nvdata, sizeof(test_nvdata));
}

static void test_read_ok_beginning(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();
	memcpy(fake_flash_region, test_nvdata2, sizeof(test_nvdata2));

	TEST_EQ(vb2_read_nv_storage_flashrom(&ctx), 0,
		"Reading storage succeeds");
	TEST_TRUE(!memcmp(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2)),
		  "The nvdata in the vb2_context was updated from flash");
}

static void test_read_ok_2ndentry(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();
	memcpy(fake_flash_region, test_nvdata, sizeof(test_nvdata));
	memcpy(fake_flash_region + VB2_NVDATA_SIZE, test_nvdata2,
	       sizeof(test_nvdata2));

	TEST_EQ(vb2_read_nv_storage_flashrom(&ctx), 0,
		"Reading storage succeeds");
	TEST_TRUE(!memcmp(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2)),
		  "The nvdata in the vb2_context was updated from flash");
}

static void test_read_ok_full(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();

	for (int entry = 0; entry < FAKE_FLASH_CHIP_ENTRIES - 2; entry++)
		memcpy(fake_flash_region + (entry * VB2_NVDATA_SIZE),
		       test_nvdata, sizeof(test_nvdata));

	memcpy(fake_flash_region +
	       ((FAKE_FLASH_CHIP_ENTRIES - 2) * VB2_NVDATA_SIZE),
	       test_nvdata2, sizeof(test_nvdata2));

	TEST_EQ(vb2_read_nv_storage_flashrom(&ctx), 0,
		"Reading storage succeeds");
	TEST_TRUE(!memcmp(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2)),
		  "The nvdata in the vb2_context was updated from flash");
}

static void test_read_fail_uninitialized(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();

	TEST_NEQ(vb2_read_nv_storage_flashrom(&ctx), 0,
		 "Reading storage fails when flash is erased");
}

static void test_write_ok_beginning(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();
	memcpy(fake_flash_region, test_nvdata, sizeof(test_nvdata));
	memcpy(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2));

	TEST_EQ(vb2_write_nv_storage_flashrom(&ctx), 0,
		"Writing storage succeeds");
	TEST_TRUE(!memcmp(fake_flash_region + VB2_NVDATA_SIZE,
			  test_nvdata2, sizeof(test_nvdata2)),
		  "The flash was updated with a new entry");
}

static void test_write_ok_2ndentry(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();
	memcpy(fake_flash_region, test_nvdata, sizeof(test_nvdata));
	memcpy(fake_flash_region + VB2_NVDATA_SIZE,
	       test_nvdata, sizeof(test_nvdata));
	memcpy(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2));

	TEST_EQ(vb2_write_nv_storage_flashrom(&ctx), 0,
		"Writing storage succeeds");
	TEST_TRUE(!memcmp(fake_flash_region + (2 * VB2_NVDATA_SIZE),
			  test_nvdata2, sizeof(test_nvdata2)),
		  "The flash was updated with a new entry");
}

static void test_write_ok_full(void)
{
	struct vb2_context ctx;
	uint8_t expected_flash[VB2_NVDATA_SIZE * FAKE_FLASH_CHIP_ENTRIES];

	init_test_vbctx(&ctx);
	clear_flash();

	for (int entry = 0; entry < FAKE_FLASH_CHIP_ENTRIES - 1; entry++)
		memcpy(fake_flash_region + (entry * VB2_NVDATA_SIZE),
		       test_nvdata, sizeof(test_nvdata));

	memcpy(expected_flash, test_nvdata2, sizeof(test_nvdata2));
	memset(expected_flash + VB2_NVDATA_SIZE, 0xff,
	       sizeof(expected_flash) - VB2_NVDATA_SIZE);
	memcpy(ctx.nvdata, test_nvdata2, sizeof(test_nvdata2));

	TEST_EQ(vb2_write_nv_storage_flashrom(&ctx), 0,
		"Writing storage succeeds");
	TEST_TRUE(!memcmp(fake_flash_region, expected_flash,
			  sizeof(expected_flash)),
		  "The flash was erased and the new entry was placed at "
		  "the beginning");
}

static void test_write_fail_uninitialized(void)
{
	struct vb2_context ctx;

	init_test_vbctx(&ctx);
	clear_flash();

	TEST_NEQ(vb2_write_nv_storage_flashrom(&ctx), 0,
		 "Writing storage fails when the flash is erased");
}

int main(int argc, char *argv[])
{
	test_read_ok_beginning();
	test_read_ok_2ndentry();
	test_read_ok_full();
	test_read_fail_uninitialized();
	test_write_ok_beginning();
	test_write_ok_2ndentry();
	test_write_ok_full();
	test_write_fail_uninitialized();

	return gTestSuccess ? 0 : 255;
}
