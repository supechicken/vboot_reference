/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "unit_tests.h"
#include "updater.h"
#include "cgptlib_internal.h"
#include "file_type.h"
#include "futility_options.h"
#include "host_misc.h"
#include <sys/mman.h>

#define IMAGE_MAIN GET_DATA("image.bin")
#define FILE_TEMP GET_DATA("file-temp")
#define FILE_SMALL GET_DATA("file-small")
#define FILE_SMALL_SIZE strlen("small")
#define FILE_NONEXISTENT GET_DATA("nonexistent")
#define FILE_READONLY GET_DATA("read-only")

static int unit_tests_prepare_data(void)
{
	UNIT_TEST_BEGIN;

	ASSERT(system("rm -rf " DATA_COPY_PATH) == 0);
	ASSERT(system("mkdir -p " DATA_COPY_PATH) == 0);

	ASSERT(futil_copy_file(GET_SOURCE("image-steelix.bin"), IMAGE_MAIN) != -1);
	ASSERT(vb2_write_file(FILE_SMALL, "small", FILE_SMALL_SIZE) == VB2_SUCCESS);
	remove(FILE_NONEXISTENT);
	ASSERT(system("touch " FILE_READONLY) == 0);
	ASSERT(system("chmod 444 " FILE_READONLY) == 0);

unit_cleanup:
	UNIT_TEST_RETURN;
}

static int test_gbb(void)
{
	UNIT_TEST_BEGIN;
	struct firmware_image image = {0};
	FmapAreaHeader *ah = NULL;		    /* Do not free. */
	uint8_t *ptr = NULL;			    /* Do not free. */
	struct vb2_gbb_header *gbb, good_gbb = {0}; /* Do not free. */
	char *str = NULL;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, "GBB", &ah);
	gbb = (struct vb2_gbb_header *)ptr;
	good_gbb = *gbb;

	TEST_EQ(ft_recognize_gbb(ptr, ah->area_size), FILE_TYPE_GBB,
		"Ft recognize GBB: correct");
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 1,
		"Futil valid GBB header: correct");

	TEST_EQ(ft_recognize_gbb(ptr, sizeof(struct vb2_gbb_header) - 1), FILE_TYPE_UNKNOWN,
		"Ft recognize GBB: too small");
	TEST_EQ(futil_valid_gbb_header(gbb, gbb->header_size - 1, NULL), 0,
		"Futil valid GBB header: too small");

	strcpy((char *)gbb->signature, "");
	TEST_EQ(ft_recognize_gbb(ptr, ah->area_size), FILE_TYPE_UNKNOWN,
		"Ft recognize GBB: invalid signature");
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid signature");
	*gbb = good_gbb;

	gbb->major_version = 65535;
	TEST_EQ(ft_recognize_gbb(ptr, ah->area_size), FILE_TYPE_UNKNOWN,
		"Ft recognize GBB: invalid major version");
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid major version");
	*gbb = good_gbb;

	gbb->header_size = ah->area_size + 1;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid header_size");
	*gbb = good_gbb;

	gbb->hwid_offset = EXPECTED_VB2_GBB_HEADER_SIZE - 1;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid hwid_offset");
	*gbb = good_gbb;

	gbb->hwid_offset = ah->area_size + 1;
	gbb->hwid_size = 0;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid hwid_offset or hwid_size");
	*gbb = good_gbb;

	gbb->rootkey_offset = ah->area_size + 1;
	gbb->rootkey_size = 0;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid rootkey_offset or rootkey_size");
	*gbb = good_gbb;

	gbb->bmpfv_offset = ah->area_size + 1;
	gbb->bmpfv_size = 0;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid bmpfv_offset or bmpfv_size");
	*gbb = good_gbb;

	gbb->recovery_key_offset = EXPECTED_VB2_GBB_HEADER_SIZE - 1;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid recovery_key_offset");
	*gbb = good_gbb;

	gbb->recovery_key_offset = ah->area_size + 1;
	gbb->recovery_key_offset = 0;
	TEST_EQ(futil_valid_gbb_header(gbb, ah->area_size, NULL), 0,
		"Futil valid GBB header: invalid recovery_key_offset or recovery_key_offset");
	*gbb = good_gbb;

	str = malloc(gbb->hwid_size + 1);
	ASSERT(str != NULL);
	for (int i = 0; i < gbb->hwid_size; i++)
		str[i] = 'X';
	str[gbb->hwid_size] = '\n';
	TEST_EQ(futil_set_gbb_hwid(gbb, str), -1, "Futil set GBB HWID: too big");
	free(str);

	str = (char *)"M";
	TEST_EQ(futil_set_gbb_hwid(gbb, str), 0, "Futil set GBB HWID: valid");
	TEST_EQ(strncmp((char *)gbb + gbb->hwid_offset, str, 2), 0, "Verifying");

	gbb->minor_version = 1;
	str = (char *)"N";
	TEST_EQ(futil_set_gbb_hwid(gbb, str), 0, "Futil set GBB HWID: minor < 2");
	TEST_EQ(strncmp((char *)gbb + gbb->hwid_offset, str, 2), 0, "Verifying");

	str = NULL;
unit_cleanup:
	free_firmware_image(&image);
	free(str);
	UNIT_TEST_RETURN;
}

static int test_files_open_close(void)
{
	UNIT_TEST_BEGIN;
	int fd;

	TEST_EQ(futil_copy_file(FILE_SMALL, FILE_TEMP), FILE_SMALL_SIZE,
		"Futil copy file: valid");
	TEST_NEQ(futil_copy_file(FILE_NONEXISTENT, FILE_TEMP), 0,
		 "Futil copy file: nonexistent");
	TEST_NEQ(futil_copy_file(FILE_TEMP, FILE_READONLY), 0, "Futil copy file: invalid");

	TEST_EQ(futil_open_file(FILE_TEMP, &fd, FILE_RW), FILE_ERR_NONE, "Futil open file: rw");
	TEST_EQ(futil_close_file(fd), FILE_ERR_NONE, "Futil close file: rw");
	TEST_EQ(futil_open_file(FILE_NONEXISTENT, &fd, FILE_RW), FILE_ERR_OPEN,
		"Futil open file: rw nonexistent");

	TEST_EQ(futil_open_file(FILE_TEMP, &fd, FILE_RO), FILE_ERR_NONE, "Futil open file: ro");
	TEST_EQ(futil_close_file(fd), FILE_ERR_NONE, "Futil close file: ro");
	TEST_EQ(futil_open_file(FILE_NONEXISTENT, &fd, FILE_RO), FILE_ERR_OPEN,
		"Futil open file: ro nonexistent");

	ASSERT(futil_open_file(FILE_TEMP, &fd, FILE_RW) == FILE_ERR_NONE);
	ASSERT(futil_close_file(fd) == FILE_ERR_NONE);
	TEST_EQ(futil_close_file(fd), FILE_ERR_CLOSE, "Futil close file: invalid");

unit_cleanup:
	UNIT_TEST_RETURN;
}

static int test_files_mmap(void)
{
	UNIT_TEST_BEGIN;
	int fd;
	uint8_t *data = NULL; /* Do not free. */
	uint32_t size;

	TEST_EQ(futil_map_file(-1, FILE_RO, &data, &size), FILE_ERR_STAT,
		"Futil map file: invalid fd");

	/* Would be nice to test unreasonable (>4GiB) files too. */

	ASSERT(futil_open_file(FILE_TEMP, &fd, FILE_RO) == FILE_ERR_NONE);
	TEST_EQ(futil_map_file(fd, FILE_RO, &data, &size), FILE_ERR_NONE, "Futil map file");
	TEST_EQ(futil_unmap_file(fd, FILE_RO, data, size), FILE_ERR_NONE, "Futil unmap file");
	ASSERT(futil_close_file(fd) == FILE_ERR_NONE);

	TEST_NEQ(futil_open_and_map_file(FILE_NONEXISTENT, &fd, FILE_RO, &data, &size),
		 FILE_ERR_NONE, "Futil open and map file: nonexistent");

	TEST_EQ(futil_open_and_map_file(FILE_TEMP, &fd, FILE_RO, &data, &size), FILE_ERR_NONE,
		"Futil open and map file");
	TEST_EQ(futil_unmap_and_close_file(fd, FILE_RO, data, size), FILE_ERR_NONE,
		"Futil unmap and close file");
	TEST_NEQ(futil_unmap_and_close_file(fd, FILE_RO, data, size), FILE_ERR_NONE,
		 "Futil unmap and close file: invalid fd");

unit_cleanup:
	UNIT_TEST_RETURN;
}

static int test_misc(void)
{
	size_t len = 4096;
	uint8_t *ptr = malloc(4096);
	GptHeader *h = (GptHeader *)(ptr + 512);
	uint8_t *data = (uint8_t *)"test";

	/* Pretend we have a valid GPT. */
	memcpy(h->signature, GPT_HEADER_SIGNATURE2, GPT_HEADER_SIGNATURE_SIZE);
	h->revision = GPT_HEADER_REVISION;
	h->my_lba = 0; /* Used to break HeaderCrc */
	h->size = MIN_SIZE_OF_HEADER + 1;
	h->header_crc32 = HeaderCrc(h);

	TEST_EQ(ft_recognize_gpt(ptr, len), FILE_TYPE_CHROMIUMOS_DISK,
		"Ft recognize GPT: valid");

	memcpy(h->signature, "12345678", GPT_HEADER_SIGNATURE_SIZE);
	TEST_EQ(ft_recognize_gpt(ptr, len), FILE_TYPE_UNKNOWN,
		"Ft recognize GPT: invalid signature");
	memcpy(h->signature, GPT_HEADER_SIGNATURE2, GPT_HEADER_SIGNATURE_SIZE);

	h->revision = GPT_HEADER_REVISION + 1;
	TEST_EQ(ft_recognize_gpt(ptr, len), FILE_TYPE_UNKNOWN,
		"Ft recognize GPT: invalid revision");
	h->revision = GPT_HEADER_REVISION;

	h->size = MAX_SIZE_OF_HEADER + 1;
	TEST_EQ(ft_recognize_gpt(ptr, len), FILE_TYPE_UNKNOWN,
		"Ft recognize GPT: invalid size");
	h->size = MIN_SIZE_OF_HEADER + 1;

	h->my_lba = 1;
	TEST_EQ(ft_recognize_gpt(ptr, len), FILE_TYPE_UNKNOWN,
		"Ft recognize GPT: invalid crc32");

	free(ptr);

	len = strlen("test");

	TEST_NEQ(write_to_file("test", FILE_READONLY, data, len), 0,
		 "Write to file: invalid file");
	TEST_EQ(write_to_file("test", FILE_TEMP, data, 0), 0, "Write to file: zero bytes");
	TEST_EQ(write_to_file("test", FILE_TEMP, data, len), 0, "Write to file: valid");

	return UNIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	if (unit_tests_prepare_data() == UNIT_FAIL) {
		ERROR("Failed to prepare data.\n");
		return 1;
	}

	test_gbb();
	test_files_open_close();
	test_files_mmap();
	test_misc();

	return !gTestSuccess;
}
