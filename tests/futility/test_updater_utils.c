/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Hack because tests are linked with futility, so these symbols are defined twice. */
#define strip_string strip_string__UNIT_TESTS__
#define save_file_from_stdin save_file_from_stdin__UNIT_TESTS__
#define check_firmware_versions check_firmware_versions__UNIT_TESTS__
#define free_firmware_image free_firmware_image__UNIT_TESTS__
#define find_firmware_section find_firmware_section__UNIT_TESTS__
#define firmware_section_exists firmware_section_exists__UNIT_TESTS__
#define preserve_firmware_section preserve_firmware_section__UNIT_TESTS__
#define find_gbb find_gbb__UNIT_TESTS__
#define load_system_frid load_system_frid__UNIT_TESTS__
#define get_model_from_frid get_model_from_frid__UNIT_TESTS__
#define is_ap_write_protection_enabled is_ap_write_protection_enabled__UNIT_TESTS__
#define is_ec_write_protection_enabled is_ec_write_protection_enabled__UNIT_TESTS__
#define prepare_servo_control prepare_servo_control__UNIT_TESTS__
#define host_detect_servo host_detect_servo__UNIT_TESTS__
#define write_system_firmware write_system_firmware__UNIT_TESTS__
#define create_temp_file create_temp_file__UNIT_TESTS__
#define get_firmware_image_temp_file get_firmware_image_temp_file__UNIT_TESTS__
#define remove_all_temp_files remove_all_temp_files__UNIT_TESTS__
#define load_firmware_image load_firmware_image__UNIT_TESTS__
#define reload_firmware_image reload_firmware_image__UNIT_TESTS__
#define load_system_firmware load_system_firmware__UNIT_TESTS__
#define get_firmware_rootkey_hash get_firmware_rootkey_hash__UNIT_TESTS__
#define overwrite_section overwrite_section__UNIT_TESTS__

#include "unit_tests_common.h"
#include "updater_utils.c"

/* Note: `int _` parameter is necessary to prevent "Function prototype ..." errors. */

static void test_temp_file(int _)
{
	struct firmware_image image;
	struct tempfile head = {NULL, NULL};
	const char *file = create_temp_file(&head);

	TEST_PTR_NEQ(file, NULL, "Creating temp file normal");

	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");
	TEST_PTR_NEQ(get_firmware_image_temp_file(&image, &head), NULL,
		     "Getting temp file for image");

	image.size = 0;
	TEST_PTR_EQ(get_firmware_image_temp_file(&image, &head), NULL,
		    "Getting temp file for invalid image");

	remove_all_temp_files(&head);
	free_firmware_image(&image);
}

/* load_firmware_image calls parse_firmware_image, so here we will test both. */
static void test_load_firmware_image(int _)
{
	struct firmware_image image;
	struct u_archive *archive;

	TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, NULL), 0, "Load normal image");
	check_firmware_versions(&image);
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, NULL, NULL), IMAGE_READ_FAILURE,
		"Load NULL filename");

	TEST_EQ(load_firmware_image(&image, "", NULL), IMAGE_READ_FAILURE,
		"Load empty filename");

	TEST_EQ(load_firmware_image(&image, FILE_NONEXISTENT, NULL), IMAGE_READ_FAILURE,
		"Load invalid file");

	archive = archive_open(ARCHIVE);
	IF_FAIL(archive != NULL, "Failed to open archive");

	TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, archive), 0, "Load from archive");
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, FILE_NONEXISTENT, archive), IMAGE_READ_FAILURE,
		"Load invalid file from archive");
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FMAP, NULL), IMAGE_PARSE_FAILURE,
		"Missing FMAP");

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FRID, NULL), IMAGE_PARSE_FAILURE,
		"Missing RO_FRID");

	TEST_EQ(load_firmware_image(&image, IMAGE_MISSING_FWID, NULL), IMAGE_PARSE_FAILURE,
		"Missing RW_FWID*");

	archive_close(archive);
	free_firmware_image(&image);
}

static void test_firmware_version(int _)
{
	struct firmware_image image;
	char *version;
	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	TEST_NEQ(load_firmware_version(&image, NULL, &version), 0,
		 "Load firmware version NULL section");
	TEST_EQ(strcmp(version, ""), 0, "Verifying");
	free(version);

	TEST_NEQ(load_firmware_version(&image, "<invalid section>", &version), 0,
		 "Load firmware version invalid section");
	TEST_EQ(strcmp(version, ""), 0, "Verifying");
	free(version);

	TEST_EQ(load_firmware_version(&image, FMAP_RO_FRID, &version), 0,
		"Load firmware version valid");
	TEST_NEQ(strcmp(version, ""), 0, "Verifying");
	free(version);

	free_firmware_image(&image);
}

static void test_reload_firmware_image(int _)
{
	struct firmware_image image;

	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");
	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading image");
	free_firmware_image(&image);

	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading unloaded image");
	free_firmware_image(&image);
}

static void test_system_firmware(int _)
{
	struct updater_config_arguments args = {0};
	struct updater_config *cfg = updater_new_config();
	const char *programmer;
	const char *regions[1] = {FMAP_RW_LEGACY};
	uint8_t *ptr;
	int value;
	uint64_t offset;

	TEST_PTR_NEQ(cfg, NULL, "Creating updater config");

	args.use_flash = 1;
	args.image = (char *)IMAGE_MAIN;
	copy_image(IMAGE_MAIN);
	args.emulation = (char *)IMAGE_TEMP;

	TEST_EQ(updater_setup_config(cfg, &args), 0, "Setting up config");
	cfg->quirks[QUIRK_EXTRA_RETRIES].value = 2;

	programmer = cfg->image_current.programmer;
	cfg->image_current.programmer = "<invalid programmer>";
	TEST_NEQ(load_system_firmware(cfg, &cfg->image_current), 0, "Invalid programmer");
	cfg->image_current.programmer = programmer;

	TEST_EQ(write_system_firmware(cfg, &cfg->image, NULL, 0), 0,
		"Writing system firmware (entire)");
	TEST_EQ(load_system_firmware(cfg, &cfg->image_current), 0, "Loading system firmware");
	TEST_TRUE(cfg->image.size == cfg->image_current.size, "Verifying size");
	TEST_EQ(memcmp(cfg->image.data, cfg->image_current.data, cfg->image.size), 0,
		"Verifying contents");

	/* Change one byte to verify that the data gets written. */
	ptr = fmap_find_by_name(cfg->image.data, cfg->image.size, cfg->image.fmap_header,
				FMAP_RW_LEGACY, NULL);
	ptr[0] ^= 255; /* This will change ptr to a different value. */
	value = ptr[0];
	offset = ptr - cfg->image.data;
	TEST_EQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		"Writing system firmware (partial)");
	IF_FAIL(load_system_firmware(cfg, &cfg->image_current) == 0,
		"Failed to load system firmware");
	TEST_EQ(cfg->image_current.data[offset], value, "Verifying written region");

	regions[0] = "<invalid region>";
	TEST_NEQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		 "Writing invalid region");

	updater_delete_config(cfg);
}

static void test_programmer(int _)
{
	struct firmware_image image1, image2;
	IF_FAIL(load_firmware_image(&image1, IMAGE_MAIN, NULL) == 0, "Failed to load image");
	IF_FAIL(load_firmware_image(&image2, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	image1.programmer = image2.programmer = "<same programmer>";
	TEST_EQ(is_the_same_programmer(&image1, &image2), 1, "Test programmer same");

	image2.programmer = strdup(image1.programmer);
	TEST_EQ(is_the_same_programmer(&image1, &image2), 1, "Test programmer same value");
	free((char *)image2.programmer);

	image1.programmer = "<another programmer>";
	TEST_EQ(is_the_same_programmer(&image1, &image2), 0, "Test programmer different");

	image1.programmer = NULL;
	TEST_EQ(is_the_same_programmer(&image1, &image2), 0,
		"Test programmer different (NULL)");

	image2.programmer = NULL;
	TEST_EQ(is_the_same_programmer(&image1, &image2), 1, "Test programmer same (NULL)");

	free_firmware_image(&image1);
	free_firmware_image(&image2);
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
	FmapAreaHeader *ah;
	uint8_t *ptr, *data, byte;

	IF_FAIL(load_firmware_image(&image_from, IMAGE_MAIN, NULL) == 0,
		"Failed to load image");
	IF_FAIL(load_firmware_image(&image_to, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	TEST_EQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		"Preserving section");
	TEST_EQ(memcmp(image_from.data, image_to.data, image_from.size), 0,
		"Verifying section");

	ptr = fmap_find_by_name(image_to.data, image_to.size, image_to.fmap_header,
				FMAP_RW_LEGACY, &ah);

	strcpy(ah->area_name, "<invalid name>");
	TEST_NEQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		 "Preserving invalid section");

	/* Modify last byte to check that it doesn't get written because
	 * preserve_firmware_section will truncate the last byte. */
	strcpy(ah->area_name, FMAP_RW_LEGACY);
	byte = *(ptr + ah->area_size - 1); /* Last byte. */
	/* A different byte to write. Should not be written. */
	image_from.data[ah->area_offset + ah->area_size] = 255 ^ byte;
	ah->area_size--;
	TEST_EQ(preserve_firmware_section(&image_from, &image_to, FMAP_RW_LEGACY), 0,
		"Preserving section (truncated)");
	TEST_EQ(*(ptr + ah->area_size), byte, "Verifying truncated section");

	IF_FAIL(reload_firmware_image(IMAGE_MAIN, &image_to) == 0, "Failed to load image");
	ptr = fmap_find_by_name(image_to.data, image_to.size, image_to.fmap_header,
				FMAP_RW_LEGACY, &ah);
	ah->area_size++;
	data = (uint8_t *)malloc(ah->area_size);
	memcpy(data, ptr, ah->area_size);
	for (int i = 0; i < ah->area_size; i++)
		data[i] ^= 255; /* Some different data. */

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

static void test_gbb(int _)
{
	struct firmware_image image;
	FmapAreaHeader *ah;
	uint8_t *ptr;

	IF_FAIL(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0, "Failed to load image");

	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, "GBB", &ah);

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
	char *s1, *s2, *s3, *s4;
	const char *pattern1, *pattern2, *pattern3, *pattern4;
	const char *res1, *res2, *res3, *res4;
	char *res_shell;
	struct updater_config_arguments args = {0};
	struct updater_config *cfg;
	struct dut_property *prop;

	s1 = strdup("hello \n \t ");
	pattern1 = NULL;
	res1 = "hello";
	strip_string(s1, pattern1);
	TEST_EQ(strcmp(s1, res1), 0, "Strip NULL pattern");
	free(s1);

	s2 = strdup("helloABC");
	pattern2 = "ABC";
	res2 = "hello";
	strip_string(s2, pattern2);
	TEST_EQ(strcmp(s2, res2), 0, "Strip entire");
	free(s2);

	s3 = strdup("helloABC");
	pattern3 = "AC";
	res3 = "helloAB";
	strip_string(s3, pattern3);
	TEST_EQ(strcmp(s3, res3), 0, "Strip partial");
	free(s3);

	s4 = strdup("helloABC");
	pattern4 = "B";
	res4 = "helloABC";
	strip_string(s4, pattern4);
	TEST_EQ(strcmp(s4, res4), 0, "Strip no effect");
	free(s4);

	TEST_NEQ(save_file_from_stdin(FILE_READONLY), 0, "Save file from stdin readonly");

	cfg = updater_new_config();

	IF_FAIL(cfg != NULL, "Failed to create updater config");
	IF_FAIL(updater_setup_config(cfg, &args) == 0, "Failed to setup updater config");
	IF_FAIL(load_firmware_image(&cfg->image, IMAGE_MAIN, NULL) == 0,
		"Failed to load image");

	/* Test uninitialized and initialized. */
	prop = &cfg->dut_properties[DUT_PROP_WP_HW];
	prop->initialized = 0;
	prop = &cfg->dut_properties[DUT_PROP_WP_SW_AP];
	prop->initialized = 1;
	prop->value = 0;
	TEST_EQ(is_ap_write_protection_enabled(cfg), 0,
		"Cheking AP write protection HW=uninitialized SW=0");

	/* Test all initialized cases. */
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

	/* Test uninitialized and initialized. */
	prop = &cfg->dut_properties[DUT_PROP_WP_HW];
	prop->initialized = 0;
	prop = &cfg->dut_properties[DUT_PROP_WP_SW_EC];
	prop->initialized = 1;
	prop->value = 0;
	TEST_EQ(is_ec_write_protection_enabled(cfg), 0,
		"Cheking EC write protection HW=uninitialized SW=0");

	/* Test all initialized cases. */
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

	res_shell = host_shell("echo test");
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
	test_firmware_version(0);
	test_reload_firmware_image(0);
	test_system_firmware(0);
	test_programmer(0);
	test_firmware_sections(0);
	test_preserve_firmware_section(0);
	test_gbb(0);
	test_misc(0);

	return !gTestSuccess;
}
