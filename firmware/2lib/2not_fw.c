/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Routines not needed by firmware, but needed by tests
 */

#include "2sysincludes.h"
#include "vboot_api.h"
#include "2not_fw.h"
#include "2common.h"

/**
 * Convert an old-style RSA public key struct to a new one.
 *
 * The new one does not allocate memory, so you must keep the old one around
 * until you're done with the new one.
 *
 * @param k2		Destination new key
 * @param key		Source old key
 */
void vb2_public_key_to_vb2(struct vb2_public_key *k2,
			  const RSAPublicKey *key)
{
	k2->len = key->len;
	k2->n0inv = key->n0inv;
	k2->n = key->n;
	k2->rr = key->rr;
	k2->algorithm = key->algorithm;
}

