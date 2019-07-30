/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Firmware management parameters (FWMP) APIs
 */

#include "2crc8.h"
#include "2common.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"

/* Calculate CRC hash from struct_version onward */
static uint32_t vb2_secdata_fwmp_crc(struct vb2_context *ctx)
{
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;
	int version_offset = offsetof(struct vb2_secdata_fwmp, struct_version);
	return vb2_crc8((void *)sec + version_offset,
			sec->struct_size - version_offset);
}

vb2_error_t vb2api_secdata_fwmp_check(struct vb2_context *ctx, uint32_t *size)
{
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;

	/* Verify that struct_size is reasonable */
	if (sec->struct_size < sizeof(struct vb2_secdata_fwmp) ||
	    sec->struct_size > VB2_SECDATA_FWMP_MAX_SIZE) {
		VB2_DEBUG("FWMP: invalid size: %d\n", sec->struct_size);
		return VB2_ERROR_SECDATA_FWMP_SIZE;
	}

	/* Verify that we have read full structure */
	if (*size < sec->struct_size) {
		VB2_DEBUG("FWMP: missing %d bytes\n", sec->struct_size - *size);
		*size = sec->struct_size;
		return VB2_ERROR_SECDATA_FWMP_INCOMPLETE;
	}
	*size = sec->struct_size;

	/* Verify CRC */
	if (sec->crc8 != vb2_secdata_fwmp_crc(ctx)) {
		VB2_DEBUG("FWMP: bad CRC\n");
		return VB2_ERROR_SECDATA_FWMP_CRC;
	}

	/* Verify major version is compatible */
	if ((sec->struct_version >> 4) != (VB2_SECDATA_FWMP_VERSION >> 4)) {
		VB2_DEBUG("FWMP: major version incompatible\n");
		return VB2_ERROR_SECDATA_FWMP_VERSION;
	}

	/*
	 * If this were a 1.1+ reader and the source was a 1.0 struct,
	 * we would need to take care of initializing the extra fields
	 * added in 1.1+.  But that's not an issue yet.
	 */
	return VB2_SUCCESS;
}

uint32_t vb2api_secdata_fwmp_create(struct vb2_context *ctx)
{
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;

	/* Clear the entire struct */
	memset(sec, 0, sizeof(*sec));

	/* Set to current version */
	sec->struct_version = VB2_SECDATA_FWMP_VERSION;

	/* Set size of struct */
	sec->struct_size = sizeof(struct vb2_secdata_fwmp);

	/* Calculate initial CRC */
	sec->crc8 = vb2_secdata_fwmp_crc(ctx);

	/* Mark as changed */
	ctx->flags |= VB2_CONTEXT_SECDATA_FWMP_CHANGED;

	return sec->struct_size;
}

vb2_error_t vb2_secdata_fwmp_init(struct vb2_context *ctx, uint32_t *size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	vb2_error_t rv;

	rv = vb2api_secdata_fwmp_check(ctx, size);
	if (rv)
		return rv;

	/* Mark as initialized */
	sd->status |= VB2_SD_STATUS_SECDATA_FWMP_INIT;

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_fwmp_get_flag(struct vb2_context *ctx,
				      enum vb2_secdata_fwmp_flags flag,
				      int *dest)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FWMP_INIT))
		return VB2_ERROR_SECDATA_FWMP_GET_UNINITIALIZED;

	*dest = !!(sec->flags & flag);

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_fwmp_set_flag(struct vb2_context *ctx,
				      enum vb2_secdata_fwmp_flags flag,
				      int value)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;
	int now;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FWMP_INIT))
		return VB2_ERROR_SECDATA_FWMP_SET_UNINITIALIZED;

	/* If not changing the value, don't regenerate the CRC */
	if (vb2_secdata_fwmp_get_flag(ctx, flag, &now) == VB2_SUCCESS &&
	    now == value)
		return VB2_SUCCESS;

	if (value)
		sec->flags |= flag;
	else
		sec->flags &= ~flag;

	/* Regenerate CRC */
	sec->crc8 = vb2_secdata_fwmp_crc(ctx);
	ctx->flags |= VB2_CONTEXT_SECDATA_FWMP_CHANGED;
	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_fwmp_get_dev_key_hash(struct vb2_context *ctx,
					      uint8_t **dev_key_hash)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)&ctx->secdata_fwmp;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FWMP_INIT))
		return VB2_ERROR_UNKNOWN;  // VB2_ERROR_SECDATA_FWMP_GET_DEV_KEY_HASH_UNINITIALIZED;

	*dev_key_hash = sec->dev_key_hash;

	return VB2_SUCCESS;
}
