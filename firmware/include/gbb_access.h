/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Access to portions of the GBB.
 */

#ifndef VBOOT_REFERENCE_GBB_ACCESS_H_
#define VBOOT_REFERENCE_GBB_ACCESS_H_

#include "vboot_api.h"

struct BmpBlockHeader;
struct ImageInfo;
struct GoogleBinaryBlockHeader;
struct ScreenLayout;
struct VbPublicKey;

VbError_t VbGbbReadHeader_static(VbCommonParams *cparams,
				 struct GoogleBinaryBlockHeader *gbb);

VbError_t VbGbbReadRootKey(VbCommonParams *cparams,
			   struct VbPublicKey **keyp);

VbError_t VbGbbReadRecoveryKey(VbCommonParams *cparams,
			       struct VbPublicKey **keyp);

VbError_t VbGbbReadBmpHeader(VbCommonParams *cparams,
			     struct BmpBlockHeader **hdrp);

VbError_t VbGbbReadImage(VbCommonParams *cparams,
			 uint32_t localization, uint32_t screen_index,
			 uint32_t image_num, struct ScreenLayout *layout,
			 struct ImageInfo *image_info, char **image_datap,
			 uint32_t *image_data_sizep);

VbError_t VbGbbReadHeader(VbCommonParams *cparams);

#endif
