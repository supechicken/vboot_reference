/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for kernel secure storage library.
 */

#include "2api.h"
#include "2common.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"
#include "2sysincludes.h"
#include "test_common.h"

static uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_secdata_kernel_v02 *sec02;
static struct vb2_secdata_kernel_v10 *sec10;

static void reset_common_data(void)
{
	memset(workbuf, 0xaa, sizeof(workbuf));
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	sd = vb2_get_sd(ctx);

	sec02 = (struct vb2_secdata_kernel_v02 *)ctx->secdata_kernel;
	sec10 = (struct vb2_secdata_kernel_v10 *)ctx->secdata_kernel;
}

static void test_changed(struct vb2_context *c, int changed, const char *why)
{
	if (changed)
		TEST_NEQ(c->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED, 0, why);
	else
		TEST_EQ(c->flags & VB2_CONTEXT_SECDATA_KERNEL_CHANGED, 0, why);

	c->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
};

static void secdata_kernel_create_v02(void)
{
	memset(sec02, 0, sizeof(*sec02));
	sec02->struct_version = VB2_SECDATA_KERNEL_VERSION_V02;
	sec02->crc8 = vb2_secdata_kernel_calc_crc8(ctx);
	sd->status |= VB2_SD_STATUS_SECDATA_KERNEL_INIT;
}

static void secdata_kernel_test(void)
{
	reset_common_data();

	/* Blank data is invalid */
	memset(&ctx->secdata_kernel, 0xa6, sizeof(ctx->secdata_kernel));
	sec02->struct_version = VB2_SECDATA_KERNEL_VERSION_V02;
	TEST_EQ(vb2api_secdata_kernel_check(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Check blank CRC (v0.2)");
	TEST_EQ(vb2_secdata_kernel_init(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Init blank CRC");
	sec10->struct_version = VB2_SECDATA_KERNEL_VERSION_V10;
	TEST_EQ(vb2api_secdata_kernel_check(ctx),
		VB2_ERROR_SECDATA_KERNEL_STRUCT_SIZE, "Check blank size (v1)");
	TEST_EQ(vb2_secdata_kernel_init(ctx),
		VB2_ERROR_SECDATA_KERNEL_STRUCT_SIZE, "Init blank size");

	/* Ensure zeroed buffers are invalid */
	memset(&ctx->secdata_kernel, 0, sizeof(ctx->secdata_kernel));
	TEST_EQ(vb2_secdata_kernel_init(ctx), VB2_ERROR_SECDATA_KERNEL_VERSION,
		"Zeroed buffer (invalid version)");

	/* Try with bad version */
	TEST_EQ(vb2api_secdata_kernel_create(ctx), VB2_SECDATA_KERNEL_SIZE_V10,
		"Create");
	sec10->struct_version -= 1;
	TEST_EQ(vb2api_secdata_kernel_check(ctx),
		VB2_ERROR_SECDATA_KERNEL_VERSION, "Check invalid version");
	TEST_EQ(vb2_secdata_kernel_init(ctx),
		VB2_ERROR_SECDATA_KERNEL_VERSION, "Init invalid version");

	/* Create good data (v1) and corrupt it. */
	vb2api_secdata_kernel_create(ctx);
	TEST_SUCC(vb2api_secdata_kernel_check(ctx), "Check created CRC");
	TEST_SUCC(vb2_secdata_kernel_init(ctx), "Init created CRC");
	TEST_NEQ(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT, 0,
		 "Init set SD status");
	sd->status &= ~VB2_SD_STATUS_SECDATA_KERNEL_INIT;
	test_changed(ctx, 1, "Create changes data");
	ctx->secdata_kernel[2]++;
	TEST_EQ(vb2api_secdata_kernel_check(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Check invalid CRC");
	TEST_EQ(vb2_secdata_kernel_init(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Init invalid CRC");

	/* Create good data (v0.2) and corrupt it. */
	secdata_kernel_create_v02();
	ctx->secdata_kernel[2]++;
	TEST_EQ(vb2api_secdata_kernel_check(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Check invalid CRC");
	TEST_EQ(vb2_secdata_kernel_init(ctx),
		VB2_ERROR_SECDATA_KERNEL_CRC, "Init invalid CRC");
}

static void secdata_kernel_access_test_v10(void)
{
	uint32_t v = 1;
	const uint8_t *p;
	uint8_t ec_hash[VB2_SHA256_DIGEST_SIZE];

	reset_common_data();

	/* Read/write versions */
	vb2api_secdata_kernel_create(ctx);
	vb2_secdata_kernel_init(ctx);
	ctx->flags = 0;
	v = vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS);
	TEST_EQ(v, 0, "Versions created 0");
	test_changed(ctx, 0, "Get doesn't change data");
	vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS, 0x123456ff);
	test_changed(ctx, 1, "Set changes data");
	vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS, 0x123456ff);
	test_changed(ctx, 0, "Set again doesn't change data");
	v = vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS);
	TEST_EQ(v, 0x123456ff, "Versions changed");

	/* Invalid field fails */
	TEST_ABORT(vb2_secdata_kernel_get(ctx, -1), "Get invalid");
	TEST_ABORT(vb2_secdata_kernel_set(ctx, -1, 456), "Set invalid");
	test_changed(ctx, 0, "Set invalid field doesn't change data");

	/* Read/write uninitialized data fails */
	sd->status &= ~VB2_SD_STATUS_SECDATA_KERNEL_INIT;
	TEST_ABORT(vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS),
		   "Get uninitialized");
	test_changed(ctx, 0, "Get uninitialized doesn't change data");
	TEST_ABORT(vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS,
					  0x123456ff),
		   "Set uninitialized");
	test_changed(ctx, 0, "Set uninitialized doesn't change data");

	/* Test EC hash set */
	vb2api_secdata_kernel_create(ctx);
	vb2_secdata_kernel_init(ctx);
	memset(ec_hash, 0xaa, sizeof(ec_hash));
	TEST_SUCC(vb2_secdata_kernel_set_ec_hash(
			ctx, ec_hash, sizeof(ec_hash)), "Set EC hash");
	TEST_EQ(memcmp(ec_hash, sec10->ec_hash, sizeof(ec_hash)), 0,
		       "Check EC hash");
	test_changed(ctx, 1, "Set EC hash changes data");

	sec10->struct_version = VB2_SECDATA_KERNEL_VERSION_V02;
	TEST_EQ(vb2_secdata_kernel_set_ec_hash(ctx, ec_hash, sizeof(ec_hash)),
		VB2_ERROR_SECDATA_KERNEL_STRUCT_VERSION,
		"Can't set EC hash for v0.2");
	test_changed(ctx, 0, "Failing to set EC hash doesn't change data");
	sec10->struct_version = VB2_SECDATA_KERNEL_VERSION_V10;

	sd->status &= ~VB2_SD_STATUS_SECDATA_KERNEL_INIT;
	TEST_EQ(vb2_secdata_kernel_set_ec_hash(ctx, ec_hash, sizeof(ec_hash)),
		VB2_ERROR_SECDATA_KERNEL_UNINITIALIZED,
		"Can't set EC hash before init");
	sd->status |= VB2_SD_STATUS_SECDATA_KERNEL_INIT;

	TEST_EQ(vb2_secdata_kernel_set_ec_hash(ctx, ec_hash, sizeof(ec_hash)-1),
		VB2_ERROR_SECDATA_KERNEL_BUFFER_SIZE,
		"Can't set EC hash of wrong size");

	/* Test EC hash get */
	p = vb2_secdata_kernel_get_ec_hash(ctx);
	TEST_PTR_EQ(p, sec10->ec_hash, "Get EC hash returns pointer");
	test_changed(ctx, 0, "Get EC hash doesn't change data");

	sec10->struct_version = VB2_SECDATA_KERNEL_VERSION_V02;
	TEST_PTR_EQ(vb2_secdata_kernel_get_ec_hash(ctx), NULL,
		    "Can't get EC hash for v0.2");
	sec10->struct_version = VB2_SECDATA_KERNEL_VERSION_V10;

	sd->status &= ~VB2_SD_STATUS_SECDATA_KERNEL_INIT;
	TEST_PTR_EQ(vb2_secdata_kernel_get_ec_hash(ctx), NULL,
		    "Can't get EC hash before init");
	sd->status |= VB2_SD_STATUS_SECDATA_KERNEL_INIT;
}

static void secdata_kernel_access_test_v02(void)
{
	uint32_t v = 1;
	reset_common_data();

	/* Read/write versions */
	secdata_kernel_create_v02();
	vb2_secdata_kernel_init(ctx);
	ctx->flags = 0;
	v = vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS);
	TEST_EQ(v, 0, "Versions created 0");
	test_changed(ctx, 0, "Get doesn't change data");
	vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS, 0x123456ff);
	test_changed(ctx, 1, "Set changes data");
	vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS, 0x123456ff);
	test_changed(ctx, 0, "Set again doesn't change data");
	v = vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS);
	TEST_EQ(v, 0x123456ff, "Versions changed");

	/* Invalid field fails */
	TEST_ABORT(vb2_secdata_kernel_get(ctx, -1), "Get invalid");
	TEST_ABORT(vb2_secdata_kernel_set(ctx, -1, 456), "Set invalid");
	test_changed(ctx, 0, "Set invalid field doesn't change data");

	/* Read/write uninitialized data fails */
	sd->status &= ~VB2_SD_STATUS_SECDATA_KERNEL_INIT;
	TEST_ABORT(vb2_secdata_kernel_get(ctx, VB2_SECDATA_KERNEL_VERSIONS),
		   "Get uninitialized");
	test_changed(ctx, 0, "Get uninitialized doesn't change data");
	TEST_ABORT(vb2_secdata_kernel_set(ctx, VB2_SECDATA_KERNEL_VERSIONS,
					  0x123456ff),
		   "Set uninitialized");
	test_changed(ctx, 0, "Set uninitialized doesn't change data");
}

int main(int argc, char* argv[])
{
	secdata_kernel_test();
	secdata_kernel_access_test_v10();
	secdata_kernel_access_test_v02();

	return gTestSuccess ? 0 : 255;
}
