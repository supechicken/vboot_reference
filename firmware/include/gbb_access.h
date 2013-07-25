/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Access to portions of the GBB.
 */

#ifndef VBOOT_REFERENCE_GBB_H_
#define VBOOT_REFERENCE_GBB_H_

#include "bmpblk_header.h"
#include "gbb_header.h"
#include "vboot_api.h"
#include "vboot_struct.h"

struct LoadKernelParams;
struct BmpBlockHeader;

VbError_t VbGbbGetData_cparams(VbCommonParams *cparams, uint32_t offset,
			       uint32_t size, void *buf);

VbError_t VbGbbReadHeader_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb);

VbError_t VbGbbGetRootKey_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb,
				  VbPublicKey **keyp);

VbError_t VbGbbGetRecoveryKey_cparams(VbCommonParams *cparams,
				      GoogleBinaryBlockHeader *gbb,
				      VbPublicKey **keyp);

VbError_t VbGbbGetRootKey(struct LoadKernelParams *lkparams,
			  GoogleBinaryBlockHeader *gbb, VbPublicKey **keyp);

VbError_t VbGbbGetRecoveryKey(struct LoadKernelParams *lkparams,
			      GoogleBinaryBlockHeader *gbb,
			      VbPublicKey **keyp);

VbError_t VbGbbGetBmpHeader(struct LoadKernelParams *lkparams,
			    BmpBlockHeader **hdrp);

VbError_t VbGbbGetHwID(struct LoadKernelParams *lkparams, char *hwid,
		       uint32_t max_size);

VbError_t VbGbbGetImage(struct LoadKernelParams *lkparams,
			uint32_t localization, uint32_t screen_index,
			uint32_t image_num, ScreenLayout *layout,
			ImageInfo *image_info, char **image_datap,
			uint32_t *image_data_sizep);

void VbGbbCheckVersion(struct LoadKernelParams *lkparams);

VbError_t VbGbbReadHeader(struct LoadKernelParams *lkparams);

#endif  /* VBOOT_REFERENCE_GBB_H_ */
