/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Access to portions of the GBB.
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
struct BmpBlockHeader;

VbError_t VbRegionReadData_cparams(VbCommonParams *cparams, uint32_t offset,
				   uint32_t size, void *buf);

VbError_t VbRegionReadGbbHeader_cparams(VbCommonParams *cparams,
					GoogleBinaryBlockHeader *gbb);

VbError_t VbRegionReadGbbRootKey_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb,
				  VbPublicKey **keyp);

VbError_t VbRegionReadGbbRecoveryKey_cparams(VbCommonParams *cparams,
				      GoogleBinaryBlockHeader *gbb,
				      VbPublicKey **keyp);

VbError_t VbRegionReadGbbRootKey(struct LoadKernelParams *lkparams,
			  GoogleBinaryBlockHeader *gbb, VbPublicKey **keyp);

VbError_t VbRegionReadRecoveryKey(struct LoadKernelParams *lkparams,
			      GoogleBinaryBlockHeader *gbb,
			      VbPublicKey **keyp);

VbError_t VbRegionReadBmpHeader(struct LoadKernelParams *lkparams,
			    BmpBlockHeader **hdrp);

VbError_t VbRegionReadHwID(struct LoadKernelParams *lkparams, char *hwid,
		       uint32_t max_size);

VbError_t VbRegionReadGbbImage(struct LoadKernelParams *lkparams,
			uint32_t localization, uint32_t screen_index,
			uint32_t image_num, ScreenLayout *layout,
			ImageInfo *image_info, char **image_datap,
			uint32_t *image_data_sizep);

void VbRegionCheckVersion(struct LoadKernelParams *lkparams);

VbError_t VbRegionReadGbbHeader(struct LoadKernelParams *lkparams);

#endif  /* VBOOT_REFERENCE_REGION_H_ */
