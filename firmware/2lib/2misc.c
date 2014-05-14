/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Misc functions which need access to vb2_context but are not public APIs
 */

#include "2sysincludes.h"
#include "2api.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "2sha.h"
#include "2rsa.h"

void vb2_init_context(struct vb2_context *ctx)
{
	struct vb2_shared_data *sd = (struct vb2_shared_data *)ctx->workbuf;

	/* Don't do anything if the context has already been initialized */
	if (ctx->workbuf_used)
		return;

	/*
	 * Workbuf had better be big enough for our shared data struct and
	 * aligned.  Not much we can do if it isn't; we'll die before we can
	 * store a recovery reason.
	 */
	assert(ctx->workbuf_size >= sizeof(*sd));
	assert(is_aligned_32(ctx->workbuf));

	/* Initialize the shared data at the start of the work buffer */
	memset(sd, 0, sizeof(*sd));
	ctx->workbuf_used = sizeof(*sd);
}
