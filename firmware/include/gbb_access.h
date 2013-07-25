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

struct BmpBlockHeader;

struct LoadKernelParams *VbGetKernelParams(VbCommonParams *cparams);

VbError_t VbGbbGetHeader_Read(VbCommonParams *cparams,
			      GoogleBinaryBlockHeader *gbb);

VbError_t VbGbbGetRootKey(VbCommonParams *cparams,
			  GoogleBinaryBlockHeader *gbb, VbPublicKey **keyp);

VbError_t VbGbbGetRecoveryKey(VbCommonParams *cparams,
			      GoogleBinaryBlockHeader *gbb,
			      VbPublicKey **keyp);

VbError_t VbGbbGetBmpHeader(VbCommonParams *cparams, BmpBlockHeader **bmpp);

VbError_t VbGbbGetHwID(VbCommonParams *cparams, char *hwid, uint32_t max_size);

VbError_t VbGbbGetImage(VbCommonParams *cparams, uint32_t localization,
			uint32_t screen_index, uint32_t image_num,
			ScreenLayout *layout, ImageInfo *image_info,
			char **image_datap, uint32_t *image_data_sizep);

void VbGbbCheckVersion(VbCommonParams *cparams);

VbError_t VbGbbGetHeader(VbCommonParams *cparams,
			 GoogleBinaryBlockHeader **gbbp);

#endif  /* VBOOT_REFERENCE_GBB_H_ */
