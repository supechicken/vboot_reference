/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Secure storage APIs - kernel version space
 */

#include <stdbool.h>

#include "2common.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"
#include "2sysincludes.h"

#define IS_V2(ctx) \
	(((struct vb2_secdata_kernel_v3 *)( \
		(ctx)->secdata_kernel))->struct_version == 2)

static uint8_t get_crc8(struct vb2_context *ctx)
{
	uint8_t crc;

	if (IS_V2(ctx)) {
		struct vb2_secdata_kernel *sec =
			(struct vb2_secdata_kernel *)ctx->secdata_kernel;
		crc = vb2_crc8(sec, offsetof(struct vb2_secdata_kernel, crc8));
	} else {
		struct vb2_secdata_kernel_v3 *sec =
			(struct vb2_secdata_kernel_v3 *)ctx->secdata_kernel;
		int offset = offsetof(struct vb2_secdata_kernel_v3, crc8)
				+ sizeof(sec->crc8);
		crc = vb2_crc8((uint8_t *)sec + offset, sizeof(*sec) - offset);
	}

	return crc;
}

static vb2_error_t secdata_kernel_check_v2(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel *sec =
		(struct vb2_secdata_kernel *)ctx->secdata_kernel;

	/* Verify CRC */
	if (sec->crc8 != get_crc8(ctx)) {
		VB2_DEBUG("secdata_kernel: bad CRC\n");
		return VB2_ERROR_SECDATA_KERNEL_CRC;
	}

	/* Verify version */
	if (sec->struct_version < 2) {
		VB2_DEBUG("secdata_firmware: version incompatible\n");
		return VB2_ERROR_SECDATA_KERNEL_VERSION;
	}

	/* Verify UID */
	if (sec->uid != VB2_SECDATA_KERNEL_UID) {
		VB2_DEBUG("secdata_kernel: bad UID\n");
		return VB2_ERROR_SECDATA_KERNEL_UID;
	}

	return VB2_SUCCESS;
}

static vb2_error_t secdata_kernel_check_v3(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v3 *sec =
		(struct vb2_secdata_kernel_v3 *)ctx->secdata_kernel;

	if (sec->struct_version != VB2_SECDATA_KERNEL_VERSION) {
		VB2_DEBUG("secdata_kernel: bad struct_version (%d.%d)\n",
			  sec->struct_version >> 4, sec->struct_version & 0xf);
		return VB2_ERROR_SECDATA_KERNEL_VERSION;
	}

	if (sec->struct_size != sizeof(*sec)) {
		VB2_DEBUG("secdata_kernel: bad struct_size (%d)\n",
			  sec->struct_size);
		return VB2_ERROR_SECDATA_KERNEL_STRUCT_SIZE;
	}

	/* Verify CRC */
	if (sec->crc8 != get_crc8(ctx)) {
		VB2_DEBUG("secdata_kernel: bad CRC\n");
		return VB2_ERROR_SECDATA_KERNEL_CRC;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_secdata_kernel_check(struct vb2_context *ctx)
{
	if (IS_V2(ctx))
		return secdata_kernel_check_v2(ctx);
	else
		return secdata_kernel_check_v3(ctx);
}

/*
 * This is called in factory when the firmware space doesn't exist. We assume
 * in factory, BIOS and Cr50 firmware match. That is, old BIOS should create
 * v2 (0.2) struct and new BIOS should always create v3 (1.0).
 */
uint32_t vb2api_secdata_kernel_create(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v3 *sec =
		(struct vb2_secdata_kernel_v3 *)ctx->secdata_kernel;

	/* Populate the struct */
	memset(sec, 0, sizeof(*sec));
	sec->struct_version = VB2_SECDATA_KERNEL_VERSION;
	sec->struct_size = VB2_SECDATA_KERNEL_SIZE_V3;
	sec->crc8 = get_crc8(ctx);

	/* Mark as changed */
	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;

	return sizeof(*sec);
}

vb2_error_t vb2_secdata_kernel_init(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	vb2_error_t rv;

	rv = vb2api_secdata_kernel_check(ctx);
	if (rv)
		return rv;

	/* Set status flag */
	sd->status |= VB2_SD_STATUS_SECDATA_KERNEL_INIT;

	return VB2_SUCCESS;
}

uint32_t vb2_secdata_kernel_get(struct vb2_context *ctx,
				enum vb2_secdata_kernel_param param)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	const char *msg;
	uint32_t kernel_versions;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		msg = "get before init";
		goto fail;
	}

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		if (IS_V2(ctx)) {
			struct vb2_secdata_kernel *sec =
				(struct vb2_secdata_kernel *)
				ctx->secdata_kernel;
			kernel_versions = sec->kernel_versions;
		} else {
			struct vb2_secdata_kernel_v3 *sec =
				(struct vb2_secdata_kernel_v3 *)
				ctx->secdata_kernel;
			kernel_versions = sec->kernel_versions;
		}
		return kernel_versions;
	default:
		msg = "invalid param";
	}

 fail:
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
		VB2_DIE("%s\n", msg);
	VB2_DEBUG("ERROR [%s] ignored in recovery mode\n", msg);
	return 0;
}

void vb2_secdata_kernel_set(struct vb2_context *ctx,
			    enum vb2_secdata_kernel_param param,
			    uint32_t value)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	const char *msg;
	uint32_t kernel_versions;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		msg = "set before init";
		goto fail;
	}

	/* If not changing the value, just return early */
	if (value == vb2_secdata_kernel_get(ctx, param))
		return;

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		if (IS_V2(ctx)) {
			struct vb2_secdata_kernel *sec =
				(struct vb2_secdata_kernel *)
				ctx->secdata_kernel;
			kernel_versions = sec->kernel_versions;
			sec->kernel_versions = value;
			sec->crc8 = get_crc8(ctx);
		} else {
			struct vb2_secdata_kernel_v3 *sec =
				(struct vb2_secdata_kernel_v3 *)
				ctx->secdata_kernel;
			kernel_versions = sec->kernel_versions;
			sec->kernel_versions = value;
			sec->crc8 = get_crc8(ctx);
		}
		VB2_DEBUG("secdata_kernel versions updated from %#x to %#x\n",
			  kernel_versions, value);
		break;

	default:
		msg = "invalid param";
		goto fail;
	}

	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	return;

 fail:
	if (!(ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
		VB2_DIE("%s\n", msg);
	VB2_DEBUG("ERROR [%s] ignored in recovery mode\n", msg);
}

vb2_error_t vb2_secdata_kernel_get_hash(struct vb2_context *ctx,
					const uint8_t **out, int *out_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_kernel_v3 *sec =
			(struct vb2_secdata_kernel_v3 *)ctx->secdata_kernel;

	if (IS_V2(ctx)) {
		VB2_DEBUG("ERROR [invalid version of kernel secdata]");
		return VB2_ERROR_SECDATA_KERNEL_STRUCT_VERSION;
	}
	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		VB2_DEBUG("ERROR [get kernel secdata before init]");
		return VB2_ERROR_SECDATA_KERNEL_UNINITIALIZED;
	}

	*out = sec->ec_hash;
	*out_size = sizeof(sec->ec_hash);

	return VB2_SUCCESS;
}

vb2_error_t vb2_secdata_kernel_set_hash(struct vb2_context *ctx,
					const uint8_t *in, size_t in_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_kernel_v3 *sec =
			(struct vb2_secdata_kernel_v3 *)ctx->secdata_kernel;

	if (IS_V2(ctx)) {
		VB2_DEBUG("ERROR [invalid version of kernel secdata]");
		return VB2_ERROR_SECDATA_KERNEL_STRUCT_VERSION;
	}
	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		VB2_DEBUG("ERROR [get kernel secdata before init]");
		return VB2_ERROR_SECDATA_KERNEL_UNINITIALIZED;
	}
	if (in_size != sizeof(sec->ec_hash)) {
		VB2_DEBUG("ERROR [Invalid buffer size for ec_hash]");
		return VB2_ERROR_SECDATA_KERNEL_BUFFER_SIZE;
	}
	memcpy(sec->ec_hash, in, sizeof(sec->ec_hash));
	sec->crc8 = get_crc8(ctx);

	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;

	return VB2_SUCCESS;
}
