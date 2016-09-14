/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string.h>

#include "2sha.h"
#include "bdb.h"

#define PACK32(str, x)						\
	{							\
		*(x) =   ((uint32_t) *((str) + 3)      )	\
			| ((uint32_t) *((str) + 2) <<  8)       \
			| ((uint32_t) *((str) + 1) << 16)       \
			| ((uint32_t) *((str) + 0) << 24);      \
	}

int bdb_sha256(void *digest, const void *buf, size_t size)
{
	struct vb2_sha256_context ctx;

	vb2_sha256_init(&ctx);
	vb2_sha256_update(&ctx, buf, size);
	vb2_sha256_finalize(&ctx, digest);

	return BDB_SUCCESS;
}

void sha256_extendish(const uint8_t *from, const uint8_t *by, uint8_t *to)
{
	struct vb2_sha256_context dc;
	int i;

	vb2_sha256_init(&dc);
	for (i = 0; i < 8; i++) {
		 PACK32(from, &dc.h[i]);
		 from += 4;
	}
	vb2_sha256_update(&dc, by, VB2_SHA256_BLOCK_SIZE);
	vb2_sha256_finalize(&dc, to);
}
