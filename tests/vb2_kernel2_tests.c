/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for vb2api_normal_boot.
 */

#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sysincludes.h"
#include "host_common.h"
#include "load_kernel_fw.h"
#include "test_common.h"
#include "tlcl.h"
#include "tss_constants.h"
#include "vboot_struct.h"

/* Mock data */

static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE]
	__attribute__((aligned(VB2_WORKBUF_ALIGN)));
static struct vb2_context *ctx;
static struct vb2_shared_data *sd;
static VbSelectAndLoadKernelParams kparams;
static struct vb2_gbb_header gbb;

static uint32_t kernel_version;
static uint32_t new_version;

static void reset_common_data(void)
{
	memset(&kparams, 0, sizeof(kparams));

	memset(&gbb, 0, sizeof(gbb));
	gbb.major_version = VB2_GBB_MAJOR_VER;
	gbb.minor_version = VB2_GBB_MINOR_VER;
	gbb.flags = 0;

	TEST_SUCC(vb2api_init(workbuf, sizeof(workbuf), &ctx),
		  "vb2api_init failed");
	sd = vb2_get_sd(ctx);

	vb2_nv_init(ctx);
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0xffffffff);

	kernel_version = new_version = 0x10002;

	sd->kernel_version_secdata = kernel_version;
	sd->kernel_version = kernel_version;
}

/* Mock functions */

struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

void vb2_secdata_kernel_set(struct vb2_context *c,
			    enum vb2_secdata_kernel_param param,
			    uint32_t value)
{
	kernel_version = value;
}

vb2_error_t VbTryLoadKernel(struct vb2_context *c, uint32_t disk_flags,
			    VbSelectAndLoadKernelParams *kpa)
{
	sd->kernel_version = new_version;

	return VB2_SUCCESS;
}

static void normal_boot_kernel_version_tests(void)
{
	reset_common_data();
	new_version = 0x20003;
	TEST_EQ(vb2api_normal_boot(ctx, &kparams), 0, "Roll forward");
	TEST_EQ(kernel_version, 0x20003, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_FW_RESULT, VB2_FW_RESULT_TRYING);
	new_version = 0x20003;
	TEST_EQ(vb2api_normal_boot(ctx, &kparams), 0,
		"Don't roll forward kernel when trying new FW");
	TEST_EQ(kernel_version, 0x10002, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0x30005);
	new_version = 0x40006;
	TEST_EQ(vb2api_normal_boot(ctx, &kparams), 0, "Limit max roll forward");
	TEST_EQ(kernel_version, 0x30005, "  version");

	reset_common_data();
	vb2_nv_set(ctx, VB2_NV_KERNEL_MAX_ROLLFORWARD, 0x10001);
	new_version = 0x40006;
	TEST_EQ(vb2api_normal_boot(ctx, &kparams), 0,
		"Max roll forward can't rollback");
	TEST_EQ(kernel_version, 0x10002, "  version");
}

int main(void)
{
	normal_boot_kernel_version_tests();

	return gTestSuccess ? 0 : 255;
}
