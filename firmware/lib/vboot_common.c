/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 * (Firmware portion)
 */

#include "2common.h"
#include "2misc.h"
#include "2rsa.h"
#include "2sha.h"
#include "2sysincludes.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "host_common.h"

vb2_error_t VerifyVmlinuzInsideKBlob(uint64_t kblob, uint64_t kblob_size,
				     uint64_t header, uint64_t header_size)
{
	uint64_t end = header-kblob;
	if (end > kblob_size)
		return VBOOT_PREAMBLE_INVALID;
	if (UINT64_MAX - end < header_size)
		return VBOOT_PREAMBLE_INVALID;
	if (end + header_size > kblob_size)
		return VBOOT_PREAMBLE_INVALID;

	return VB2_SUCCESS;
}
