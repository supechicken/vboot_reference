/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stub implementations of firmware-provided API functions.
 */

#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/time.h>

#include "2common.h"
#include "vboot_api.h"

uint32_t vb2ex_mtime(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

vb2_error_t vb2ex_commit_data(struct vb2_context *ctx)
{
	ctx->flags &= ~VB2_CONTEXT_SECDATA_FIRMWARE_CHANGED;
	ctx->flags &= ~VB2_CONTEXT_SECDATA_KERNEL_CHANGED;
	ctx->flags &= ~VB2_CONTEXT_NVDATA_CHANGED;
	return VB2_SUCCESS;
}
