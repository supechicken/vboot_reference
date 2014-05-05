/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Routines not used by firmware, but needed by tests for those */

#ifndef VBOOT_REFERENCE_2NOT_FW_H_
#define VBOOT_REFERENCE_2NOT_FW_H_
#include <stdint.h>
#include "2rsa.h"
#include "cryptolib.h"

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
			  const RSAPublicKey *key);

#endif  /* VBOOT_REFERENCE_2NOT_FW_H_ */
