/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdbool.h>

#include "futility.h"
#include "gsc_ro.h"

/**
 * Basic validation of gscvd included in a AP firmware file.
 *
 * This is not a cryptographic verification, just a check that the structure
 * makes sense and the expected values are found in certain fields.
 *
 * @param gscvd  pointer to the gscvd header followed by the ranges
 * @param ap_firmware_file  pointer to the AP firmware file layout descriptor
 *
 * @return zero on success, -1 on failure.
 */
bool futil_valid_gscvd_header(const struct gsc_verification_data *gscvd,
			      uint32_t len)
{
	if (len < sizeof(*gscvd)) {
		ERROR("Too small gscvd size %u\n", len);
		return false;
	}

	if (gscvd->gv_magic != GSC_VD_MAGIC) {
		ERROR("Incorrect gscvd magic %x\n", gscvd->gv_magic);
		return false;
	}

	if (gscvd->size > len) {
		ERROR("Incorrect gscvd size %u\n", gscvd->size);
		return false;
	}

	if (!gscvd->range_count || (gscvd->range_count > MAX_RANGES)) {
		ERROR("Incorrect gscvd range count %d\n", gscvd->range_count);
		return false;
	}

	if (vb2_verify_signature_inside(gscvd, gscvd->size,
					&gscvd->sig_header)) {
		ERROR("Corrupted signature header in gscvd\n");
		return false;
	}

	if (vb2_verify_packed_key_inside(gscvd, gscvd->size,
					 &gscvd->root_key_header)) {
		ERROR("Corrupted root key header in gscvd\n");
		return false;
	}

	return true;
}
