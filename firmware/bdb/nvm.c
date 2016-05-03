/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "bdb_api.h"

static nvm_init(ctx)
{
	uint8_t *buf;

	buf = vbe_get_global_data();
	vbe_get_nvmrw(buf, size);
}

uint32_t vba_nvm_get_kernel_version(struct vba_context *ctx)
{
	if (!ctx->nvmrw) {
		nvm_init(ctx);
	}
}
