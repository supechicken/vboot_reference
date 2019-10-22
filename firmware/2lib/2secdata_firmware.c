/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Secure storage APIs
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"

vb2_error_t vb2api_secdata_firmware_check(struct vb2_context *ctx)
{
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;

	/* Verify CRC */
	if (sec->crc8 != vb2_crc8(sec, offsetof(struct vb2_secdata_firmware,
						crc8))) {
		VB2_DEBUG("secdata_firmware: bad CRC\n");
		return VB2_ERROR_SECDATA_FIRMWARE_CRC;
	}

	/* Verify version */
	if (sec->struct_version < VB2_SECDATA_FIRMWARE_VERSION) {
		VB2_DEBUG("secdata_firmware: version incompatible\n");
		return VB2_ERROR_SECDATA_FIRMWARE_VERSION;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_secdata_firmware_create(struct vb2_context *ctx)
{
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;

	/* Clear the entire struct */
	memset(sec, 0, sizeof(*sec));

	/* Set to current version */
	sec->struct_version = VB2_SECDATA_FIRMWARE_VERSION;

	/* Calculate initial CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_firmware, crc8));

	/* Mark as changed */
	ctx->flags |= VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_firmware_init(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	vb2_error_t rv;

	rv = vb2api_secdata_firmware_check(ctx);
	if (rv)
		return rv;

	/* Set status flag */
	sd->status |= VB2_SD_STATUS_SECDATA_FIRMWARE_INIT;

	/* Read this now to make sure crossystem has it even in rec mode. */
	rv = vb2_secdata_firmware_get(ctx, VB2_SECDATA_FIRMWARE_VERSIONS,
				      &sd->fw_version_secdata);
	if (rv)
		return rv;

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_firmware_get(struct vb2_context *ctx,
				     enum vb2_secdata_firmware_param param,
				     void *dest)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_FIRMWARE_INIT))
		return VB2_ERROR_SECDATA_FIRMWARE_GET_UNINITIALIZED;

	switch (param) {
	case VB2_SECDATA_FIRMWARE_FLAGS:
		*(uint8_t *)dest = sec->flags;
		return VB2_SUCCESS;

	case VB2_SECDATA_FIRMWARE_VERSIONS:
		*(uint32_t *)dest = sec->fw_versions;
		return VB2_SUCCESS;

	case VB2_SECDATA_FIRMWARE_EC_HASH:
		memcpy(dest, sec->ec_hash, sizeof(sec->ec_hash));
		return VB2_SUCCESS;

	default:
		return VB2_ERROR_SECDATA_FIRMWARE_GET_PARAM;
	}
}

vb2_error_t vb2_secdata_firmware_set(struct vb2_context *ctx,
				     enum vb2_secdata_firmware_param param,
				     void *value)
{
	struct vb2_secdata_firmware *sec =
		(struct vb2_secdata_firmware *)ctx->secdata_firmware;
	uint32_t now;

	if (!(vb2_get_sd(ctx)->status & VB2_SD_STATUS_SECDATA_FIRMWARE_INIT))
		return VB2_ERROR_SECDATA_FIRMWARE_SET_UNINITIALIZED;

	switch (param) {
	case VB2_SECDATA_FIRMWARE_FLAGS:
		/* Make sure flags is in valid range */
		uint8_t flags = *(uint8_t *)value;
		if (flags > 0xff)
			return VB2_ERROR_SECDATA_FIRMWARE_SET_FLAGS;
		if (flags == sec->flags)
			return VB2_SUCCESS;
		VB2_DEBUG("secdata_firmware flags updated from 0x%x to 0x%x\n",
			  sec->flags, flags);
		sec->flags = flags;
		break;

	case VB2_SECDATA_FIRMWARE_VERSIONS:
		uint32_t versions = *(uint32_t *)value;
		if (versions == sec->fw_versions)
			return VB2_SUCCESS;
		VB2_DEBUG("secdata_firmware versions updated from "
			  "0x%x to 0x%x\n",
			  sec->fw_versions, versions);
		sec->fw_versions = versions;
		break;

	case VB2_SECDATA_FIRMWARE_EC_HASH:
		if (!memcmp(value, sec->ec_hash, sizeof(sec->ec_hash)))
			return VB2_SUCCESS;
		VB2_DEBUG("secdata->ec_hash updated from %08x... to %08x...\n",
			  *(uint32_t *)sec->ec_hash, *(uint32_t *)value);
		memcpy(sec->ec_hash, value, sizeof(sec->ec_hash));
		return VB2_SUCCESS;

	default:
		return VB2_ERROR_SECDATA_FIRMWARE_SET_PARAM;
	}

	/* Regenerate CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_firmware, crc8));
	ctx->flags |= VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	return VB2_SUCCESS;
}
