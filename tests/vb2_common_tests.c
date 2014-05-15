/* Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for firmware 2common.c
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "test_common.h"

#include "2common.h"

/**
 * Test struct packing for vboot_struct.h structs which are passed between
 * firmware and OS, or passed between different phases of firmware.
 */
static void test_struct_packing(void)
{
	TEST_EQ(EXPECTED_VBPUBLICKEY_SIZE, sizeof(struct vb2_packed_key),
		"sizeof(vb2_packed_key)");
	TEST_EQ(EXPECTED_VBSIGNATURE_SIZE, sizeof(struct vb2_signature),
		"sizeof(vb2_signature)");
	TEST_EQ(EXPECTED_VB2KEYBLOCKHEADER_SIZE,
		sizeof(struct vb2_keyblock),
		"sizeof(VbKeyBlockHeader)");
	TEST_EQ(EXPECTED_VB2FIRMWAREPREAMBLEHEADER2_1_SIZE,
		sizeof(struct vb2_fw_preamble),
		"sizeof(vb2_fw_preamble)");
}

/**
 * Helper functions not dependent on specific key sizes
 */
static void test_helper_functions(void)
{
	{
		uint8_t *p = (uint8_t *)test_helper_functions;
		TEST_EQ((int)vb2_offset_of(p, p), 0, "vb2_offset_of() equal");
		TEST_EQ((int)vb2_offset_of(p, p+10), 10,
			"vb2_offset_of() positive");
	}

	{
		struct vb2_packed_key k = {.key_offset = sizeof(k)};
		TEST_EQ((int)vb2_offset_of(&k, vb2_packed_key_data(&k)),
			sizeof(k), "vb2_packed_key_data() adjacent");
	}

	{
		struct vb2_packed_key k = {.key_offset = 123};
		TEST_EQ((int)vb2_offset_of(&k, vb2_packed_key_data(&k)), 123,
			"vb2_packed_key_data() spaced");
	}

	{
		struct vb2_signature s = {.sig_offset = sizeof(s)};
		TEST_EQ((int)vb2_offset_of(&s, vb2_signature_data(&s)),
			sizeof(s), "vb2_signature_data() adjacent");
	}

	{
		struct vb2_signature s = {.sig_offset = 123};
		TEST_EQ((int)vb2_offset_of(&s, vb2_signature_data(&s)), 123,
			"vb2_signature_data() spaced");
	}

	{
		uint8_t *p = (uint8_t *)test_helper_functions;
		TEST_EQ(vb2_verify_member_inside(p, 20, p, 6, 11, 3), 0,
			"MemberInside ok 1");
		TEST_EQ(vb2_verify_member_inside(p, 20, p+4, 4, 8, 4), 0,
			"MemberInside ok 2");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p-4, 4, 8, 4), 0,
			 "MemberInside member before parent");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p+20, 4, 8, 4), 0,
			 "MemberInside member after parent");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p, 21, 0, 0), 0,
			 "MemberInside member too big");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p, 4, 21, 0), 0,
			 "MemberInside data after parent");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p, 4, (uint32_t)-1, 0),
			 0, "MemberInside data before parent");
		TEST_NEQ(vb2_verify_member_inside(p, 20, p, 4, 4, 17), 0,
			 "MemberInside data too big");
		TEST_NEQ(vb2_verify_member_inside(p, (uint32_t)-1,
						  p+(uint32_t)-10, 12, 5, 0), 0,
			 "MemberInside wraparound 1");
		TEST_NEQ(vb2_verify_member_inside(p, (uint32_t)-1,
						  p+(uint32_t)-10, 5, 12, 0), 0,
			 "MemberInside wraparound 2");
		TEST_NEQ(vb2_verify_member_inside(p, (uint32_t)-1,
						  p+(uint32_t)-10, 5, 0, 12), 0,
			 "MemberInside wraparound 3");
	}

	{
		struct vb2_packed_key k = {.key_offset = sizeof(k),
					   .key_size = 128};
		TEST_EQ(vb2_verify_packed_key_inside(&k, sizeof(k)+128, &k), 0,
			"PublicKeyInside ok 1");
		TEST_EQ(vb2_verify_packed_key_inside(&k - 1,
						     2*sizeof(k)+128, &k),
			0, "PublicKeyInside ok 2");
		TEST_NEQ(vb2_verify_packed_key_inside(&k, 128, &k), 0,
			 "PublicKeyInside key too big");
	}

	{
		struct vb2_packed_key k = {.key_offset = 100,
					   .key_size = 4};
		TEST_NEQ(vb2_verify_packed_key_inside(&k, 99, &k), 0,
			 "PublicKeyInside offset too big");
	}

	{
		struct vb2_signature s = {.sig_offset = sizeof(s),
					  .sig_size = 128};
		TEST_EQ(vb2_verify_signature_inside(&s, sizeof(s)+128, &s), 0,
			"SignatureInside ok 1");
		TEST_EQ(vb2_verify_signature_inside(&s - 1,
						    2*sizeof(s)+128, &s),
			0, "SignatureInside ok 2");
		TEST_NEQ(vb2_verify_signature_inside(&s, 128, &s), 0,
			 "SignatureInside sig too big");
	}

	{
		struct vb2_signature s = {.sig_offset = 100,
					  .sig_size = 4};
		TEST_NEQ(vb2_verify_signature_inside(&s, 99, &s), 0,
			 "SignatureInside offset too big");
	}
}

int main(int argc, char* argv[])
{
	test_struct_packing();
	test_helper_functions();

	if (vboot_api_stub_check_memory())
		return 255;

	return gTestSuccess ? 0 : 255;
}
