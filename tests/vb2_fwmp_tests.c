/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware management parameters (FWMP) library.
 */

#include "2common.h"
#include "2secdata.h"
#include "test_common.h"

static void fwmp_test(void)
{
	uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
		__attribute__ ((aligned (VB2_WORKBUF_ALIGN)));
	struct vb2_context c = {
		.flags = 0,
		.workbuf = workbuf,
		.workbuf_size = sizeof(workbuf),
	};

	/* Check size constant */
	TEST_TRUE(sizeof(struct vb2_fwmp) <= VB2_FWMP_MAX_SIZE,
		"Struct size constant");

	/* Blank data is invalid */
	memset(c.fwmp, 0xa6, sizeof(c.fwmp));
	TEST_EQ(vb2api_fwmp_check(&c), VB2_ERROR_FWMP_CRC, "Check blank CRC");
	TEST_EQ(vb2_fwmp_init(&c), VB2_ERROR_FWMP_CRC, "Init blank CRC");
}

int main(int argc, char* argv[])
{
	fwmp_test();

	return gTestSuccess ? 0 : 255;
}
