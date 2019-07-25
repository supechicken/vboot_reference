/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for kernel secure storage library.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "2sysincludes.h"

#include "test_common.h"
#include "vboot_common.h"

#include "2common.h"
#include "2api.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"

static void test_changed(struct vb2_context *c, int changed, const char *why)
{
	if (changed)
		TEST_NEQ(c->flags & VB2_CONTEXT_SECDATAK_CHANGED, 0, why);
	else
		TEST_EQ(c->flags & VB2_CONTEXT_SECDATAK_CHANGED, 0, why);

	c->flags &= ~VB2_CONTEXT_SECDATAK_CHANGED;
};

static void secdatak_test(void)
{
	uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
		__attribute__ ((aligned (VB2_WORKBUF_ALIGN)));
	struct vb2_context *ctx;

	vb2api_init_workbuf(workbuf, sizeof(workbuf));
	ctx = (struct vb2_context *)workbuf;
	uint32_t v = 1;

	/* Check size constant  */
	TEST_EQ(VB2_SECDATAK_SIZE, sizeof(struct vb2_secdatak),
		"Struct size constant");

	/* Blank data is invalid */
	memset(ctx->secdatak, 0xa6, sizeof(ctx->secdatak));
	TEST_EQ(vb2api_secdatak_check(ctx),
		VB2_ERROR_SECDATAK_CRC, "Check blank CRC");
	TEST_EQ(vb2_secdatak_init(ctx),
		 VB2_ERROR_SECDATAK_CRC, "Init blank CRC");

	/* Create good data */
	TEST_SUCC(vb2api_secdatak_create(ctx), "Create");
	TEST_SUCC(vb2api_secdatak_check(ctx), "Check created CRC");
	TEST_SUCC(vb2_secdatak_init(ctx), "Init created CRC");
	test_changed(ctx, 1, "Create changes data");

	/* Now corrupt it */
	ctx->secdatak[2]++;
	TEST_EQ(vb2api_secdatak_check(ctx),
		VB2_ERROR_SECDATAK_CRC, "Check invalid CRC");
	TEST_EQ(vb2_secdatak_init(ctx),
		 VB2_ERROR_SECDATAK_CRC, "Init invalid CRC");

	/* Make sure UID is checked */
	{
		struct vb2_secdatak *sec = (struct vb2_secdatak *)ctx->secdatak;

		vb2api_secdatak_create(ctx);
		sec->uid++;
		sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdatak, crc8));

		TEST_EQ(vb2_secdatak_init(ctx), VB2_ERROR_SECDATAK_UID,
			"Init invalid struct UID");
	}

	/* Read/write versions */
	vb2api_secdatak_create(ctx);
	ctx->flags = 0;
	TEST_SUCC(vb2_secdatak_get(ctx, VB2_SECDATAK_VERSIONS, &v),
		  "Get versions");
	TEST_EQ(v, 0, "Versions created 0");
	test_changed(ctx, 0, "Get doesn't change data");
	TEST_SUCC(vb2_secdatak_set(ctx, VB2_SECDATAK_VERSIONS, 0x123456ff),
		  "Set versions");
	test_changed(ctx, 1, "Set changes data");
	TEST_SUCC(vb2_secdatak_set(ctx, VB2_SECDATAK_VERSIONS, 0x123456ff),
		  "Set versions 2");
	test_changed(ctx, 0, "Set again doesn't change data");
	TEST_SUCC(vb2_secdatak_get(ctx, VB2_SECDATAK_VERSIONS, &v),
		  "Get versions 2");
	TEST_EQ(v, 0x123456ff, "Versions changed");

	/* Invalid field fails */
	TEST_EQ(vb2_secdatak_get(ctx, -1, &v),
		VB2_ERROR_SECDATAK_GET_PARAM, "Get invalid");
	TEST_EQ(vb2_secdatak_set(ctx, -1, 456),
		VB2_ERROR_SECDATAK_SET_PARAM, "Set invalid");
	test_changed(ctx, 0, "Set invalid field doesn't change data");

	/* Read/write uninitialized data fails */
	vb2_get_sd(ctx)->status &= ~VB2_SD_STATUS_SECDATAK_INIT;
	TEST_EQ(vb2_secdatak_get(ctx, VB2_SECDATAK_VERSIONS, &v),
		VB2_ERROR_SECDATAK_GET_UNINITIALIZED, "Get uninitialized");
	test_changed(ctx, 0, "Get uninitialized doesn't change data");
	TEST_EQ(vb2_secdatak_set(ctx, VB2_SECDATAK_VERSIONS, 0x123456ff),
		VB2_ERROR_SECDATAK_SET_UNINITIALIZED, "Set uninitialized");
	test_changed(ctx, 0, "Set uninitialized doesn't change data");
}

int main(int argc, char* argv[])
{
	secdatak_test();

	return gTestSuccess ? 0 : 255;
}
