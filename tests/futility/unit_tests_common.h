/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include "futility.h"
#include "updater.h"
#include "2struct.h"
#include "common/tests.h"

#define DATA_PATH "tests/futility/data_copy/"
#define IMAGE_TEMP DATA_PATH "image-temp.bin"
#define IMAGE_MAIN DATA_PATH "image.bin"
#define IMAGE_BAD DATA_PATH "image-bad.bin"
#define IMAGE_MISSING_FMAP DATA_PATH "image-missing-fmap.bin"
#define IMAGE_MISSING_FRID DATA_PATH "image-missing-ro_frid.bin"
#define IMAGE_MISSING_FWID DATA_PATH "image-missing-rw_fwid.bin"
#define ARCHIVE DATA_PATH "images.zip"
#define FILE_NONEXISTENT DATA_PATH "nonexistent"
#define FILE_READONLY DATA_PATH "read-only"

#ifndef UNIT_TESTS_NO_FORMAT
static char __format_buf[4096];
#define format(...)                                                                            \
	({                                                                                     \
		sprintf(__format_buf, __VA_ARGS__);                                            \
		__format_buf;                                                                  \
	})
#endif

#define IF_FAIL(value, ...)                                                                    \
	{                                                                                      \
		if (!(value))                                                                  \
			TEST_SUCC(1, format(__VA_ARGS__));                                     \
	}
