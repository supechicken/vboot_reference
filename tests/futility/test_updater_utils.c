/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define __USE_GNU

#include <stdio.h>
#include <stdlib.h>
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
	check_firmware_versions(&image);
	free_firmware_image(&image);

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

	const char *regions[1] = {FMAP_RW_LEGACY};
	uint8_t *ptr = fmap_find_by_name(cfg->image.data, cfg->image.size,
					 cfg->image.fmap_header, FMAP_RW_LEGACY, NULL);
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

	free_firmware_image(&image);
}

static void test_preserve_firmware_section(int _)
{
	struct firmware_image image_from, image_to;
	IF_FAIL(load_firmware_image(&image_from, IMAGE_MAIN, NULL) == 0,
		"Failed to load image");
	IF_FAIL(load_firmware_image(&image_to, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	TEST_EQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		"Preserving section");
	TEST_EQ(memcmp(image_from.data, image_to.data, image_from.size), 0,
		"Verifying section");

	FmapAreaHeader *ah;
	uint8_t *ptr = fmap_find_by_name(image_to.data, image_to.size, image_to.fmap_header,
					 FMAP_RW_LEGACY, &ah);

	strcpy(ah->area_name, "<invalid name>");
	TEST_NEQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		 "Preserving missing section");

	strcpy(ah->area_name, FMAP_RW_LEGACY);
	uint8_t byte = *(ptr + ah->area_size - 1); // byte to check truncation
	image_from.data[ah->area_offset + ah->area_size] = 255 ^ byte;
	ah->area_size--;
	TEST_EQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		"Preserving section (truncated)");
	TEST_EQ(*(ptr + ah->area_size), byte, "Verifying truncated section");

	IF_FAIL(reload_firmware_image(IMAGE_MAIN, &image_to) == 0, "Failed to load image");
	ptr = fmap_find_by_name(image_to.data, image_to.size, image_to.fmap_header,
				FMAP_RW_LEGACY, &ah);
	ah->area_size++;
	uint8_t *data = (uint8_t *)malloc(ah->area_size);
	memcpy(data, ptr, ah->area_size);
	for (int i = 0; i < ah->area_size; i++)
		data[i] ^= 255; // different then section contents

	TEST_NEQ(overwrite_section(&image_to, "<invalid section>", 0, ah->area_size, data), 0,
		 "Overwriting missing section");

	TEST_NEQ(overwrite_section(&image_to, FMAP_RW_LEGACY, 0, ah->area_size + 1, data), 0,
		 "Overwriting section and beyond");

	TEST_EQ(overwrite_section(&image_to, FMAP_RW_LEGACY, 0, ah->area_size, ptr), 0,
		"Overwriting section with same data");

	TEST_EQ(overwrite_section(&image_to, FMAP_RW_LEGACY, 0, ah->area_size, data), 0,
		"Overwriting section");
	TEST_EQ(memcmp(ptr, data, ah->area_size), 0, "Verifying section");

	free_firmware_image(&image_from);
	free_firmware_image(&image_to);
}

static void test_gbb_stuff(int _)
{
	struct firmware_image image;
	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	FmapAreaHeader *ah;
	uint8_t *ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, "GBB", &ah);

	strcpy(ah->area_name, "<invalid name>");
	TEST_PTR_EQ(get_firmware_rootkey_hash(&image), NULL,
		    "Getting firmware rootkey hash from missing GBB");
	TEST_PTR_EQ(find_gbb(&image), NULL, "Finding missing GBB");

	strcpy(ah->area_name, "GBB");
	memset(ptr, 0, ah->area_size);
	TEST_PTR_EQ(get_firmware_rootkey_hash(&image), NULL,
		    "Getting firmware rootkey hash from invalid GBB");
	TEST_PTR_EQ(find_gbb(&image), NULL, "Finding invalid GBB");

	IF_FAIL(reload_firmware_image(IMAGE_MAIN, &image) == 0, "Failed to load image");
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, "GBB", &ah);

	TEST_PTR_NEQ(get_firmware_rootkey_hash(&image), NULL, "Getting firmware rootkey hash");
	TEST_PTR_NEQ(find_gbb(&image), NULL, "Finding GBB");

	free_firmware_image(&image);
}

static void test_misc(int _)
{
	char *s1 = strdup("hello \n \t ");
	const char *pattern1 = NULL;
	const char *res1 = "hello";
	strip_string(s1, pattern1);
	TEST_EQ(strcmp(s1, res1), 0, "Strip NULL pattern");
	free(s1);

	char *s2 = strdup("helloABC");
	const char *pattern2 = "ABC";
	const char *res2 = "hello";
	strip_string(s2, pattern2);
	TEST_EQ(strcmp(s2, res2), 0, "Strip entire");
	free(s2);

	char *s3 = strdup("helloABC");
	const char *pattern3 = "AC";
	const char *res3 = "helloAB";
	strip_string(s3, pattern3);
	TEST_EQ(strcmp(s3, res3), 0, "Strip partial");
	free(s3);

	char *s4 = strdup("helloABC");
	const char *pattern4 = "B";
	const char *res4 = "helloABC";
	strip_string(s4, pattern4);
	TEST_EQ(strcmp(s4, res4), 0, "Strip no effect");
	free(s4);

	// save_file_from_stdin ?

	struct updater_config_arguments args = {0};
	struct updater_config *cfg = updater_new_config();
	struct dut_property *prop;

	IF_FAIL(cfg != NULL, "Failed to create updater config");
	IF_FAIL(updater_setup_config(cfg, &args) == 0, "Failed to setup updater config");
	IF_FAIL(load_firmware_image(&cfg->image, IMAGE_MAIN, NULL) == 0,
		"Failed to load image");

	prop = &cfg->dut_properties[DUT_PROP_WP_HW];
	prop->initialized = 0;
	prop = &cfg->dut_properties[DUT_PROP_WP_SW_AP];
	prop->initialized = 1;
	prop->value = 0;
	TEST_EQ(is_ap_write_protection_enabled(cfg), 0,
		"Cheking AP write protection HW=uninitialized SW=0");

	for (int mask = 0; mask < 4; mask++) {
		prop = &cfg->dut_properties[DUT_PROP_WP_HW];
		prop->initialized = 1;
		prop->value = mask & 1;
		prop = &cfg->dut_properties[DUT_PROP_WP_SW_AP];
		prop->initialized = 1;
		prop->value = mask & 2;
		TEST_EQ(is_ap_write_protection_enabled(cfg), mask == 3,
			format("Checking AP write protection HW=%d, SW=%d", mask & 1,
			       !!(mask & 2)));
	}

	prop = &cfg->dut_properties[DUT_PROP_WP_HW];
	prop->initialized = 0;
	prop = &cfg->dut_properties[DUT_PROP_WP_SW_EC];
	prop->initialized = 1;
	prop->value = 0;
	TEST_EQ(is_ec_write_protection_enabled(cfg), 0,
		"Cheking EC write protection HW=uninitialized SW=0");

	for (int mask = 0; mask < 4; mask++) {
		prop = &cfg->dut_properties[DUT_PROP_WP_HW];
		prop->initialized = 1;
		prop->value = mask & 1;
		prop = &cfg->dut_properties[DUT_PROP_WP_SW_EC];
		prop->initialized = 1;
		prop->value = mask & 2;
		TEST_EQ(is_ec_write_protection_enabled(cfg), mask == 3,
			format("Checking EC write protection HW=%d, SW=%d", mask & 1,
			       !!(mask & 2)));
	}

	updater_delete_config(cfg);

	char *res_shell = host_shell("echo test");
	TEST_EQ(strcmp(res_shell, "test"), 0, "Host shell echo");
	free(res_shell);

	res_shell = host_shell(")certainly_not_a_valid_thing");
	TEST_EQ(strcmp(res_shell, ""), 0, "Host shell invalid command");
	free(res_shell);
}

int main(int argc, char *argv[])
{
	test_temp_file(0);
	test_load_firmware_image(0);
	test_reload_firmware_image(0);
	test_system_firmware(0);
	test_firmware_sections(0);
	test_preserve_firmware_section(0);
	test_gbb_stuff(0);
	test_misc(0);

	return !gTestSuccess;
}
