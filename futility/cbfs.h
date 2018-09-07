/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef VBOOT_REFERENCE_FUTILITY_CBFS_H_
#define VBOOT_REFERENCE_FUTILITY_CBFS_H_

#include <stdint.h>

/*
 * Returns 1 if the CBFS blob (by start and size) contains an entry in given
 * file_name, otherwise 0.
 */
int cbfs_has_file(const uint8_t *start, size_t size, const char *file_name);

#endif	/* VBOOT_REFERENCE_FUTILITY_CBFS_H_ */
