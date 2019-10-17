/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for vboot_api_kernel, part 3 - software sync
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2sysincludes.h"
#include "ec_sync.h"
#include "host_common.h"
#include "load_kernel_fw.h"
#include "secdata_tpm.h"
#include "test_common.h"
#include "vboot_audio.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_kernel.h"
#include "vboot_struct.h"

/* Mock data */
static uint8_t shared_data[VB_SHARED_DATA_MIN_SIZE];
static VbSharedDataHeader *shared = (VbSharedDataHeader *)shared_data;

static int mock_in_rw;
static vb2_error_t in_rw_retval;
static int protect_retval;
static int ec_ro_protected;
static int ec_rw_protected;
static int run_retval;
static int ec_run_image;
static int update_retval;
static int ec_ro_updated;
static int ec_rw_updated;
static int get_expected_retval;
static int shutdown_request_calls_left;

static struct vb2_context ctx;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE];
static struct vb2_shared_data *sd;
static struct vb2_gbb_header gbb;

static uint32_t screens_displayed[8];
static uint32_t screens_count = 0;

static vb2_error_t ec_aux_fw_retval;
static int ec_aux_fw_update_req;
static VbAuxFwUpdateSeverity_t ec_aux_fw_mock_severity;
static VbAuxFwUpdateSeverity_t ec_aux_fw_update_severity;
static int ec_aux_fw_protected;

/* Reset mock data (for use before each test) */
static void ResetMocks(void)
{
	memset(&ctx, 0, sizeof(ctx));
	ctx.workbuf = workbuf;
	ctx.workbuf_size = sizeof(workbuf);
	ctx.flags = VB2_CONTEXT_EC_SYNC_SUPPORTED;
	vb2_init_context(&ctx);
	vb2_nv_init(&ctx);

	sd = vb2_get_sd(&ctx);
	sd->vbsd = shared;
	sd->flags |= VB2_SD_FLAG_DISPLAY_AVAILABLE;
	sd->flags |= VB2_SD_FLAG_ECSYNC_EC_RW;

	memset(&gbb, 0, sizeof(gbb));

	memset(&shared_data, 0, sizeof(shared_data));
	VbSharedDataInit(shared, sizeof(shared_data));

	mock_in_rw = 0;
	ec_ro_protected = 0;
	ec_rw_protected = 0;
	ec_run_image = 0;   /* 0 = RO, 1 = RW */
	ec_ro_updated = 0;
	ec_rw_updated = 0;
	in_rw_retval = VB2_SUCCESS;
	protect_retval = VB2_SUCCESS;
	update_retval = VB2_SUCCESS;
	run_retval = VB2_SUCCESS;
	get_expected_retval = VB2_SUCCESS;
	shutdown_request_calls_left = -1;

	// TODO: ensure these are actually needed

	memset(screens_displayed, 0, sizeof(screens_displayed));
	screens_count = 0;

	ec_aux_fw_retval = VB2_SUCCESS;
	ec_aux_fw_mock_severity = VB_AUX_FW_NO_UPDATE;
	ec_aux_fw_update_severity = VB_AUX_FW_NO_UPDATE;
	ec_aux_fw_update_req = 0;
	ec_aux_fw_protected = 0;
}

/* Mock functions */
struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return &gbb;
}

uint32_t VbExIsShutdownRequested(void)
{
	if (shutdown_request_calls_left == 0)
		return 1;
	else if (shutdown_request_calls_left > 0)
		shutdown_request_calls_left--;

	return 0;
}

int VbExTrustEC(int devidx)
{
	return !mock_in_rw;
}

vb2_error_t VbExEcRunningRW(int devidx, int *in_rw)
{
	*in_rw = mock_in_rw;
	return in_rw_retval;
}

vb2_error_t VbExEcProtect(int devidx, enum VbSelectFirmware_t select)
{
	if (select == VB_SELECT_FIRMWARE_READONLY)
		ec_ro_protected = 1;
	else
		ec_rw_protected = 1;
	return protect_retval;
}

vb2_error_t VbExEcDisableJump(int devidx)
{
	return run_retval;
}

vb2_error_t VbExEcJumpToRW(int devidx)
{
	ec_run_image = 1;
	mock_in_rw = 1;
	return run_retval;
}

vb2_error_t VbExEcHashImage(int devidx, enum VbSelectFirmware_t select,
			    const uint8_t **hash, int *hash_size)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExEcGetExpectedImage(int devidx, enum VbSelectFirmware_t select,
				   const uint8_t **image, int *image_size)
{
	static uint8_t fake_image[64] = {5, 6, 7, 8};
	*image = fake_image;
	*image_size = sizeof(fake_image);
	return get_expected_retval;
}

vb2_error_t VbExEcGetExpectedImageHash(int devidx,
				       enum VbSelectFirmware_t select,
				       const uint8_t **hash, int *hash_size)
{
	return VB2_SUCCESS;
}

vb2_error_t VbExEcUpdateImage(int devidx, enum VbSelectFirmware_t select,
			      const uint8_t *image, int image_size)
{
	if (select == VB_SELECT_FIRMWARE_READONLY) {
		ec_ro_updated = 1;
	 } else {
		ec_rw_updated = 1;
	}
	return update_retval;
}

vb2_error_t VbDisplayScreen(struct vb2_context *c, uint32_t screen, int force,
			    const VbScreenData *data)
{
	if (screens_count < ARRAY_SIZE(screens_displayed))
		screens_displayed[screens_count++] = screen;

	return VB2_SUCCESS;
}

vb2_error_t VbExCheckAuxFw(VbAuxFwUpdateSeverity_t *severity)
{
	*severity = ec_aux_fw_mock_severity;
	ec_aux_fw_update_severity = ec_aux_fw_mock_severity;
	return VB2_SUCCESS;
}

vb2_error_t VbExUpdateAuxFw()
{
	if (ec_aux_fw_update_severity != VB_AUX_FW_NO_DEVICE &&
	    ec_aux_fw_update_severity != VB_AUX_FW_NO_UPDATE)
		ec_aux_fw_update_req = 1;
	return ec_aux_fw_retval;
}

vb2_error_t VbExEcVbootDone(int in_recovery)
{
	ec_aux_fw_protected = ec_aux_fw_update_severity != VB_AUX_FW_NO_DEVICE;
	return ec_aux_fw_retval;
}

static void test_ssync(vb2_error_t retval, int recovery_reason,
		       const char *desc)
{
	TEST_EQ(auxfw_sync_all(&ctx), retval, desc);
	TEST_EQ(vb2_nv_get(&ctx, VB2_NV_RECOVERY_REQUEST),
		recovery_reason, "  recovery reason");
}

/* Tests */

static void VbSoftwareSyncTest(void)
{
	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	sd->flags &= ~VB2_SD_FLAG_DISPLAY_AVAILABLE;
	sd->flags |= VB2_SD_FLAG_ECSYNC_EC_RO;
	sd->flags &= ~VB2_SD_FLAG_ECSYNC_EC_RW;
	test_ssync(VBERROR_REBOOT_REQUIRED, 0,
		   "Slow update - reboot for display (EC RO)");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	sd->flags &= ~VB2_SD_FLAG_DISPLAY_AVAILABLE;
	test_ssync(VBERROR_REBOOT_REQUIRED, 0,
		   "Slow update - reboot for display (EC RW)");

	ResetMocks();
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	vb2_nv_set(&ctx, VB2_NV_DISPLAY_REQUEST, 1);
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	test_ssync(VBERROR_EC_REBOOT_TO_RO_REQUIRED, 0,
		   "Slow update with display request");
	TEST_EQ(screens_displayed[0], VB_SCREEN_WAIT, "  wait screen");
	TEST_EQ(vb2_nv_get(&ctx, VB2_NV_DISPLAY_REQUEST), 1,
		"  DISPLAY_REQUEST left untouched");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_FAST_UPDATE;
	test_ssync(VBERROR_EC_REBOOT_TO_RO_REQUIRED, 0,
		   "Fast auxiliary FW update needed");
	TEST_EQ(screens_count, 0,
		"  wait screen skipped");
	TEST_EQ(ec_aux_fw_update_req, 1, "  aux fw update requested");
	TEST_EQ(ec_aux_fw_protected, 0, "  aux fw protected");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_NO_DEVICE;
	test_ssync(VB2_SUCCESS, 0,
		   "No auxiliary FW update needed");
	TEST_EQ(screens_count, 0,
		"  wait screen skipped");
	TEST_EQ(ec_aux_fw_update_req, 0, "  no aux fw update requested");
	TEST_EQ(ec_aux_fw_protected, 0, "  no aux fw protected");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_NO_UPDATE;
	test_ssync(VB2_SUCCESS, 0,
		   "No auxiliary FW update needed");
	TEST_EQ(screens_count, 0,
		"  wait screen skipped");
	TEST_EQ(ec_aux_fw_update_req, 0, "  no aux fw update requested");
	TEST_EQ(ec_aux_fw_protected, 0, "  aux fw protected");

	ResetMocks();
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	sd->flags &= ~VB2_SD_FLAG_DISPLAY_AVAILABLE;
	test_ssync(VBERROR_REBOOT_REQUIRED, 0,
		   "Slow auxiliary FW update needed - reboot for display");
	TEST_EQ(vb2_nv_get(&ctx, VB2_NV_DISPLAY_REQUEST), 1,
		"  DISPLAY_REQUEST is enabled");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	vb2_nv_set(&ctx, VB2_NV_DISPLAY_REQUEST, 0);
	test_ssync(VBERROR_EC_REBOOT_TO_RO_REQUIRED, 0,
		   "Slow update without display request (no reboot needed)");
	TEST_EQ(screens_displayed[0], VB_SCREEN_WAIT, "  wait screen");
	TEST_EQ(vb2_nv_get(&ctx, VB2_NV_DISPLAY_REQUEST), 0,
		"  DISPLAY_REQUEST left untouched");

	ResetMocks();
	ec_aux_fw_mock_severity = VB_AUX_FW_FAST_UPDATE;
	ec_aux_fw_retval = VB2_ERROR_UNKNOWN;
	test_ssync(VB2_ERROR_UNKNOWN, VB2_RECOVERY_AUX_FW_UPDATE,
		   "Error updating AUX firmware");

	ResetMocks();
	ctx.flags |= VB2_CONTEXT_EC_SYNC_SLOW;
	ec_aux_fw_mock_severity = VB_AUX_FW_SLOW_UPDATE;
	test_ssync(VBERROR_EC_REBOOT_TO_RO_REQUIRED, 0,
		   "Slow auxiliary FW update needed");
	TEST_EQ(ec_aux_fw_update_req, 1, "  aux fw update requested");
	TEST_EQ(ec_aux_fw_protected, 0, "  aux fw protected");
	TEST_EQ(screens_displayed[0], VB_SCREEN_WAIT,
		"  wait screen forced");
}

int main(void)
{
	VbSoftwareSyncTest();

	return gTestSuccess ? 0 : 255;
}

