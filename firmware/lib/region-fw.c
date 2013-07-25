/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware API for loading and verifying rewritable firmware.
 * (Firmware portion)
 */

#include "sysincludes.h"

#include "bmpblk_header.h"
#include "region.h"
#include "gbb_header.h"
#include "load_kernel_fw.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_struct.h"

static VbError_t VbRegionReadGbbKey_cparams(VbCommonParams *cparams,
				     GoogleBinaryBlockHeader *gbb,
				     uint32_t offset, VbPublicKey **keyp)
{
	VbPublicKey key;
	VbError_t ret;
	uint32_t size;

	ret = VbRegionReadData_cparams(cparams, offset, sizeof(VbPublicKey),
				       &key);
	if (ret)
		return ret;

	size = sizeof(key) + key.key_offset + key.key_size;
	*keyp = VbExMalloc(size);
	return VbRegionReadData_cparams(cparams, offset, size, *keyp);
}

VbError_t VbRegionReadGbbRootKey_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb,
				  VbPublicKey **keyp)
{
	return VbRegionReadGbbKey_cparams(cparams, gbb, gbb->rootkey_offset, keyp);
}

VbError_t VbRegionReadGbbRecoveryKey_cparams(VbCommonParams *cparams,
				      GoogleBinaryBlockHeader *gbb,
				      VbPublicKey **keyp)
{
	return VbRegionReadGbbKey_cparams(cparams, gbb, gbb->recovery_key_offset,
				   keyp);
}
