/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for vb2api_check_kernel_version.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sysincludes.h"
#include "common/boot_mode.h"
#include "common/tests.h"
#include "host_common.h"
#include "tlcl.h"
#include "tss_constants.h"
#include "vboot_struct.h"

/* Common context for tests */
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static uint32_t kernel_version;
static vb2_kernel_params kparams;

/* Mocked function data */
static struct vb2_gbb_header mock_gbb;
static int mock_vbtlk_expect_fixed;
static int mock_vbtlk_expect_removable;
static vb2_error_t mock_vbtlk_retval;

static void reset_common_data(void)
{
	memset(&kparams, 0, sizeof(kparams));

	memset(&mock_gbb, 0, sizeof(mock_gbb));
	mock_gbb.major_version = VB2_GBB_MAJOR_VER;
	mock_gbb.minor_version = VB2_GBB_MINOR_VER;
	mock_gbb.flags = 0;

	mock_vbtlk_expect_fixed = 1;
	mock_vbtlk_expect_removable = 0;
	mock_vbtlk_retval = VB2_SUCCESS;

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");

	SET_BOOT_MODE(ctx, VB2_BOOT_MODE_NORMAL);

	sd = vb2_get_sd(ctx);

	vb2_nv_init(ctx);
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0xffffffff);

	kernel_version = 0x10002;

	sd->kernel_version_secdata = kernel_version;
	sd->kernel_version = kernel_version;
}

/* Mock functions */

struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &mock_gbb;
}

void vb2_secdata_kernel_set(struct vb2_context *c,
			    enum vb2_secdata_kernel_param param,
			    uint32_t value)
{
	kernel_version = value;
}

/* Tests */

static void check_kernel_version_tests(void)
{
	reset_common_data();
	sd->kernel_version = 0x20003;
	vb2api_check_kernel_version(ctx);
	TEST_EQ(kernel_version, 0x20003, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_FW_RESULT, VB2_FW_RESULT_TRYING);
	sd->kernel_version = 0x20003;
	vb2api_check_kernel_version(ctx);
	TEST_EQ(kernel_version, 0x10002, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0x30005);
	sd->kernel_version = 0x40006;
	vb2api_check_kernel_version(ctx);
	TEST_EQ(kernel_version, 0x30005, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0x10001);
	sd->kernel_version = 0x40006;
	vb2api_check_kernel_version(ctx);
	TEST_EQ(kernel_version, 0x10002, "  version");
}

int main(void)
{
	check_kernel_version_tests();
	return gTestSuccess ? 0 : 255;
}
