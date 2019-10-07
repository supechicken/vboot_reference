/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware vboot_common.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "2common.h"
#include "host_common.h"
#include "test_common.h"
#include "utility.h"

/*
 * Test struct packing for vboot_struct.h structs which are passed between
 * firmware and OS, or passed between different phases of firmware.
 */
static void StructPackingTest(void)
{
	TEST_EQ(EXPECTED_VBKERNELPREAMBLEHEADER2_2_SIZE,
		sizeof(VbKernelPreambleHeader),
		"sizeof(VbKernelPreambleHeader)");

	TEST_EQ(VB_SHARED_DATA_HEADER_SIZE_V1,
		(long)&((VbSharedDataHeader*)NULL)->recovery_reason,
		"sizeof(VbSharedDataHeader) V1");

	TEST_EQ(VB_SHARED_DATA_HEADER_SIZE_V2,
		sizeof(VbSharedDataHeader),
		"sizeof(VbSharedDataHeader) V2");
}

int main(int argc, char* argv[])
{
	StructPackingTest();

	return gTestSuccess ? 0 : 255;
}
