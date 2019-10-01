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

vb2_error_t vb2api_secdata_kernel_check(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel *sec =
		(struct vb2_secdata_kernel *)ctx->secdata_kernel;

	/* Verify CRC */
	if (sec->crc8 != vb2_crc8(sec, offsetof(struct vb2_secdata_kernel,
						crc8))) {
		VB2_DEBUG("secdata_kernel: bad CRC\n");
		return VB2_ERROR_SECDATA_KERNEL_CRC;
	}

	/* Verify version */
	if (sec->struct_version < VB2_SECDATA_KERNEL_VERSION) {
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

uint32_t vb2api_secdata_kernel_create(struct vb2_context *ctx)
{
	struct vb2_secdata_kernel *sec =
		(struct vb2_secdata_kernel *)ctx->secdata_kernel;

	/* Clear the entire struct */
	memset(sec, 0, sizeof(*sec));

	/* Set to current version */
	sec->struct_version = VB2_SECDATA_KERNEL_VERSION;

	/* Set UID */
	sec->uid = VB2_SECDATA_KERNEL_UID;

	/* Calculate initial CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_kernel, crc8));

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
	struct vb2_secdata_kernel *sec =
		(struct vb2_secdata_kernel *)ctx->secdata_kernel;
	int ret = 0;
	int use_default = 0;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) {
			VB2_DEBUG("secdata_kernel broken, get default\n");
			use_default = 1;
		} else {
			VB2_DIE("Must init secdata_kernel before getting\n");
		}
	}

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		ret = use_default ? 0 : sec->kernel_versions;
		break;

	default:
		VB2_DIE("Invalid param\n");
	}

	return ret;
}

void vb2_secdata_kernel_set(struct vb2_context *ctx,
			    enum vb2_secdata_kernel_param param,
			    uint32_t value)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_secdata_kernel *sec =
		(struct vb2_secdata_kernel *)ctx->secdata_kernel;
	uint32_t now;
	int ignore_set = 0;

	if (!(sd->status & VB2_SD_STATUS_SECDATA_KERNEL_INIT)) {
		if (ctx->flags & VB2_CONTEXT_RECOVERY_MODE) {
			VB2_DEBUG("secdata_kernel broken, ignore set\n");
			ignore_set = 1;
		} else {
			VB2_DIE("Must init secdata_kernel before setting\n");
		}
	}

	/* If not changing the value, don't regenerate the CRC */
	now = vb2_secdata_kernel_get(ctx, param);
	if (now == value)
		return;

	/* Param validity gets checked above in the get call */
	if (ignore_set)
		return;

	switch (param) {
	case VB2_SECDATA_KERNEL_VERSIONS:
		VB2_DEBUG("secdata_kernel versions updated from 0x%x to 0x%x\n",
			  sec->kernel_versions, value);
		sec->kernel_versions = value;
		break;

	default:
		VB2_DIE("Invalid param\n");
	}


	/* Regenerate CRC */
	sec->crc8 = vb2_crc8(sec, offsetof(struct vb2_secdata_kernel, crc8));
	ctx->flags |= VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
}
