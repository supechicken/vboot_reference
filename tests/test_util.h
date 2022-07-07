/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Some helper function for tests.
 */

#include "2api.h"

void set_boot_mode(struct vb2_context *ctx, enum vb2_boot_mode boot_mode,
		   uint32_t recovery_reason);
