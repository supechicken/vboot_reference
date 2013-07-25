/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Access to portions of the firmware image, perhaps later to be expanded
 * to other devices.
 */

#ifndef VBOOT_REFERENCE_REGION_H_
#define VBOOT_REFERENCE_REGION_H_

#include "bmpblk_header.h"
#include "gbb_header.h"
#include "vboot_api.h"
#include "vboot_struct.h"

/* The maximum length of a hardware ID */
#define VB_REGION_HWID_LEN	256

struct LoadKernelParams;

VbError_t VbRegionReadData(VbCommonParams *cparams,
			   enum vb_firmware_region region, uint32_t offset,
			   uint32_t size, void *buf);

void VbRegionCheckVersion(VbCommonParams *cparams);

VbError_t VbRegionReadHwID(VbCommonParams *cparams, char *hwid,
			   uint32_t max_size);

#endif  /* VBOOT_REFERENCE_REGION_H_ */
