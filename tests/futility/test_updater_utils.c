/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>
#include "futility.h"
#include "updater.h"
#include "updater_utils.h"
#include "2struct.h"
#include "common/tests.h"

#define IMAGE_TEMP "tests/futility/data_copy/image-temp.bin"
#define IMAGE_MAIN "tests/futility/data_copy/image-newer.bin"
#define IMAGE_BAD "tests/futility/data_copy/image-bad.bin"
#define IMAGE_MISSING_FMAP "tests/futility/data_copy/image-missing-fmap.bin"
#define IMAGE_MISSING_FRID "tests/futility/data_copy/image-missing-ro_frid.bin"
#define IMAGE_MISSING_FWID "tests/futility/data_copy/image-missing-rw_fwid.bin"
#define ARCHIVE "tests/futility/data_copy/images.zip"

static char __format_buf[4096];
#define format(...)                                                                            \
	({                                                                                     \
		sprintf(__format_buf, __VA_ARGS__);                                            \
		__format_buf;                                                                  \
	})

static int copy_image(const char *path)
{
	static char buf[1024];
	sprintf(buf, "cp %s %s", path, IMAGE_TEMP);
	INFO("%s", buf);
	return system(buf);
}

#define IF_FAIL(value, ...)                                                                    \
	{                                                                                      \
		if (!(value))                                                                  \
			TEST_SUCC(1, format(__VA_ARGS__));                                     \
	}

static void test_temp_file(int _)
{
	struct tempfile head = {NULL, NULL};
	const char *file = create_temp_file(&head);
	TEST_PTR_NEQ(file, NULL, "Creating temp file normal");

	struct firmware_image image;
	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");
	TEST_PTR_NEQ(get_firmware_image_temp_file(&image, &head), NULL,
		     "Getting temp file for image");

	image.size = 0;
	TEST_PTR_EQ(get_firmware_image_temp_file(&image, &head), NULL,
		    "Getting temp file for invalid image");

	remove_all_temp_files(&head);
}

static void test_load_firmware_image(int _)
{
	struct firmware_image image;

	TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, NULL), 0, "Load normal image");

	TEST_EQ(load_firmware_image(&image, NULL, NULL), IMAGE_READ_FAILURE,
		"Load NULL filename");

	TEST_EQ(load_firmware_image(&image, "", NULL), IMAGE_READ_FAILURE,
		"Load empty filename");

	TEST_EQ(load_firmware_image(&image, "invalid file", NULL), IMAGE_READ_FAILURE,
		"Load invalid file");

	{
		struct u_archive *archive = archive_open(ARCHIVE);
		IF_FAIL(archive != NULL, "Failed to open archive");

		TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, archive), 0,
			"Load from archive");
		free_firmware_image(&image);

		TEST_EQ(load_firmware_image(&image, "invalid file", archive),
			IMAGE_READ_FAILURE, "Load invalid file from archive");
		free_firmware_image(&image);

		archive_close(archive);
	}

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FMAP, NULL), IMAGE_PARSE_FAILURE,
		"Missing FMAP");

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FRID, NULL), IMAGE_PARSE_FAILURE,
		"Missing RO_FRID");

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FWID, NULL), IMAGE_PARSE_FAILURE,
		"Missing RW_FWID*");

	free_firmware_image(&image);
}

static void test_reload_firmware_image(int _)
{
	struct firmware_image image;
	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");
	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading image");
	free_firmware_image(&image);

	// Unopened image
	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading image");
	free_firmware_image(&image);
}

static void test_system_firmware(int _)
{
	struct updater_config_arguments args = {0};
	struct updater_config *cfg = updater_new_config();
	TEST_PTR_NEQ(cfg, NULL, "Creating updater config");

	args.use_flash = 1;
	args.image = (char *)IMAGE_MAIN;
	copy_image(IMAGE_MAIN);
	args.emulation = (char *)IMAGE_TEMP;

	TEST_EQ(updater_setup_config(cfg, &args), 0, "Setting up config");
	cfg->quirks[QUIRK_EXTRA_RETRIES].value = 2;

	const char *programmer = cfg->image_current.programmer;
	cfg->image_current.programmer = "<invalid programmer>";
	TEST_NEQ(load_system_firmware(cfg, &cfg->image_current), 0, "Invalid programmer");
	cfg->image_current.programmer = programmer;

	TEST_EQ(write_system_firmware(cfg, &cfg->image, NULL, 0), 0, "Writing system firmware");
	TEST_EQ(load_system_firmware(cfg, &cfg->image_current), 0, "Loading system firmware");
	TEST_TRUE(cfg->image.size == cfg->image_current.size, "Verifying size");
	TEST_EQ(memcmp(cfg->image.data, cfg->image_current.data, cfg->image.size), 0,
		"Verifying contents");

	const char *regions[1] = {"RW_LEGACY"};
	uint8_t *ptr = fmap_find_by_name(cfg->image.data, cfg->image.size,
					 cfg->image.fmap_header, "RW_LEGACY", NULL);
	ptr[0] ^= 255; // To check whether the data was indeed written
	int value = ptr[0];
	uint64_t offset = ptr - cfg->image.data;
	TEST_EQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		"Writing system firmware (region)");
	IF_FAIL(load_system_firmware(cfg, &cfg->image_current) == 0,
		"Failed to load system firmware");
	TEST_EQ(cfg->image_current.data[offset], value, "Verifying written region");

	regions[0] = "<invalid region>";
	TEST_NEQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		 "Writing invalid region");

	updater_delete_config(cfg);
}

static void test_firmware_sections(int _)
{
	struct firmware_image image;
	struct firmware_section section;
	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	TEST_EQ(find_firmware_section(&section, &image, "RO_FRID"), 0, "Find firmware section");
	TEST_EQ(firmware_section_exists(&image, "RO_FRID"), 1, "Firmware section exists");

	memset(image.data, 0, image.size);

	TEST_NEQ(find_firmware_section(&section, &image, "RO_FRID"), 0,
		 "Find missing firmware section");
	TEST_NEQ(firmware_section_exists(&image, "RO_FRID"), 1,
		 "Firmware section doesn't exist");
}

#if 0
static void test_preserve_firmware_section(int _)
{
	/* Same image */
	{
		struct firmware_image image1 = load_image(IMAGE_MAIN, 0);
		struct firmware_image image2 = load_image(IMAGE_MAIN, 0);

		FmapAreaHeader *ah;
		fmap_find_by_name(image1.data, image1.size, image1.fmap_header, "RW_LEGACY",
				  &ah);

		TEST_EQ(preserve_firmware_section(&image1, &image2, "RW_LEGACY"), 0,
			"Preserving RW_LEGACY");

		memset(image1.data + ah->area_offset, 0, ah->area_size);

		TEST_EQ(preserve_firmware_section(&image2, &image1, "RW_LEGACY"), 0,
			"Restoring RW_LEGACY");

		TEST_EQ(memcmp(image1.data, image2.data, image1.size), 0, "Verifying");

		free_firmware_image(&image1);
		free_firmware_image(&image2);
	}

	/* Different images, compatible sections */
	{
		struct firmware_image image0 = load_image(IMAGE_MAIN, 0);
		struct firmware_image image1 = load_image(IMAGE_MAIN, 0);
		struct firmware_image image2 =
			load_image("tests/futility/data/images/image-skyrim.bin", 0);

		FmapAreaHeader *ah;
		fmap_find_by_name(image1.data, image1.size, image1.fmap_header, "RO_FRID", &ah);

		TEST_EQ(preserve_firmware_section(&image1, &image2, "RO_FRID"), 0,
			"Preserving RO_FRID");

		memset(image1.data + ah->area_offset, 0, ah->area_size);

		TEST_EQ(preserve_firmware_section(&image2, &image1, "RO_FRID"), 0,
			"Restoring RO_FRID");

		TEST_EQ(memcmp(image0.data, image1.data, image0.size), 0, "Verifying");

		free_firmware_image(&image0);
		free_firmware_image(&image1);
		free_firmware_image(&image2);
	}

	/* Different images, data gets truncated, should fail */
	{
		struct firmware_image image0 = load_image(IMAGE_MAIN, 0);
		struct firmware_image image1 =
			load_image(IMAGE_MAIN, 0); // RW_LEGACY around 2mib
		struct firmware_image image2 =
			load_image("tests/futility/data/images/image-skyrim.bin", 0); // 1.7mib

		FmapAreaHeader *ah;
		fmap_find_by_name(image1.data, image1.size, image1.fmap_header, "RW_LEGACY",
				  &ah);

		TEST_EQ(preserve_firmware_section(&image1, &image2, "RW_LEGACY"), 0,
			"Preserving RW_LEGACY");

		memset(image1.data + ah->area_offset, 0, ah->area_size);

		TEST_EQ(preserve_firmware_section(&image2, &image1, "RW_LEGACY"), 0,
			"Restoring RW_LEGACY");

		TEST_NEQ(memcmp(image0.data, image1.data, image0.size), 0, "Verifying");

		free_firmware_image(&image0);
		free_firmware_image(&image1);
		free_firmware_image(&image2);
	}
}

#endif

int main(int argc, char *argv[])
{
	test_temp_file(0);
	test_load_firmware_image(0);
	test_reload_firmware_image(0);
	test_system_firmware(0);
	test_firmware_sections(0);
	printf("\n\n==================================\n");
	TEST_SUCC(!gTestSuccess, "RESULT OF test_updater_utils: ");

	return !gTestSuccess;
}
