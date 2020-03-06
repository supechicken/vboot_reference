/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests menu UI
 */

#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2ui.h"
#include "test_common.h"
#include "vboot_api.h"
#include "vboot_kernel.h"

#define SET_RETVAL(retval, flag) ((retval)-(flag))

/* Mock data */
static LoadKernelParams lkp;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;
static struct vb2_secdata_fwmp *fwmp;
static vb2_error_t vbtlk_retval[5];
static vb2_error_t vbtlk_last_retval;
static int vbtlk_retval_count;
static int vbtlk_retval_total;

static void add_vbtlk_retval(vb2_error_t retval, uint32_t get_info_flags)
{
	if (vbtlk_retval_total < ARRAY_SIZE(vbtlk_retval)) {
		vbtlk_retval[vbtlk_retval_total] = SET_RETVAL(retval,
							      get_info_flags);
		vbtlk_retval_total++;
	}
	else {
		fprintf(stderr, "Ran out of vbtlk_retval entries!\n");
	}
}

/* Reset mock data (for use before each test) */
static void reset_common_data()
{
	memset(&lkp, 0, sizeof(lkp));

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	vb2_nv_init(ctx);

	sd = vb2_get_sd(ctx);

	/* CRC will be invalid after here, but nobody's checking */
	sd->status |= VB2_SD_STATUS_SECDATA_FWMP_INIT;
	fwmp = (struct vb2_secdata_fwmp *)ctx->secdata_fwmp;

	memset(&gbb, 0, sizeof(gbb));

	memset(vbtlk_retval, 0, sizeof(vbtlk_retval));
	vbtlk_last_retval = SET_RETVAL(VB2_ERROR_MOCK, VB_DISK_FLAG_FIXED);
	vbtlk_retval_count = 0;
	vbtlk_retval_total = 0;
}

/* Mock functions */

struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	if (vbtlk_retval_count < vbtlk_retval_total &&
	    vbtlk_retval[vbtlk_retval_count] != 0)
		vbtlk_last_retval = vbtlk_retval[vbtlk_retval_count++];

	return vbtlk_last_retval + get_info_flags;
}

/* Tests */

/* VbootNormal() tests from vboot_kernel.h */
static void normal_tests(void)
{
	reset_common_data();
	add_vbtlk_retval(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	vbtlk_last_retval = VB2_SUCCESS - VB_DISK_FLAG_FIXED;
	TEST_EQ(VbBootNormal(ctx), VB2_SUCCESS,
		"VbBootNormal() returns VB2_SUCCESS");

	reset_common_data();
	TEST_EQ(VbBootNormal(ctx), VB2_ERROR_MOCK,
		"VbBootNormal() returns VB2_ERROR_MOCK");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_DISPLAY_REQUEST, 1);
	TEST_EQ(VbBootNormal(ctx), VBERROR_REBOOT_REQUIRED,
		"VbBootNormal() reboot to reset NVRAM display request");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_DISPLAY_REQUEST), 0,
		"  display request reset");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_DIAG_REQUEST, 1);
	TEST_EQ(VbBootNormal(ctx), VBERROR_REBOOT_REQUIRED,
		"VbBootNormal() reboot to reset NVRAM diag request");
	TEST_EQ(vb2_nv_get(ctx, VB2_NV_DIAG_REQUEST), 0,
		"  diag request reset");
}

static void developer_tests(void)
{
	/* TODO(roccochen) */
}

static void broken_recovery_tests(void)
{
	/* TODO(roccochen) */
}

static void manual_recovery_tests(void)
{
	/* TODO(roccochen) */
}

int main(void)
{
	normal_tests();
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
