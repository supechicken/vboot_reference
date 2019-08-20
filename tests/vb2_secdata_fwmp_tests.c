/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware management parameters (FWMP) library.
 */

#include "2common.h"
#include "2misc.h"
#include "2secdata.h"
#include "test_common.h"

static void test_changed(struct vb2_context *c, int changed, const char *why)
{
	if (changed)
		TEST_NEQ(c->flags & VB2_CONTEXT_SECDATA_FWMP_CHANGED, 0, why);
	else
		TEST_EQ(c->flags & VB2_CONTEXT_SECDATA_FWMP_CHANGED, 0, why);

	c->flags &= ~VB2_CONTEXT_SECDATA_FWMP_CHANGED;
};

static void secdata_fwmp_test(void)
{
	uint8_t workbuf[VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE]
		__attribute__ ((aligned (VB2_WORKBUF_ALIGN)));
	struct vb2_context c = {
		.flags = 0,
		.workbuf = workbuf,
		.workbuf_size = sizeof(workbuf),
	};
	struct vb2_secdata_fwmp *sec =
		(struct vb2_secdata_fwmp *)c.secdata_fwmp;
	struct vb2_shared_data *sd = vb2_get_sd(&c);
	uint32_t size = 0;

	/* Check size constant */
	TEST_TRUE(sizeof(struct vb2_secdata_fwmp) <= VB2_SECDATA_FWMP_MAX_SIZE,
		  "Struct size constant");

	/* Size too large */
	memset(c.secdata_fwmp, 0xff, sizeof(c.secdata_fwmp));
	TEST_EQ(vb2api_secdata_fwmp_check(&c, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Check large size");
	TEST_EQ(vb2_secdata_fwmp_init(&c, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Init large size");

	/* Size too small */
	memset(c.secdata_fwmp, 0, sizeof(c.secdata_fwmp));
	TEST_EQ(vb2api_secdata_fwmp_check(&c, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Check small size");
	TEST_EQ(vb2_secdata_fwmp_init(&c, &size),
		VB2_ERROR_SECDATA_FWMP_SIZE, "Init small size");

	/* Blank data is invalid */
	memset(c.secdata_fwmp, 0xa6, sizeof(c.secdata_fwmp));
	sec->struct_size = sizeof(*sec);
	size = sec->struct_size;
	TEST_EQ(vb2api_secdata_fwmp_check(&c, &size),
		VB2_ERROR_SECDATA_FWMP_CRC, "Check blank CRC");
	TEST_EQ(vb2_secdata_fwmp_init(&c, &size),
		VB2_ERROR_SECDATA_FWMP_CRC, "Init blank CRC");

	/* Create good data */
	vb2api_secdata_fwmp_create(&c);
	TEST_SUCC(vb2api_secdata_fwmp_check(&c, &size), "Check created CRC");
	TEST_SUCC(vb2_secdata_fwmp_init(&c, &size), "Init created CRC");
	TEST_NEQ(sd->status & VB2_SD_STATUS_SECDATA_FWMP_INIT, 0,
		 "Init set SD status");
	sd->status &= ~VB2_SD_STATUS_SECDATA_FWMP_INIT;
	test_changed(&c, 1, "Create changes data");

#if 0
	/* Bad CRC causes retry, then eventual failure */
	ResetMocks(0, 0);
	mock_fwmp.fwmp.crc++;
	TEST_EQ(secdata_fwmp_read(&c), TPM_E_CORRUPTED_STATE,
		"secdata_fwmp_read() crc");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n",
		    "  tlcl calls");

	/* Struct size too large with bad CRC */
	ResetMocks(0, 0);
	mock_fwmp.fwmp.struct_size += 4;
	mock_fwmp.fwmp.crc++;
	TEST_EQ(secdata_fwmp_read(&c), TPM_E_CORRUPTED_STATE,
		"secdata_fwmp_read() bigger crc");
	TEST_STR_EQ(mock_calls,
		    "TlclRead(0x100a, 40)\n"
		    "TlclRead(0x100a, 44)\n",
		    "  tlcl calls");
	TEST_EQ(0, memcmp(&fwmp, &fwmp_zero, sizeof(fwmp)), "  data");

	/* Minor version difference ok */
	ResetMocks(0, 0);
	mock_fwmp.fwmp.struct_version++;
	TEST_EQ(secdata_fwmp_read(&c), 0, "secdata_fwmp_read() minor version");
	TEST_EQ(0, memcmp(&fwmp, &mock_fwmp, sizeof(fwmp)), "  data");

	/* Major version difference not ok */
	ResetMocks(0, 0);
	mock_fwmp.fwmp.struct_version += 0x10;
	TEST_EQ(secdata_fwmp_read(&c), TPM_E_STRUCT_VERSION,
		"secdata_fwmp_read() major version");
#endif
}

int main(int argc, char* argv[])
{
	secdata_fwmp_test();

	return gTestSuccess ? 0 : 255;
}
