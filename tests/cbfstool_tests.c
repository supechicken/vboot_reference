/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2return_codes.h"
#include "cbfstool.h"
#include "common/tests.h"

#define IMAGE "tests/futility/data/bios_coachz_cbfs.bin"

static void cbfstool_get_config_bool_tests(void)
{
	bool value;
	vb2_error_t rv;

	/* File not found */
	value = false;
	rv = cbfstool_get_config_bool("no_such_file", NULL,
				      "CONFIG_CHROMEOS", &value);
	TEST_FAIL(rv, "file not found");

	/* Config not found */
	value = false;
	rv = cbfstool_get_config_bool(IMAGE, NULL,
				      "CONFIG_NOT_FOUND", &value);
	TEST_SUCC(rv, "config not found");
	TEST_FALSE(value, "  value is null");

	/* Config CHROMEOS */
	value = false;
	rv = cbfstool_get_config_bool(IMAGE, NULL,
				      "CONFIG_CHROMEOS", &value);
	TEST_SUCC(rv, "get CHROMEOS value");
	TEST_TRUE(value, "  value is y");

	/* Config CHROMEOS from FW_MAIN_A */
	value = false;
	rv = cbfstool_get_config_bool(IMAGE, "FW_MAIN_A",
				      "CONFIG_CHROMEOS", &value);
	TEST_SUCC(rv, "get CHROMEOS value from FW_MAIN_A");
	TEST_TRUE(value, "  value is y");
}

static void cbfstool_get_config_string_tests(void)
{
	char *value;
	vb2_error_t rv;

	/* Config not found */
	value = NULL;
	rv = cbfstool_get_config_string(IMAGE, NULL,
					"CONFIG_NOT_FOUND", &value);
	TEST_FAIL(rv, "config not found");

	/* Config MAINBOARD_PART_NUMBER */
	value = NULL;
	rv = cbfstool_get_config_string(IMAGE, NULL,
					"CONFIG_MAINBOARD_PART_NUMBER", &value);
	TEST_SUCC(rv, "get MAINBOARD_PART_NUMBER value");
	TEST_PTR_NEQ(value, NULL, "  value not null");
	TEST_EQ(strcmp(value, "Coachz"), 0, "  value is Coachz");
}

int main(int argc, char *argv[])
{
	cbfstool_get_config_bool_tests();
	cbfstool_get_config_string_tests();

	return gTestSuccess ? 0 : 255;
}
