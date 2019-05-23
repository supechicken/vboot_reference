/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * GBB accessor functions.
 */

#include "2common.h"
#include "2misc.h"
#include "vb2_struct.h"

static int vb2_read_gbb_key(struct vb2_context *ctx, uint32_t offset,
			    uint32_t size, struct vb2_packed_key **keyp,
			    struct vb2_workbuf *wb)
{
	int rv;

	*keyp = vb2_workbuf_alloc(wb, size);
	if (!*keyp)
		return VB2_ERROR_GBB_WORKBUF;

	rv = vb2ex_read_resource(ctx, VB2_RES_GBB, offset, *keyp, size);
	if (!rv)
		return rv;

	/* Deal with a zero-size key (used in testing). */
	size = (*keyp)->key_offset + (*keyp)->key_size;
	if (size < sizeof(**keyp))
		size = sizeof(**keyp);

	return VB2_SUCCESS;
}

int vb2_gbb_read_root_key(struct vb2_context *ctx, struct vb2_packed_key **keyp,
			  struct vb2_workbuf *wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	return vb2_read_gbb_key(ctx, gbb->rootkey_offset, gbb->rootkey_size,
				keyp, wb);
}

int vb2_gbb_read_recovery_key(struct vb2_context *ctx,
			      struct vb2_packed_key **keyp,
			      struct vb2_workbuf *wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	return vb2_read_gbb_key(ctx, gbb->recovery_key_offset,
				gbb->recovery_key_size, keyp, wb);
}

int vb2_gbb_read_hwid(struct vb2_context *ctx, char **hwid, uint32_t *size,
		      struct vb2_workbuf *wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	uint32_t real_size;
	int ret;

	if (gbb->hwid_size == 0) {
		VB2_DEBUG("%s: invalid HWID size %d\n", __func__,
			  gbb->hwid_size);
		return VB2_ERROR_GBB_INVALID;
	}

	*hwid = vb2_workbuf_alloc(wb, gbb->hwid_size);
	if (!*hwid) {
		VB2_DEBUG("%s: allocation failure\n", __func__);
		return VB2_ERROR_GBB_WORKBUF;
	}

	ret = vb2ex_read_resource(ctx, VB2_RES_GBB, gbb->hwid_offset,
				  *hwid, gbb->hwid_size);
	if (ret) {
		VB2_DEBUG("%s: read resource failure: %d\n", __func__, ret);
		return ret;
	}

	/* HWID in GBB is padded.  Get the real size, and realloc
	   to save space on the workbuf. */
	real_size = strlen(*hwid) + 1;
	*hwid = vb2_workbuf_realloc(wb, gbb->hwid_size, real_size);

	if (size)
		*size = real_size;
	return VB2_SUCCESS;
}

int vb2api_gbb_read_hwid(struct vb2_context *ctx, char **hwid, uint32_t *size)
{
	struct vb2_workbuf wb;
	vb2_workbuf_from_ctx(ctx, &wb);
	int ret = vb2_gbb_read_hwid(ctx, hwid, size, &wb);
	ctx->workbuf_used = vb2_offset_of(ctx->workbuf, wb.buf);
	return ret;
}
