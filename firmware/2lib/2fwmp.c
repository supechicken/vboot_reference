/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware management parameters (FWMP) APIs
 */

#include "2crc8.h"
#include "2common.h"
#include "2secdata.h"

vb2_error_t vb2api_fwmp_check(struct vb2_context *ctx)
{
	struct vb2_fwmp *fwmp = (struct vb2_fwmp *)ctx->fwmp;

	/* Verify CRC (data starts at struct_version) */
	int version_offset = offsetof(struct vb2_fwmp, struct_version);
	if (fwmp->crc8 != vb2_crc8((void *)fwmp + version_offset,
				   fwmp->struct_size - version_offset)) {
		VB2_DEBUG("FWMP: bad CRC\n");
		return VB2_ERROR_FWMP_CRC;
	}

	/* Verify major version is compatible */
	if ((fwmp->struct_version >> 4) != (VB2_FWMP_VERSION >> 4)) {
		VB2_DEBUG("FWMP: major version incompatible\n");
		return VB2_ERROR_FWMP_VERSION;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_fwmp_create(struct vb2_context *ctx)
{
	return VB2_SUCCESS;
}

vb2_error_t vb2_fwmp_init(struct vb2_context *ctx)
{
	return vb2api_fwmp_check(ctx);
}

vb2_error_t vb2_fwmp_get_flag(struct vb2_context *ctx, enum vb2_fwmp_flags flag,
			      int *dest)
{
	return VB2_SUCCESS;
}

vb2_error_t vb2_fwmp_set_flag(struct vb2_context *ctx, enum vb2_fwmp_flags flag,
			      int value)
{
	return VB2_SUCCESS;
}
