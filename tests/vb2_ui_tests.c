/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests menu UI
 */

#include "2common.h"
#include "2nvstorage.h"
#include "test_common.h"
#include "vboot_api.h"
#include "vboot_display.h"
#include "vboot_kernel.h"

/* Mock data */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;

static vb2_error_t vbtlk_retval[5];
static vb2_error_t vbtlk_last_retval;
static int vbtlk_retval_count;
static const vb2_error_t vbtlk_retval_fixed = 1002;
static uint32_t screens_displayed[64];
static uint32_t screens_count = 0;

/* Reset mock data (for use before each test) */
static void reset_common_data(void)
{
	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	vb2_nv_init(ctx);

	vbtlk_last_retval = vbtlk_retval_fixed - VB_DISK_FLAG_FIXED;
	memset(vbtlk_retval, 0, sizeof(vbtlk_retval));
	vbtlk_retval_count = 0;

	memset(screens_displayed, 0, sizeof(screens_displayed));
	screens_count = 0;
}

/* Mock functions */

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t get_info_flags)
{
	if (vbtlk_retval_count < ARRAY_SIZE(vbtlk_retval) &&
	    vbtlk_retval[vbtlk_retval_count] != 0)
		vbtlk_last_retval = vbtlk_retval[vbtlk_retval_count++];
	return vbtlk_last_retval + get_info_flags;
}

vb2_error_t VbDisplayScreen(struct vb2_context *c, uint32_t screen, int force,
			    const VbScreenData *data)
{
	if (screens_count < ARRAY_SIZE(screens_displayed))
		screens_displayed[screens_count++] = screen;
	VB2_DEBUG("VbDisplayScreen: screens_displayed[%d] = %#x\n",
		  screens_count - 1, screen);
	return VB2_SUCCESS;
}

/* Tests */

/*
 * VbootNormal tests
 *
 * This test function references vboot_legacy_clamshell_tests.c, which will
 * be deprecated.
 */
static void normal_tests(void)
{
	// TODO(roccochen): I have not figured out how to write this
	/*reset_common_data();
	vbtlk_retval[0] = VB2_SUCCESS;
	TEST_EQ(VbBootNormal(ctx), VB2_SUCCESS,
		"VbBootNormal() returns VB2_SUCCESS");

	reset_common_data();
	vbtlk_retval[0] = VB2_SUCCESS + VB2_ERROR_MOCK;
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
		"  diag request reset");*/
}

static void developer_tests(void)
{
	/* Developer entry point */
	reset_common_data();
}

static void broken_recovery_tests(void)
{
	/* Recovery entry point for very broken non-manual recovery */
	reset_common_data();
}

static void manual_recovery_tests(void)
{
	/* Recovery entry point for manual recovery */
	reset_common_data();
}

int main(void)
{
	normal_tests();
	developer_tests();
	broken_recovery_tests();
	manual_recovery_tests();

	return gTestSuccess ? 0 : 255;
}
