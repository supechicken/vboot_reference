/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Include the function declarations we want to test */
#include "../host/lib/include/flashrom.h"
#include "../host/lib/include/subprocess.h"
/* Include the common test framework */
#include "common/tests.h"

/*
 * Mocking infrastructure for subprocess_run().
 * We use the --wrap linker option to redirect all calls to subprocess_run()
 * to our __wrap_subprocess_run() function.
 */

/* Global variables to capture the arguments passed to the mock function. */
static const char **g_captured_argv = NULL;
static int g_mock_subprocess_return_code = 0;

/* Helper to free the memory allocated for capturing argv. */
static void free_captured_argv(void)
{
	if (!g_captured_argv)
		return;

	for (int i = 0; g_captured_argv[i] != NULL; i++)
		free((void *)g_captured_argv[i]);
	free(g_captured_argv);
	g_captured_argv = NULL;
}

/*
 * Reset the state of the mock before each test case.
 * This is crucial for test isolation.
 */
static void mock_subprocess_reset(void)
{
	free_captured_argv();
	g_mock_subprocess_return_code = 0; /* Default to success */
}

/*
 * The wrapper function for subprocess_run(). The linker will replace all
 * calls to subprocess_run() with calls to this function.
 * Its signature MUST match the real function.
 */
int __wrap_subprocess_run(const char *const argv[], struct subprocess_target *stdin_target,
			  struct subprocess_target *stdout_target,
			  struct subprocess_target *stderr_target)
{
	int argc = 0;

	/* It's a good practice to ensure the previous test cleaned up. */
	if (g_captured_argv)
		free_captured_argv();

	/* Count the number of arguments. */
	while (argv[argc] != NULL)
		argc++;

	/* Allocate memory to store a deep copy of the arguments. */
	g_captured_argv = calloc(argc + 1, sizeof(char *));
	if (!g_captured_argv) {
		fprintf(stderr, "Mock failed: calloc failed\n");
		return -1;
	}

	/* Copy each argument string. */
	for (int i = 0; i < argc; i++) {
		g_captured_argv[i] = strdup(argv[i]);
		if (!g_captured_argv[i]) {
			fprintf(stderr, "Mock failed: strdup failed\n");
			free_captured_argv();
			return -1;
		}
	}
	g_captured_argv[argc] = NULL;

	return g_mock_subprocess_return_code;
}

/*
 * Helper function to compare the captured argv with an expected list.
 */
static void assert_argv_eq(const char *const expected[])
{
	int i;
	TEST_TRUE(g_captured_argv != NULL, "subprocess_run was not called");

	for (i = 0; expected[i] != NULL; i++) {
		TEST_TRUE(g_captured_argv[i] != NULL, "Captured argv is shorter than expected");
		TEST_STR_EQ(g_captured_argv[i], expected[i], "Argument mismatch");
	}
	/* Ensure the captured array is not longer than expected. */
	TEST_TRUE(g_captured_argv[i] == NULL, "Captured argv is longer than expected");
}

/* --- Test Cases --- */

static void test_flashrom_read_single_region(void)
{
	mock_subprocess_reset();

	struct firmware_image image = {
		.programmer = "internal:host",
		.file_name = "/tmp/test.bin",
	};
	static const char *const regions[] = {"GBB"};

	int ret = flashrom_read_image(&image, regions, 1, 0);

	TEST_EQ(ret, 0, "flashrom_read_image should return success");

	static const char *const expected_argv[] = {"flashrom", "-p", "internal:host", "-r",
						    "/tmp/test.bin", "-i", "GBB", NULL};
	assert_argv_eq(expected_argv);
}

static void test_flashrom_read_multiple_regions(void)
{
	mock_subprocess_reset();

	struct firmware_image image = {
		.programmer = "internal:host",
		.file_name = "/tmp/fw.bin",
	};
	static const char *const regions[] = {"WP_RO", "EC_RW", "SI_DESC"};

	int ret = flashrom_read_image(&image, regions, 3, 0);

	TEST_EQ(ret, 0, "flashrom_read_image with multiple regions should succeed");

	static const char *const expected_argv[] = {"flashrom", "-p", "internal:host", "-r",
						    "/tmp/fw.bin", "-i", "WP_RO", "-i",
						    "EC_RW", "-i", "SI_DESC", NULL};
	assert_argv_eq(expected_argv);
}

static void test_flashrom_set_wp_enabled(void)
{
	mock_subprocess_reset();

	const char *programmer = "raiden_debug_spi:target=AP";
	uint32_t start = 0;
	uint32_t len = 4096;
	char expected_range_arg[64];
	snprintf(expected_range_arg, sizeof(expected_range_arg), "--wp-range=%u,%u", start,
		 len);

	int ret = flashrom_set_wp(programmer, true, start, len, 0);

	TEST_EQ(ret, 0, "flashrom_set_wp enable should succeed");

	static const char *const expected_argv[] = {"flashrom", "-p", programmer, "--wp-enable",
						    expected_range_arg, NULL};
	assert_argv_eq(expected_argv);
}

/* --- Test Runner --- */

int main(int argc, char *argv[])
{
	/*
	 * Call all the test cases. The common test framework will print
	 * PASS/FAIL for each TEST_... macro.
	 */
	test_flashrom_read_single_region();
	test_flashrom_read_multiple_regions();
	test_flashrom_set_wp_enabled();

	/*
	 * The gTestSuccess variable is defined in common/tests.h and is
	 * updated by the TEST_... macros.
	 */
	return gTestSuccess ? 0 : 255;
}
