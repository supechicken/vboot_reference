/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Implementation of RSA signature verification which uses a pre-processed key
 * for computation. The code extends Android's RSA verification code to support
 * multiple RSA key lengths and hash digest algorithms.
 */

#include "2sysincludes.h"
#include "2common.h"

int vb2_align(uint8_t **ptr, uint32_t *size, uint32_t align, uint32_t want_size)
{
	size_t p = (size_t)*ptr;
	size_t offs = p & (align - 1);

	if (offs) {
		offs = align - offs;

		if (*size < offs)
			return VB2_ERROR_WORKBUF_TOO_SMALL;

		*ptr += offs;
		*size -= offs;
	}

	if (*size < want_size)
		return VB2_ERROR_WORKBUF_TOO_SMALL;

	return VB2_SUCCESS;
}
