/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
#include "futility.h"
#include "updater.h"
#include "updater_utils.h"
#include "2struct.h"
#include "common/tests.h"

#define IMAGE_TEMP "tests/futility/data_copy/image-temp.bin"
#define IMAGE_MAIN "tests/futility/data_copy/image-newer.bin"
#define IMAGE_BAD "tests/futility/data_copy/image-bad.bin"
#define IMAGE_MISSING_FMAP "tests/futility/data_copy/image-missing-fmap.bin"
#define IMAGE_MISSING_FRID "tests/futility/data_copy/image-missing-ro_frid.bin"
#define IMAGE_MISSING_FWID "tests/futility/data_copy/image-missing-rw_fwid.bin"
#define ARCHIVE "tests/futility/data_copy/images.zip"

static char __format_buf[4096];
#define format(...)                                                                            \
	({                                                                                     \
		sprintf(__format_buf, __VA_ARGS__);                                            \
		__format_buf;                                                                  \
	})

static int copy_image(const char *path)
{
	static char buf[1024];
	sprintf(buf, "cp %s %s", path, IMAGE_TEMP);
	INFO("%s", buf);
	return system(buf);
}

#define IF_FAIL(value, ...)                                                                    \
	{                                                                                      \
		if (!(value))                                                                  \
			TEST_SUCC(1, format(__VA_ARGS__));                                     \
	}

int main(int argc, char *argv[])
{

	return !gTestSuccess;
}
