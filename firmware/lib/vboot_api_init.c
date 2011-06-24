/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware wrapper API - entry points for init, firmware selection
 */

#include "gbb_header.h"
#include "load_firmware_fw.h"
#include "rollback_index.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_nvstorage.h"


VbError_t VbInit(VbCommonParams* cparams, VbInitParams* iparams) {
  VbSharedDataHeader* shared = (VbSharedDataHeader*)cparams->shared_data_blob;
  VbNvContext vnc;
  uint32_t recovery = VBNV_RECOVERY_NOT_REQUESTED;

  /* Initialize output flags */
  iparams->out_flags = 0;

  /* Set up NV storage and read the current recovery request */
  VbExNvStorageRead(vnc.raw);
  VbNvSetup(&vnc);
  VbNvGet(&vnc, VBNV_RECOVERY_REQUEST, &recovery);

  /* Initialize shared data structure */
  if (0 != VbSharedDataInit(shared, cparams->shared_data_size)) {
    VBDEBUG(("Shared data init error\n"));
    return 1;
  }

  /* Copy boot switch flags */
  shared->flags = 0;
  if (iparams->flags & VB_INIT_FLAG_DEV_SWITCH_ON)
    shared->flags |= VBSD_BOOT_DEV_SWITCH_ON;
  if (iparams->flags & VB_INIT_FLAG_REC_BUTTON_PRESSED)
    shared->flags |= VBSD_BOOT_REC_SWITCH_ON;
  if (iparams->flags & VB_INIT_FLAG_WP_ENABLED)
    shared->flags |= VBSD_BOOT_FIRMWARE_WP_ENABLED;

  /* If recovery button is pressed, override recovery reason */
  if (iparams->flags & VB_INIT_FLAG_REC_BUTTON_PRESSED)
    recovery = VBNV_RECOVERY_RO_MANUAL;

  /* Set output flags */
  if (VBNV_RECOVERY_NOT_REQUESTED != recovery) {
    /* Requesting recovery mode */
    iparams->out_flags |= (VB_INIT_OUT_ENABLE_RECOVERY |
                          VB_INIT_OUT_CLEAR_RAM |
                          VB_INIT_OUT_ENABLE_DISPLAY |
                          VB_INIT_OUT_ENABLE_USB_STORAGE);
  }
  else if (iparams->flags & VB_INIT_FLAG_DEV_SWITCH_ON) {
    /* Developer switch is on, so need to support dev mode */
    iparams->out_flags |= (VB_INIT_OUT_CLEAR_RAM |
                          VB_INIT_OUT_ENABLE_DISPLAY |
                          VB_INIT_OUT_ENABLE_USB_STORAGE);
  }

  /* Copy current recovery reason to shared data */
  shared->recovery_reason = (uint8_t)recovery;

  /* Clear the recovery request, so we won't get stuck in recovery mode */
  VbNvSet(&vnc, VBNV_RECOVERY_REQUEST, VBNV_RECOVERY_NOT_REQUESTED);

  /* Tear down NV storage */
  VbNvTeardown(&vnc);
  if (vnc.raw_changed)
    VbExNvStorageWrite(vnc.raw);

  return VBERROR_SUCCESS;
}


VbError_t VbS3Resume(void) {

  /* TODO: handle test errors (requires passing in VbNvContext) */

  /* Resume the TPM */
  uint32_t status = RollbackS3Resume();

  /* If we can't resume, just do a full reboot.  No need to go to recovery
   * mode here, since if the TPM is really broken we'll catch it on the
   * next boot. */
  if (status == TPM_SUCCESS)
    return VBERROR_SUCCESS;
  else
    return 1;
}
