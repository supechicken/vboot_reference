/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Secure storage APIs - kernel version space
 */

#include "2common.h"
#include "2crc8.h"
#include "2misc.h"
#include "2secdata.h"
#include "2secdata_struct.h"
#include "2sysincludes.h"

static int is_v0(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v10 *sec = (void *)ctx->secdata_kernel;
	return MAJOR_VER(sec->struct_version) == 0;
}

uint8_t vb2_secdata_kernel_calc_crc8(struct vb2_context *ctx)
{
	size_t offset, size;

	if (is_v0(ctx)) {
		offset = 0;
		size = offsetof(struct vb2_secdata_kernel_v02, crc8);
	} else {
		struct vb2_secdata_kernel_v10 *sec
			= (void *)ctx->secdata_kernel;
		offset = offsetof(struct vb2_secdata_kernel_v10, reserved0);
		size = sec->struct_size - offset;
	}

	return vb2_crc8(ctx->secdata_kernel + offset, size);
}

static vb2_error_t secdata_kernel_check_v02(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v02 *sec = (void *)ctx->secdata_kernel;
	uint8_t ver = sec->struct_version;

	if (MINOR_VER(ver) != MINOR_VER(VB2_SECDATA_KERNEL_VERSION_V02)) {
		VB2_DEBUG("secdata_kernel: bad struct_version (%d.%d)\n",
			  MAJOR_VER(ver), MINOR_VER(ver));
		return VB2_ERROR_SECDATA_KERNEL_VERSION;
	}

	/* Verify CRC */
	if (sec->crc8 != vb2_secdata_kernel_calc_crc8(ctx)) {
		VB2_DEBUG("secdata_kernel: bad CRC\n");
		return VB2_ERROR_SECDATA_KERNEL_CRC;
	}

	/* Verify UID */
	if (sec->uid != VB2_SECDATA_KERNEL_UID) {
		VB2_DEBUG("secdata_kernel: bad UID\n");
		return VB2_ERROR_SECDATA_KERNEL_UID;
	}

	return VB2_SUCCESS;
}

static vb2_error_t secdata_kernel_check_v10(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v10 *sec = (void *)ctx->secdata_kernel;
	uint8_t ver = sec->struct_version;

	if (MAJOR_VER(ver) != MAJOR_VER(VB2_SECDATA_KERNEL_VERSION_V10)) {
		VB2_DEBUG("secdata_kernel: bad struct_version (%d.%d)\n",
			  MAJOR_VER(ver), MINOR_VER(ver));
		return VB2_ERROR_SECDATA_KERNEL_VERSION;
	}

	if (sec->struct_size < VB2_SECDATA_KERNEL_SIZE_V10 ||
			VB2_SECDATA_KERNEL_MAX_SIZE < sec->struct_size) {
		VB2_DEBUG("secdata_kernel: bad struct_size (%d)\n",
			  sec->struct_size);
		return VB2_ERROR_SECDATA_KERNEL_STRUCT_SIZE;
	}

	/* Verify CRC */
	if (sec->crc8 != vb2_secdata_kernel_calc_crc8(ctx)) {
		VB2_DEBUG("secdata_kernel: bad CRC\n");
		return VB2_ERROR_SECDATA_KERNEL_CRC;
	}

	return VB2_SUCCESS;
}

vb2_error_t vb2api_secdata_kernel_check(struct vb2_context *ctx)
{
	if (is_v0(ctx))
		return secdata_kernel_check_v02(ctx);
	else
		return secdata_kernel_check_v10(ctx);
}

uint32_t vb2api_secdata_kernel_create(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel_v10 *sec = (void *)ctx->secdata_kernel;

	/* Populate the struct */
	memset(sec, 0, sizeof(*sec));
	sec->struct_version = VB2_SECDATA_KERNEL_VERSION_V10;
	sec->struct_size = VB2_SECDATA_KERNEL_SIZE_V10;
	sec->crc8 = vb2_secdata_kernel_calc_crc8(ctx);

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

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		msg = "get before init";
		goto fail;
	}

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		if (is_v0(ctx)) {
			struct vb2_secdata_kernel_v02 *sec =
					(void *)ctx->secdata_kernel;
			return sec->kernel_versions;
		} else {
			struct vb2_secdata_kernel_v10 *sec =
					(void *)ctx->secdata_kernel;
			return sec->kernel_versions;
		}
	default:
		msg = "invalid param";
	}

 fail:
	VB2_REC_OR_DIE(ctx, "%s\n", msg);
	return 0;
}

void vb2_secdata_kernel_set(struct vb2_context *ctx,
			    enum vb2_secdata_kernel_param param,
			    uint32_t value)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	const char *msg;
	struct vb2_secdata_kernel_v02 *v0 = (void *)ctx->secdata_kernel;
	struct vb2_secdata_kernel_v10 *v1 = (void *)ctx->secdata_kernel;
	uint32_t *ptr;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		msg = "set before init";
		goto fail;
	}

	/* If not changing the value, just return early */
	if (value == vb2_secdata_kernel_get(ctx, param))
		return;

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		ptr = is_v0(ctx) ? &v0->kernel_versions : &v1->kernel_versions;
		VB2_DEBUG("secdata_kernel versions updated from %#x to %#x\n",
			  *ptr, value);
		break;

	default:
		msg = "invalid param";
		goto fail;
	}

	*ptr = value;

	if (is_v0(ctx))
		v0->crc8 = vb2_secdata_kernel_calc_crc8(ctx);
	else
		v1->crc8 = vb2_secdata_kernel_calc_crc8(ctx);

	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	return;

 fail:
	VB2_REC_OR_DIE(ctx, "%s\n", msg);
}

const uint8_t *vb2_secdata_kernel_get_ec_hash(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_kernel_v10 *sec = (void *)ctx->secdata_kernel;

	if (is_v0(ctx)) {
		VB2_DEBUG("ERROR [invalid version of kernel secdata]");
		return NULL;
	}
	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		VB2_DEBUG("ERROR [get kernel secdata before init]");
		return NULL;
	}

	return sec->ec_hash;
}

vb2_error_t vb2_secdata_kernel_set_ec_hash(struct vb2_context *ctx,
					   const uint8_t *in, size_t in_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_kernel_v10 *sec = (void *)ctx->secdata_kernel;

	if (is_v0(ctx)) {
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
	sec->crc8 = vb2_secdata_kernel_calc_crc8(ctx);

	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;

	return VB2_SUCCESS;
}
