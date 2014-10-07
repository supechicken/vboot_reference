/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Stub implementations of stream APIs.
 */

#include <stdio.h>
#include <stdint.h>

#define _STUB_IMPLEMENTATION_

#include "vboot_api.h"

#define LBA_BYTES 512

/* Internal struct to simulate a stream for sector-based disks */
struct vbexstream {
	VbExDiskHandle_t handle;
	uint64_t sector;
	uint64_t sectors_left;
};

VbError_t VbExStreamOpen(VbExDiskHandle_t handle, uint64_t lba_start,
			 uint64_t lba_count, VbStream_t *stream)
{
	struct vbexstream *s;

	s = VbExMalloc(sizeof(*s));
	s->handle = handle;
	s->sector = lba_start;
	s->sectors_left = lba_count;

	*stream = (void *)s;

	return VBERROR_SUCCESS;
}

VbError_t VbExStreamRead(VbStream_t stream, uint32_t bytes, void *buffer)
{
	struct vbexstream *s = (struct vbexstream *)stream;
	uint64_t sectors;
	VbError_t rv;

	if (!s)
		return VBERROR_UNKNOWN;

	/* For now, require reads to be a multiple of the LBA size */
	if (bytes % LBA_BYTES) {
		printf("VbExStreamRead(%d) not LBA multiple\n", bytes);
		return VBERROR_UNKNOWN;
	}

	/* Fail on overflow */
	sectors = bytes / LBA_BYTES;
	if (sectors > s->sectors_left)
		return VBERROR_UNKNOWN;

	rv = VbExDiskRead(s->handle, s->sector, sectors, buffer);
	if (rv != VBERROR_SUCCESS)
		return rv;

	s->sector += sectors;
	s->sectors_left -= sectors;

	return VBERROR_SUCCESS;
}

VbError_t VbExStreamClose(VbStream_t stream)
{
	struct vbexstream *s = (struct vbexstream *)stream;

	/* Allow freeing a null pointer */
	if (!s)
		return VBERROR_SUCCESS;

	VbExFree(s);
	return VBERROR_SUCCESS;
}
