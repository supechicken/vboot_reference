/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 *
 * Utilities for manipulating an FMAP based firmware image.
 */

#ifndef VBOOT_REFERENCE_FUTILITY_FIRMWARE_IMAGE_H_
#define VBOOT_REFERENCE_FUTILITY_FIRMWARE_IMAGE_H_

#include <stdio.h>

#include "fmap.h"

struct firmware_image {
	const char *programmer;
	uint32_t size;
	uint8_t *data;
	char *file_name;
	char *ro_version, *rw_version_a, *rw_version_b;
	FmapHeader *fmap_header;
};

struct firmware_section {
	uint8_t *data;
	size_t size;
};

#endif  /* VBOOT_REFERENCE_FUTILITY_FIRMWARE_IMAGE_H_ */
