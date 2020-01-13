/* Copyright 2020 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common code used by both vboot_ui and vboot_ui_vendor_data.
 */

#ifndef VBOOT_REFERENCE_VBOOT_UI_VENDOR_DATA_H_
#define VBOOT_REFERENCE_VBOOT_UI_VENDOR_DATA_H_

/*
 * User interface for setting the vendor data in VPD
 */
vb2_error_t vb2_vendor_data_ui(struct vb2_context *ctx);

/**
 * Checks GBB flags against VbExIsShutdownRequested() shutdown request to
 * determine if a shutdown is required.
 *
 * Returns zero or more of the following flags (if any are set then typically
 * shutdown is required):
 * VB_SHUTDOWN_REQUEST_LID_CLOSED
 * VB_SHUTDOWN_REQUEST_POWER_BUTTON
 */
int VbWantShutdown(struct vb2_context *ctx, uint32_t key);

#endif  /* VBOOT_REFERENCE_VBOOT_UI_VENDOR_DATA_H_ */
