/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware management parameters (FWMP) library.
 */

#include "2common.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"
#include "test_common.h"

static uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
	__attribute__ ((aligned (VB2_WORKBUF_ALIGN)));
static struct vb2_context ctx;
static struct vb2_gbb_header gbb;
static struct vb2_shared_data *sd;
static struct vb2_secdata_fwmp *sec;

static void reset_common_data(void)
{
	memset(workbuf, 0xaa, sizeof(workbuf));

	memset(&ctx, 0, sizeof(ctx));
	ctx.workbuf = workbuf;
	ctx.workbuf_size = sizeof(workbuf);
	ctx.flags = 0;

	vb2_init_context(&ctx);
	sd = vb2_get_sd(&ctx);
	sd->status = VB2_SD_STATUS_SECDATA_FWMP_INIT;

	memset(&gbb, 0, sizeof(gbb));

	sec = (struct vb2_secdata_fwmp *)ctx.secdata_fwmp;
	sec->struct_size = VB2_SECDATA_FWMP_MIN_SIZE;
	sec->struct_version = VB2_SECDATA_FWMP_VERSION;
	sec->flags = 0;
}

/* Mocked functions */

struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

static void check_init_test(void)
{
	uint8_t size = 0;

	/* Check size constants */
	TEST_TRUE(sizeof(struct vb2_secdata_fwmp) >= VB2_SECDATA_FWMP_MIN_SIZE,
		  "Struct min size constant");
	TEST_TRUE(sizeof(struct vb2_secdata_fwmp) <= VB2_SECDATA_FWMP_MAX_SIZE,
		  "Struct max size constant");

	/* Size too large */
	reset_common_data();
	sec->struct_size = VB2_SECDATA_FWMP_MAX_SIZE + 1;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Check large size");
	TEST_EQ(vb2_secdata_fwmp_init(&ctx),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Init large size");

	/* Size too small */
	reset_common_data();
	sec->struct_size = VB2_SECDATA_FWMP_MIN_SIZE - 1;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Check small size");
	TEST_EQ(vb2_secdata_fwmp_init(&ctx),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Init small size");

	/* Still need to read more data */
	reset_common_data();
	size = 0;
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_INCOMPLETE, "Check need more data");

	/* Blank data is invalid */
	reset_common_data();
	memset(ctx.secdata_fwmp, 0xa6, sizeof(ctx.secdata_fwmp));
	sec->struct_size = VB2_SECDATA_FWMP_MIN_SIZE;
	size = sec->struct_size;
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_CRC, "Check blank CRC");
	TEST_EQ(vb2_secdata_fwmp_init(&ctx),
		VB2_ERROR_SECDATA_FWMP_CRC, "Init blank CRC");

	/* Major version too high */
	reset_common_data();
	sec->struct_version = ((VB2_SECDATA_FWMP_VERSION >> 4) + 1) << 4;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_VERSION, "Check major too high");
	TEST_EQ(vb2_secdata_fwmp_init(&ctx),
		VB2_ERROR_SECDATA_FWMP_VERSION, "Init major too high");

	/* Major version too low */
	reset_common_data();
	sec->struct_version = ((VB2_SECDATA_FWMP_VERSION >> 4) - 1) << 4;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_EQ(vb2api_secdata_fwmp_check(&ctx, &size),
		VB2_ERROR_SECDATA_FWMP_VERSION, "Check major too low");
	TEST_EQ(vb2_secdata_fwmp_init(&ctx),
		VB2_ERROR_SECDATA_FWMP_VERSION, "Init major too low");

	/* Minor version difference okay */
	reset_common_data();
	sec->struct_version += 1;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_SUCC(vb2api_secdata_fwmp_check(&ctx, &size), "Check minor okay");
	TEST_SUCC(vb2_secdata_fwmp_init(&ctx), "Init minor okay");

	/* Good FWMP data */
	reset_common_data();
	sec->struct_version = VB2_SECDATA_FWMP_VERSION;
	sec->crc8 = vb2_secdata_fwmp_crc(&ctx);
	TEST_SUCC(vb2api_secdata_fwmp_check(&ctx, &size), "Check good");
	TEST_SUCC(vb2_secdata_fwmp_init(&ctx), "Init good");
	TEST_NEQ(sd->status & VB2_SD_STATUS_SECDATA_FWMP_INIT, 0,
		 "Init flag set");
}

static void get_flag_test(void)
{
	/* Successfully returns value */
	reset_common_data();
	sec->flags |= 1;
	TEST_EQ(vb2_secdata_fwmp_get_flag(&ctx, 1), 1,
		"Successfully returns flag value");

	/* CONTEXT_NO_SECDATA_FWMP */
	reset_common_data();
	sec->flags |= 1;
	ctx.flags |= VB2_CONTEXT_NO_SECDATA_FWMP;
	TEST_EQ(vb2_secdata_fwmp_get_flag(&ctx, 1), 0,
		"NO_SECDATA_FWMP forces default flag value");

	/* GBB_FLAG_DISABLE_FWMP */
	reset_common_data();
	sec->flags |= 1;
	gbb.flags |= VB2_GBB_FLAG_DISABLE_FWMP;
	TEST_EQ(vb2_secdata_fwmp_get_flag(&ctx, 1), 0,
		"GBB_FLAG_DISABLE_FWMP forces default flag value");

	/* FWMP hasn't been initialized (recovery mode) */
	reset_common_data();
	sd->status &= ~VB2_SD_STATUS_SECDATA_FWMP_INIT;
	ctx.flags |= VB2_CONTEXT_RECOVERY_MODE;
	TEST_EQ(vb2_secdata_fwmp_get_flag(&ctx, 0), 0,
		"non-init in recovery mode forces default flag value");

	/* FWMP hasn't been initialized (normal mode) */
	reset_common_data();
	sd->status &= ~VB2_SD_STATUS_SECDATA_FWMP_INIT;
	TEST_ABORT(vb2_secdata_fwmp_get_flag(&ctx, 0),
		   "non-init in normal mode triggers abort");
}

static void get_dev_key_hash_test(void)
{
	reset_common_data();
	TEST_TRUE(vb2_secdata_fwmp_get_dev_key_hash(&ctx) ==
		  sec->dev_key_hash, "proper dev_key_hash pointer returned");
}

int main(int argc, char* argv[])
{
	check_init_test();
	get_flag_test();
	get_dev_key_hash_test();

	return gTestSuccess ? 0 : 255;
}
