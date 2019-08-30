/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Signature related functions.
 */

#include "2common.h"

uint8_t *vb2_signature_data(struct vb2_signature *sig)
{
	return (uint8_t *)sig + sig->sig_offset;
}

vb2_error_t vb2_verify_signature_inside(const void *parent,
					uint32_t parent_size,
					const struct vb2_signature *sig)
{
	return vb2_verify_member_inside(parent, parent_size,
					sig, sizeof(*sig),
					sig->sig_offset, sig->sig_size);
}
