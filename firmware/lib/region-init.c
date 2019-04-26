/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * High-level firmware API for loading and verifying rewritable firmware.
 * (Firmware portion)
 */

#include "2common.h"
#include "2misc.h"
#include "gbb_access.h"
#include "vboot_struct.h"

static int vb2_read_gbb_key(struct vb2_context *ctx, uint32_t offset,
			    uint32_t size, VbPublicKey **keyp,
			    struct vb2_workbuf *wb, int save_on_wb)
{
	struct vb2_workbuf wblocal = *wb;
	int rv;

	*keyp = vb2_workbuf_alloc(&wblocal, size);
	if (!*keyp)
		return VB2_ERROR_GBB_WORKBUF;

	rv = vb2ex_read_resource(ctx, VB2_RES_GBB, offset, *keyp, size);
	if (!rv)
		return rv;

	/* Deal with a zero-size key (used in testing). */
	size = (*keyp)->key_offset + (*keyp)->key_size;
	if (size < sizeof(**keyp))
		size = sizeof(**keyp);

	if (save_on_wb)
		*wb = wblocal;
	return VB2_SUCCESS;
}

int vb2api_gbb_read_root_key(struct vb2_context *ctx, VbPublicKey **keyp,
			     struct vb2_workbuf *wb, int save_on_wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	return vb2_read_gbb_key(ctx, gbb->rootkey_offset, gbb->rootkey_size,
				keyp, wb, save_on_wb);
}

int vb2api_gbb_read_recovery_key(struct vb2_context *ctx, VbPublicKey **keyp,
				 struct vb2_workbuf *wb, int save_on_wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);

	return vb2_read_gbb_key(ctx, gbb->recovery_key_offset,
				gbb->recovery_key_size, keyp, wb, save_on_wb);
}

int vb2api_gbb_read_hwid(struct vb2_context *ctx, char **hwid,
			 struct vb2_workbuf *wb, int save_on_wb)
{
	struct vb2_gbb_header *gbb = vb2_get_gbb(ctx);
	struct vb2_workbuf wblocal = *wb;
	int rv;

	if (gbb->hwid_size == 0) {
		VB2_DEBUG("%s: invalid HWID size %d\n", __func__,
			  gbb->hwid_size);
		return VB2_ERROR_GBB_INVALID;
	}

	*hwid = vb2_workbuf_alloc(&wblocal, gbb->hwid_size);
	if (!*hwid)
		return VB2_ERROR_GBB_WORKBUF;


	rv = vb2ex_read_resource(ctx, VB2_RES_GBB, gbb->hwid_offset, *hwid,
				 gbb->hwid_size);
	if (rv)
		return rv;

	if (save_on_wb)
		*wb = wblocal;
	return VB2_SUCCESS;
}
