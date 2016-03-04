/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 */

#ifndef VBOOT_REFERENCE_TEST_COMMON_H_
#define VBOOT_REFERENCE_TEST_COMMON_H_

#include "call_trace.h"

extern int gTestSuccess;

/* Return 1 if result is 0 (VB_ERROR_SUCCESS / VB2_SUCCESS), else return 0.
 * Also update the global gTestSuccess flag if test fails. */
int test_equal(int result, const char* testname, int expect);

#define TEST_EQ(function_call, expect, testname) do {			\
	struct call_trace ct;						\
	vb_init_call_trace(&ct);					\
	if (!test_equal(function_call, testname, expect))		\
		vb_dump_call_trace();					\
} while(0)

/* Return 0 if result is equal to not_expected_result, else return 1.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_NEQ(int result, int not_expected_result, const char* testname);

/* Return 1 if result pointer is equal to expected_result pointer,
 * else return 0.  Does not check pointer contents, only the pointer
 * itself.  Also update the global gTestSuccess flag if test fails. */
int TEST_PTR_EQ(const void* result, const void* expected_result,
                const char* testname);

/* Return 1 if result pointer is not equal to expected_result pointer,
 * else return 0.  Does not check pointer contents, only the pointer
 * itself.  Also update the global gTestSuccess flag if test fails. */
int TEST_PTR_NEQ(const void* result, const void* expected_result,
                 const char* testname);

/* Return 1 if result string is equal to expected_result string,
 * else return 0.  Also update the global gTestSuccess flag if test fails. */
int TEST_STR_EQ(const char* result, const char* expected_result,
                const char* testname);

/* Return 1 if the result is true, else return 0.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_TRUE(int result, const char* testname);

/* Return 1 if the result is false, else return 0.
 * Also update the global gTestSuccess flag if test fails. */
int TEST_FALSE(int result, const char* testname);

#define TEST_SUCC(function_call, testname) do {				\
	struct call_trace ct;						\
	vb_init_call_trace(&ct);					\
	if (!test_equal(function_call, testname, 0))			\
		vb_dump_call_trace();					\
} while(0)

/* ANSI Color coding sequences.
 *
 * Don't use \e as MSC does not recognize it as a valid escape sequence.
 */
#define COL_GREEN "\x1b[1;32m"
#define COL_YELLOW "\x1b[1;33m"
#define COL_RED "\x1b[0;31m"
#define COL_STOP "\x1b[m"

/* Check that all memory allocations were freed */
int vboot_api_stub_check_memory(void);

/* Abort if asprintf fails. */
#define xasprintf(...) \
	do { \
		if (asprintf(__VA_ARGS__) < 0) \
			abort(); \
	} while (0)

#endif  /* VBOOT_REFERENCE_TEST_COMMON_H_ */
