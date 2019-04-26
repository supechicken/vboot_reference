/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware API for loading and verifying rewritable firmware.
 * (Firmware portion)
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2misc.h"

#include "sysincludes.h"
#include "gbb_access.h"
#include "load_kernel_fw.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_struct.h"

static int vb2_load_gbb_data(struct vb2_context *ctx, uint32_t offset,
			     uint32_t size, uint32_t *sd_offset)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_workbuf wb;
	void *data;
	int rv;

	vb2_workbuf_from_ctx(ctx, &wb);

	data = vb2_workbuf_alloc(&wb, size);
	if (!data)
		return VB2_ERROR_GBB_WORKBUF;

	rv = vb2ex_read_resource(ctx, VB2_RES_GBB, offset, data, size);
	if (rv)
		return rv;

	/* Keep on the work buffer permanently */
	*sd_offset = vb2_offset_of(sd, data);
	ctx->workbuf_used += wb.used;

	return VB2_SUCCESS;
}

static int vb2_read_gbb_key(struct vb2_context *ctx, uint32_t offset,
			    uint32_t size, uint32_t *sd_offset,
			    VbPublicKey **keyp)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	VbPublicKey *src, *dst;
	int rv;

	/* If the key is not yet on the workbuf, load it in. */
	if (*sd_offset == 0) {
		rv = vb2_load_gbb_data(ctx, offset, size, sd_offset);
		if (rv)
			return rv;
	}

	/* Deal with a zero-size key (used in testing). */
	src = (VbPublicKey *)((void *)sd + *sd_offset);
	size = src->key_offset + src->key_size;
	if (size < sizeof(*src))
		size = sizeof(*src);

	/* Allocate memory and copy the key. */
	dst = malloc(size);
	memcpy(dst, src, size);
	*keyp = dst;
	return VB2_SUCCESS;
}

VbError_t VbGbbReadHWID(struct vb2_context *ctx, char *hwid, uint32_t max_size)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	int rv;

	if (gbb == NULL)
		return VBERROR_INVALID_GBB;

	if (!max_size)
		return VBERROR_INVALID_PARAMETER;
	*hwid = '\0';
	StrnAppend(hwid, "{INVALID}", max_size);
	if (!ctx)
		return VBERROR_INVALID_GBB;

	if (0 == gbb->hwid_size) {
		VB2_DEBUG("VbHWID(): invalid hwid size\n");
		return VBERROR_SUCCESS; /* oddly enough! */
	}

	if (gbb->hwid_size > max_size) {
		VB2_DEBUG("VbDisplayDebugInfo(): invalid hwid offset/size\n");
		return VBERROR_INVALID_PARAMETER;
	}

	/* If HWID is not yet on the workbuf, load it in. */
	if (sd->gbb_hwid_offset == 0) {
		rv = vb2_load_gbb_data(ctx, gbb->hwid_offset, gbb->hwid_size,
				       &sd->gbb_hwid_offset);
		if (rv)
			return VBERROR_INVALID_GBB;
	}

	memcpy(hwid, (void *)sd + sd->gbb_hwid_offset, gbb->hwid_size);
	return VBERROR_SUCCESS;
}

VbError_t VbGbbReadRootKey(struct vb2_context *ctx, VbPublicKey **keyp)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	int rv;

	if (gbb == NULL)
		return VBERROR_INVALID_GBB;

	rv = vb2_read_gbb_key(
		ctx, gbb->rootkey_offset, gbb->rootkey_size,
		&sd->gbb_rootkey_offset, keyp);

	if (rv)
		return VBERROR_INVALID_GBB;

	return VBERROR_SUCCESS;
}

VbError_t VbGbbReadRecoveryKey(struct vb2_context *ctx, VbPublicKey **keyp)
{
	struct vb2_shared_data *sd = vb2_get_sd(ctx);
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	int rv;

	if (gbb == NULL)
		return VBERROR_INVALID_GBB;

	rv = vb2_read_gbb_key(
		ctx, gbb->recovery_key_offset, gbb->recovery_key_size,
		&sd->gbb_recovery_key_offset, keyp);

	if (rv)
		return VBERROR_INVALID_GBB;

	return VBERROR_SUCCESS;
}
