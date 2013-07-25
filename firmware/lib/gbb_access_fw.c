/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware API for loading and verifying rewritable firmware.
 * (Firmware portion)
 */

#include "sysincludes.h"

#include "bmpblk_header.h"
#include "gbb_access.h"
#include "gbb_header.h"
#include "load_kernel_fw.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_struct.h"

VbError_t VbGbbGetData_cparams(VbCommonParams *cparams, uint32_t offset,
			       uint32_t size, void *buf)
{
	VbError_t ret;

#ifdef VBOOT_GBB_DATA
	if (cparams->gbb_data) {
		if (offset > cparams->gbb_size ||
		    offset + size > cparams->gbb_size) {
			return VBERROR_INVALID_GBB;
		memcpy(buf, cparams->gbb_data + offset, size);
	} else
#endif
	{
		ret = VbExReadFirmwareRegion(cparams, VB_REGION_GBB,
					     offset, size, buf);
		if (ret)
			return ret;
	}

	return VBERROR_SUCCESS;
}

VbError_t VbGbbReadHeader_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb)
{
	return VbGbbGetData_cparams(cparams, 0,
				    sizeof(GoogleBinaryBlockHeader), gbb);
}

static VbError_t VbGbbGetKey_cparams(VbCommonParams *cparams,
				     GoogleBinaryBlockHeader *gbb,
				     uint32_t offset, VbPublicKey **keyp)
{
	VbPublicKey key;
	VbError_t ret;
	uint32_t size;

	ret = VbGbbGetData_cparams(cparams, offset, sizeof(VbPublicKey), &key);
	if (ret)
		return ret;

	size = sizeof(key) + key.key_offset + key.key_size;
	*keyp = VbExMalloc(size);
	return VbGbbGetData_cparams(cparams, offset, size, *keyp);
}

VbError_t VbGbbGetRootKey_cparams(VbCommonParams *cparams,
				  GoogleBinaryBlockHeader *gbb,
				  VbPublicKey **keyp)
{
	return VbGbbGetKey_cparams(cparams, gbb, gbb->rootkey_offset, keyp);
}

VbError_t VbGbbGetRecoveryKey_cparams(VbCommonParams *cparams,
				      GoogleBinaryBlockHeader *gbb,
				      VbPublicKey **keyp)
{
	return VbGbbGetKey_cparams(cparams, gbb, gbb->recovery_key_offset,
				   keyp);
}
