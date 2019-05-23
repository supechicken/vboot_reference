/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for GBB library.
 */

#include "2common.h"
#include "2misc.h"
#include "test_common.h"

/* Mock data */
static char gbb_data[4096 + sizeof(struct vb2_gbb_header)];
static struct vb2_gbb_header *gbb = (struct vb2_gbb_header *)gbb_data;
static struct vb2_packed_key *rootkey;
static struct vb2_context ctx;
static struct vb2_workbuf wb;
static uint8_t workbuf[VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE];

static void set_gbb_hwid(const char *hwid, size_t size)
{
	memcpy(gbb_data + gbb->hwid_offset, hwid, size);
	gbb->hwid_size = size;
}

static void reset_common_data(void)
{
	int gbb_used;

	memset(gbb_data, 0, sizeof(gbb_data));
	gbb->header_size = sizeof(*gbb);
	gbb->major_version = VB2_GBB_MAJOR_VER;
	gbb->minor_version = VB2_GBB_MINOR_VER;
	gbb->flags = 0;
	gbb_used = sizeof(struct vb2_gbb_header);

	gbb->recovery_key_offset = gbb_used;
	gbb->recovery_key_size = 64;
	gbb_used += gbb->recovery_key_size;
	gbb->rootkey_offset = gbb_used;
	gbb->rootkey_size = sizeof(struct vb2_packed_key);
	gbb_used += gbb->rootkey_size;

	rootkey = ((void *)gbb + gbb->rootkey_offset);
	rootkey->key_offset = sizeof(*rootkey);

	gbb->hwid_offset = gbb_used;
	const char hwid_src[] = "Test HWID";
	set_gbb_hwid(hwid_src, sizeof(hwid_src));

	memset(workbuf, 0, sizeof(workbuf));
	memset(&ctx, 0, sizeof(ctx));
	ctx.workbuf = workbuf;
	ctx.workbuf_size = sizeof(workbuf);
	vb2_init_context(&ctx);
	vb2_workbuf_from_ctx(&ctx, &wb);
}

/* Mocks */
struct vb2_gbb_header *vb2_get_gbb(struct vb2_context *c)
{
	return gbb;
}

int vb2ex_read_resource(struct vb2_context *c,
			enum vb2_resource_index index,
			uint32_t offset,
			void *buf,
			uint32_t size)
{
	uint8_t *rptr;
	uint32_t rsize;

	switch(index) {
	case VB2_RES_GBB:
		rptr = (uint8_t *)&gbb_data;
		rsize = sizeof(gbb_data);
		break;
	default:
		return VB2_ERROR_EX_READ_RESOURCE_INDEX;
	}

	if (offset > rsize || offset + size > rsize)
		return VB2_ERROR_EX_READ_RESOURCE_SIZE;

	memcpy(buf, rptr + offset, size);
	return VB2_SUCCESS;
}

/* Tests */
static void key_tests(void)
{
	/* Assume that root key and recovery key are dealt with using the same
	   code in our GBB library functions. */
	struct vb2_packed_key *keyp;
	struct vb2_workbuf wborig;
	const char key_data[] = "HELLOWORLD";
	uint32_t size;

	/* gbb.offset < sizeof(vb2_gbb_header) */
	reset_common_data();
	wborig = wb;
	gbb->rootkey_offset = sizeof(*rootkey) - 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_GBB_INVALID,
		"gbb.rootkey offset too small");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* gbb.offset > gbb_data */
	reset_common_data();
	wborig = wb;
	gbb->rootkey_offset = sizeof(gbb_data) + 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_EX_READ_RESOURCE_SIZE,
		"gbb.rootkey offset too large");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* gbb.size < sizeof(vb2_packed_key) */
	reset_common_data();
	wborig = wb;
	gbb->rootkey_size = sizeof(*rootkey) - 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_GBB_INVALID,
		"gbb.rootkey size too small");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* sizeof(vb2_packed_key) > workbuf.size */
	reset_common_data();
	wborig = wb;
	wb.size = sizeof(*rootkey) - 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_GBB_WORKBUF,
		"workbuf size too small for vb2_packed_key header");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* packed_key.offset < sizeof(vb2_packed_key) */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = 1; 
	rootkey->key_offset = sizeof(*rootkey) - 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_INSIDE_DATA_OVERLAP,
		"rootkey offset too small");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* packed_key.offset > gbb_data */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = 1;
	rootkey->key_offset = sizeof(gbb_data) + 1;
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_EX_READ_RESOURCE_SIZE,
		"rootkey size too large");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* packed_key.size > workbuf.size */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = wb.size + 1;
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size + 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_GBB_WORKBUF,
		"workbuf size too small for vb2_packed_key contents");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* gbb.size < sizeof(vb2_packed_key) + packed_key.size */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = 2;
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size - 1;
	TEST_EQ(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		VB2_ERROR_INSIDE_DATA_OUTSIDE,
		"rootkey size exceeds gbb.rootkey size");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	/* gbb.size == sizeof(vb2_packed_key) + packed_key.size */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = sizeof(key_data);
	memcpy((void *)rootkey + rootkey->key_offset,
	       key_data, sizeof(key_data));
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size;
	TEST_SUCC(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		  "succeeds when gbb.rootkey and rootkey sizes agree");
	TEST_TRUE(wb.size < wborig.size,
		  "  workbuf shrank on success");
	TEST_EQ(memcmp(rootkey, keyp, rootkey->key_offset + rootkey->key_size),
		0, "  copied key data successfully");
	TEST_EQ(size, rootkey->key_offset + rootkey->key_size,
		"  correct size returned");

	/* gbb.size > sizeof(vb2_packed_key) + packed_key.size */
	reset_common_data();
	wborig = wb;
	rootkey->key_size = 1;
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size + 1;
	TEST_SUCC(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		  "succeeds when gbb.rootkey is padded");
	TEST_TRUE(wb.size < wborig.size,
		  "  workbuf shrank on success");
	TEST_EQ(size, rootkey->key_offset + rootkey->key_size,
		"  correct size returned");

	/* packed_key.size = 0, packed_key.offset = +1 */
	reset_common_data();
	wborig = wb;
	rootkey->key_offset = sizeof(*rootkey) + 1;
	rootkey->key_size = 0;
	gbb->rootkey_size = rootkey->key_offset + rootkey->key_size + 1;
	TEST_SUCC(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		  "succeeds when gbb.rootkey is padded; empty test key");
	TEST_TRUE(wb.size < wborig.size,
		  "  workbuf shrank on success");
	TEST_EQ(size, rootkey->key_offset + rootkey->key_size,
		"  correct size returned");

	/* packed_key.size = 0, packed_key.offset = -1 */
	reset_common_data();
	wborig = wb;
	rootkey->key_offset = sizeof(*rootkey) - 1;
	rootkey->key_size = 0;
	gbb->rootkey_size = sizeof(*rootkey) + rootkey->key_size + 1;
	TEST_SUCC(vb2_gbb_read_root_key(&ctx, &keyp, &size, &wb),
		  "succeeds when gbb.rootkey is padded; empty test key");
	TEST_TRUE(wb.size < wborig.size,
		  "  workbuf shrank on success");
	TEST_EQ(size, sizeof(*rootkey), "  correct size returned");
}

static void hwid_tests(void)
{
	char *hwid;
	uint32_t size;
	struct vb2_workbuf wborig;

	reset_common_data();
	size = 0;
	const char hwid_src1[] = "Test HWID\0garbagegarbage";
	set_gbb_hwid(hwid_src1, sizeof(hwid_src1));
	TEST_SUCC(vb2_gbb_read_hwid(&ctx, &hwid, &size, &wb),
		  "read HWID with garbage");
	TEST_TRUE((void *)wb.buf > (void *)hwid, "  workbuf contains HWID");
	TEST_EQ(strcmp(hwid, "Test HWID"), 0, "  HWID correct");
	TEST_EQ(strlen(hwid) + 1, size, "  HWID size consistent");
	TEST_EQ(strlen(hwid), strlen("Test HWID"), "  HWID size correct");

	reset_common_data();
	size = 0;
	const char hwid_src2[] = "HWIDtest";
	set_gbb_hwid(hwid_src2, sizeof(hwid_src2));
	TEST_SUCC(vb2_gbb_read_hwid(&ctx, &hwid, &size, &wb),
		  "read normal HWID");
	TEST_TRUE((void *)wb.buf > (void *)hwid, "  workbuf contains HWID");
	TEST_EQ(strcmp(hwid, hwid_src2), 0, "  HWID correct");
	TEST_EQ(strlen(hwid) + 1, size, "  HWID size consistent");
	TEST_EQ(strlen(hwid), strlen(hwid_src2), "  HWID size correct");

	reset_common_data();
	wborig = wb;
	gbb->hwid_size = 0;
	TEST_EQ(vb2_gbb_read_hwid(&ctx, &hwid, NULL, &wb),
		VB2_ERROR_GBB_INVALID,
		"HWID size invalid (HWID missing)");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	reset_common_data();
	wborig = wb;
	gbb->hwid_offset = sizeof(gbb_data) + 1;
	TEST_EQ(vb2_gbb_read_hwid(&ctx, &hwid, NULL, &wb),
		VB2_ERROR_EX_READ_RESOURCE_SIZE,
		"HWID offset invalid (HWID missing)");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	reset_common_data();
	wborig = wb;
	gbb->hwid_size = wb.size + 1;
	TEST_EQ(vb2_gbb_read_hwid(&ctx, &hwid, NULL, &wb),
		VB2_ERROR_GBB_WORKBUF,
		"workbuf too small for HWID");
	TEST_TRUE(wb.buf == wborig.buf,
		  "  workbuf restored on error");

	{
		char *hwid2;
		uint32_t size2 = 0;
		reset_common_data();
		TEST_SUCC(vb2api_gbb_read_hwid(&ctx, &hwid, &size),
			  "read HWID to check dynamic allocation");
		TEST_SUCC(vb2api_gbb_read_hwid(&ctx, &hwid2, &size2),
			  "  read HWID again");
		TEST_TRUE(hwid != hwid2, "  different pointer returned");
		TEST_TRUE(size == size2, "  same size returned");
		free(hwid);
		free(hwid2);
	}
}

int main(int argc, char* argv[])
{
	key_tests();
	hwid_tests();

	return gTestSuccess ? 0 : 255;
}
