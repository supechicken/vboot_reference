/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2return_codes.h"
#include "cbfstool.h"
#include "common/tests.h"

#define BRYA_IMAGE "tests/cbfstool_data/image-brya0.bin"

static void cbfstool_get_config_value_tests(void)
{
	char *value;
	vb2_error_t rv;

	/* CHROMEOS: bool */
	value = NULL;
	rv = cbfstool_get_config_value(BRYA_IMAGE, NULL,
				       "CONFIG_CHROMEOS", &value);
	TEST_SUCC(rv, "get CHROMEOS value");
	TEST_PTR_NEQ(value, NULL, "value not null");
	TEST_EQ(strcmp(value, "y"), 0, "value is y");

	/* MAINBOARD_PART_NUMBER: str */
	value = NULL;
	rv = cbfstool_get_config_value(BRYA_IMAGE, NULL,
				       "CONFIG_MAINBOARD_PART_NUMBER", &value);
	TEST_SUCC(rv, "get MAINBOARD_PART_NUMBER value");
	TEST_PTR_NEQ(value, NULL, "value not null");
	TEST_EQ(strcmp(value, "\"Brya\""), 0, "value is \"Brya\"");
}

int main(int argc, char *argv[])
{
	cbfstool_get_config_value_tests();

	return gTestSuccess ? 0 : 255;
}
