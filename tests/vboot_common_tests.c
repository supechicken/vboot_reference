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
#include "vboot_common.h"

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

/* VbSharedData utility tests */
static void VbSharedDataTest(void)
{
	uint8_t buf[VB_SHARED_DATA_MIN_SIZE + 1];
	VbSharedDataHeader* d = (VbSharedDataHeader*)buf;

	TEST_NEQ(VB2_SUCCESS,
		 VbSharedDataInit(d, sizeof(VbSharedDataHeader) - 1),
		 "VbSharedDataInit too small");
	TEST_NEQ(VB2_SUCCESS,
		 VbSharedDataInit(d, VB_SHARED_DATA_MIN_SIZE - 1),
		 "VbSharedDataInit too small 2");
	TEST_NEQ(VB2_SUCCESS,
		 VbSharedDataInit(NULL, VB_SHARED_DATA_MIN_SIZE),
		 "VbSharedDataInit null");

	memset(buf, 0x68, sizeof(buf));
	TEST_EQ(VB2_SUCCESS, VbSharedDataInit(d, VB_SHARED_DATA_MIN_SIZE),
		"VbSharedDataInit");

	/* Check fields that should have been initialized */
	TEST_EQ(d->magic, VB_SHARED_DATA_MAGIC, "VbSharedDataInit magic");
	TEST_EQ(d->struct_version, VB_SHARED_DATA_VERSION,
		"VbSharedDataInit version");
	TEST_EQ(d->struct_size, sizeof(VbSharedDataHeader),
		"VbSharedDataInit struct_size");
	TEST_EQ(d->data_size, VB_SHARED_DATA_MIN_SIZE,
		"VbSharedDataInit data_size");
	TEST_EQ(d->data_used, d->struct_size, "VbSharedDataInit data_used");
	TEST_EQ(d->firmware_index, 0xFF, "VbSharedDataInit firmware index");

	/* Sample some other fields to make sure they were zeroed */
	TEST_EQ(d->flags, 0, "VbSharedDataInit firmware flags");
	TEST_EQ(d->lk_call_count, 0, "VbSharedDataInit lk_call_count");
	TEST_EQ(d->kernel_version_lowest, 0,
		"VbSharedDataInit kernel_version_lowest");

	TEST_EQ(VBOOT_SHARED_DATA_INVALID, VbSharedDataSetKernelKey(NULL, NULL),
		"VbSharedDataSetKernelKey sd null");
	TEST_EQ(VBOOT_PUBLIC_KEY_INVALID, VbSharedDataSetKernelKey(d, NULL),
		"VbSharedDataSetKernelKey pubkey null");
}

int main(int argc, char* argv[])
{
	StructPackingTest();
	VbSharedDataTest();

	return gTestSuccess ? 0 : 255;
}
