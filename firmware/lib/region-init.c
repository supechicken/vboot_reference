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
			     uint32_t size, struct vb2_workbuf *wb)
{
	void *data;
	int rv;

	data = vb2_workbuf_alloc(wb, size);
	if (!data)
		return VB2_ERROR_GBB_WORKBUF;

	rv = vb2ex_read_resource(ctx, VB2_RES_GBB, offset, data, size);
	return rv;
}

static int vb2_read_gbb_key(struct vb2_context *ctx, uint32_t offset,
			    uint32_t size, VbPublicKey **keyp)
{
	VbPublicKey *src, *dst;
	int rv;
	struct vb2_workbuf wb;

	vb2_workbuf_from_ctx(ctx, &wb);

	src = (VbPublicKey *)wb.buf;
	rv = vb2_load_gbb_data(ctx, offset, size, &wb);
	if (rv)
		return rv;

	/* Deal with a zero-size key (used in testing). */
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
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	void *src;
	int rv;
	struct vb2_workbuf wb;

	vb2_workbuf_from_ctx(ctx, &wb);

	if (!max_size)
		return VBERROR_INVALID_PARAMETER;
	*hwid = '\0';
	StrnAppend(hwid, "{INVALID}", max_size);
	if (!ctx)
		return VBERROR_INVALID_GBB;

	if (0 == gbb->hwid_size) {
		VB2_DEBUG("VbHWID(): invalid hwid size\n");
		return VBERROR_SUCCESS;  /* oddly enough! */
	}

	if (gbb->hwid_size > max_size) {
		VB2_DEBUG("VbDisplayDebugInfo(): invalid hwid offset/size\n");
		return VBERROR_INVALID_PARAMETER;
	}

	src = wb.buf;
	rv = vb2_load_gbb_data(ctx, gbb->hwid_offset, gbb->hwid_size, &wb);

	memcpy(hwid, src, gbb->hwid_size);
	return rv;
}

VbError_t VbGbbReadRootKey(struct vb2_context *ctx, VbPublicKey **keyp)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	int rv;

	rv = vb2_read_gbb_key(
		ctx, gbb->rootkey_offset, gbb->rootkey_size, keyp);

	return rv;
}

VbError_t VbGbbReadRecoveryKey(struct vb2_context *ctx, VbPublicKey **keyp)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	int rv;

	rv = vb2_read_gbb_key(
		ctx, gbb->recovery_key_offset, gbb->recovery_key_size, keyp);

	return rv;
}
