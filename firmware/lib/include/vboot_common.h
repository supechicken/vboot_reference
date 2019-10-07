/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Common functions between firmware and kernel verified boot.
 */

#ifndef VBOOT_REFERENCE_VBOOT_COMMON_H_
#define VBOOT_REFERENCE_VBOOT_COMMON_H_

#include "2api.h"
#include "2struct.h"
#include "vboot_struct.h"

/**
 * Verify that the Vmlinuz Header is contained inside of the kernel blob.
 *
 * Returns VB2_SUCCESS or VBOOT_PREAMBLE_INVALID on error
 */
vb2_error_t VerifyVmlinuzInsideKBlob(uint64_t kblob, uint64_t kblob_size,
				     uint64_t header, uint64_t header_size);

#endif  /* VBOOT_REFERENCE_VBOOT_COMMON_H_ */
