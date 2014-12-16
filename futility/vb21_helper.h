/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef VBOOT_REFERENCE_FUTILITY_VB21_HELPER_H_
#define VBOOT_REFERENCE_FUTILITY_VB21_HELPER_H_

#include "traversal.h"

enum futil_file_type futil_vb21_what_file_type_buf(uint8_t *buf,
						   uint32_t len);

int futil_cb_vb21_show_pubkey(struct futil_traverse_state_s *state);


#endif	/* VBOOT_REFERENCE_FUTILITY_VB21_HELPER_H_ */
