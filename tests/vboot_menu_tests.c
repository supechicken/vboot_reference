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
#include "vboot_ui_common.h"

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
static void ResetMocks(void)
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
	printf("VbDisplayScreen: screens_displayed[%d] = %#x\n",
	       screens_count - 1, screen);
	return VB2_SUCCESS;
}

/* Tests */

/*
 * VbBootNormal tests: Please see VbBootTest in vboot_api_kernel2_tests.c
 * and VbBootDevTest/VbBootRecTest in vboot_legacy_menu_tests.c
 */

static void VbBootDevTest(void)
{
	printf("Testing VbBootDeveloperMenu()...\n");

	/* Developer entry point */
	ResetMocks();
	TEST_EQ(VbBootDeveloperMenu(ctx), vbtlk_retval_fixed, "entry point");
	TEST_EQ(screens_displayed[0], VB_SCREEN_BLANK, "  blank screen");
	TEST_EQ(screens_count, 1, "  no extra screens");


	printf("...done.\n");
}

static void VbBootRecTest(void)
{
	printf("Testing VbBootRecoveryMenu()...\n");

	/* Recovery entry point */
	ResetMocks();
	TEST_EQ(VbBootRecoveryMenu(ctx), vbtlk_retval_fixed, "entry point");
	TEST_EQ(screens_displayed[0], VB_SCREEN_BLANK, "  blank screen");
	TEST_EQ(screens_count, 1, "  no extra screens");


	printf("...done.\n");
}

int main(void)
{
	VbBootDevTest();
	VbBootRecTest();

	return gTestSuccess ? 0 : 255;
}
