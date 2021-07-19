/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stub implementations of stream APIs.
 */

#include "vboot_api.h"

__attribute__((weak))
vb2_error_t VbExStreamOpen(VbExDiskHandle_t handle, uint64_t lba_start,
			   uint64_t lba_count, VbExStream_t *stream)
{
	return VB2_SUCCESS;
}

__attribute__((weak))
vb2_error_t VbExStreamRead(VbExStream_t stream, uint32_t bytes, void *buffer)
{
	return VB2_SUCCESS;
}

__attribute__((weak))
void VbExStreamClose(VbExStream_t stream)
{
}
