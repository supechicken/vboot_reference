/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#define __USE_GNU

#include <stdlib.h>
#include "futility.h"
#include "updater.h"
#include "2struct.h"
#include "common/tests.h"

#include "updater_utils.c"

#define DATA_PATH "tests/futility/data_copy/"
#define IMAGE_MAIN DATA_PATH "image.bin"
#define ARCHIVE DATA_PATH "images.zip"
#define FILE_NONEXISTENT DATA_PATH "nonexistent"
#define FILE_READONLY DATA_PATH "read-only"

/* When a custom image needs to be created, it will be written to this file. It also acts as a
 * temporary file. */
#define TARGET DATA_PATH "target"

static char __format_buf[4096];
#define format(...)                                                                            \
	({                                                                                     \
		snprintf(__format_buf, sizeof(__format_buf), __VA_ARGS__);                     \
		__format_buf;                                                                  \
	})

#define ASSERT(value)                                                                          \
	{                                                                                      \
		if ((value) != 1) {                                                            \
			fprintf(stderr, "FAILED: %s:%d: " #value ": tests failed.\n",          \
				__func__, __LINE__);                                           \
			exit(1);                                                               \
		}                                                                              \
	}

static void create_image_missing_fmap(void)
{
	struct firmware_image image;
	FmapAreaHeader *ah;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	ASSERT(image.fmap_header != NULL);
	ASSERT(fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RO_FMAP,
				 &ah) != NULL);
	memset(image.data + ah->area_offset, 0, ah->area_size);

	ASSERT(vb2_write_file(TARGET, image.data, image.size) == VB2_SUCCESS);
	free_firmware_image(&image);
}

static void create_image_missing_ro_frid_in_fmap(void)
{
	struct firmware_image image;
	FmapAreaHeader *ah;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	ASSERT(image.fmap_header != NULL);
	ASSERT(fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RO_FRID,
				 &ah) != NULL);
	ah->area_name[0] = '\0';

	ASSERT(vb2_write_file(TARGET, image.data, image.size) == VB2_SUCCESS);
	free_firmware_image(&image);
}

static void create_image_missing_rw_fwid_in_fmap(void)
{
	struct firmware_image image;
	FmapAreaHeader *ah;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	ASSERT(image.fmap_header != NULL);
	if (fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_FWID_A, &ah) !=
	    NULL)
		ah->area_name[0] = '\0';
	if (fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_FWID_B, &ah) !=
	    NULL)
		ah->area_name[0] = '\0';
	if (fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_FWID, &ah) !=
	    NULL)
		ah->area_name[0] = '\0';

	ASSERT(vb2_write_file(TARGET, image.data, image.size) == VB2_SUCCESS);
	free_firmware_image(&image);
}

static void copy_image(const char *path)
{
	uint8_t *ptr;
	uint32_t size;

	ASSERT(vb2_read_file(path, &ptr, &size) == VB2_SUCCESS);
	ASSERT(vb2_write_file(TARGET, ptr, size) == VB2_SUCCESS);
}

static void test_temp_file(void)
{
	struct firmware_image image;
	struct tempfile head;
	memset(&head, 0, sizeof(head));
	const char *file = create_temp_file(&head);

	TEST_PTR_NEQ(file, NULL, "Creating temp file normal");

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	TEST_PTR_NEQ(get_firmware_image_temp_file(&image, &head), NULL,
		     "Getting temp file for image");

	remove_all_temp_files(&head);
	free_firmware_image(&image);
}

/* Both `load_firmware_image` and `parse_firmware_image` are tested here.  */
static void test_load_firmware_image(void)
{
	struct firmware_image image;
	struct u_archive *archive;
	uint8_t *ref_ptr;
	uint32_t ref_size;

	ASSERT(vb2_read_file(IMAGE_MAIN, &ref_ptr, &ref_size) == VB2_SUCCESS);

	TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, NULL), 0, "Load normal image");
	TEST_EQ(ref_size == image.size, 1, "Verifying size");
	TEST_EQ(memcmp(ref_ptr, image.data, ref_size), 0, "Verifying data");
	TEST_PTR_NEQ(image.fmap_header, NULL, "Verifying FMAP");
	check_firmware_versions(&image);
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, NULL, NULL), IMAGE_READ_FAILURE,
		"Load NULL filename");
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, "", NULL), IMAGE_READ_FAILURE,
		"Load empty filename");
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, FILE_NONEXISTENT, NULL), IMAGE_READ_FAILURE,
		"Load invalid file");
	free_firmware_image(&image);

	archive = archive_open(ARCHIVE);
	ASSERT(archive != NULL);

	TEST_EQ(load_firmware_image(&image, IMAGE_MAIN, archive), 0, "Load from archive");
	TEST_EQ(ref_size == image.size, 1, "Verifying size");
	TEST_EQ(memcmp(ref_ptr, image.data, ref_size), 0, "Verifying data");
	TEST_PTR_NEQ(image.fmap_header, NULL, "Verifying FMAP");
	check_firmware_versions(&image);
	free_firmware_image(&image);

	TEST_EQ(load_firmware_image(&image, FILE_NONEXISTENT, archive), IMAGE_READ_FAILURE,
		"Load invalid file from archive");
	free_firmware_image(&image);

	archive_close(archive);
	free_firmware_image(&image);
}

static void test_parse_firmware_image(void)
{
	struct firmware_image image;

	memset(&image, 0, sizeof(image));
	ASSERT(vb2_read_file(IMAGE_MAIN, &image.data, &image.size) == VB2_SUCCESS);
	TEST_EQ(parse_firmware_image(&image), IMAGE_LOAD_SUCCESS, "Parse firmware image valid");
	TEST_PTR_EQ(fmap_find(image.data, image.size), image.fmap_header, "Verifying FMAP");
	free_firmware_image(&image);

	memset(&image, 0, sizeof(image));
	create_image_missing_fmap();
	ASSERT(vb2_read_file(TARGET, &image.data, &image.size) == VB2_SUCCESS);
	TEST_EQ(parse_firmware_image(&image), IMAGE_PARSE_FAILURE,
		"Parse firmware image missing FMAP");
	free_firmware_image(&image);

	memset(&image, 0, sizeof(image));
	create_image_missing_ro_frid_in_fmap();
	ASSERT(vb2_read_file(TARGET, &image.data, &image.size) == VB2_SUCCESS);
	TEST_EQ(parse_firmware_image(&image), IMAGE_PARSE_FAILURE,
		"Parse firmware image missing RO_FRID");
	free_firmware_image(&image);

	memset(&image, 0, sizeof(image));
	create_image_missing_rw_fwid_in_fmap();
	ASSERT(vb2_read_file(TARGET, &image.data, &image.size) == VB2_SUCCESS);
	TEST_EQ(parse_firmware_image(&image), IMAGE_PARSE_FAILURE,
		"Parse firmware image missing RW_FWID");
	free_firmware_image(&image);
}

static void test_firmware_version(void)
{
	struct firmware_image image;
	FmapAreaHeader *ah;
	char *version;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);

	TEST_NEQ(load_firmware_version(&image, NULL, &version), 0,
		 "Load firmware version NULL section");
	TEST_STR_EQ(version, "", "Verifying");
	free(version);

	TEST_NEQ(load_firmware_version(&image, "<invalid section>", &version), 0,
		 "Load firmware version invalid section");
	TEST_STR_EQ(version, "", "Verifying");
	free(version);

	TEST_EQ(load_firmware_version(&image, FMAP_RO_FRID, &version), 0,
		"Load firmware version valid");
	TEST_STR_NEQ(version, "", "Verifying");
	free(version);

	/* It would be difficult to overwrite the cbfs file without cbfstool (which is not
	 * available on some boards...), so we just set the entire section to zero. */
	ASSERT(fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_FW_MAIN_A,
				 &ah) != NULL);
	memset(image.data + ah->area_offset, 0, ah->area_size);
	version = load_ecrw_version(&image, TARGET, FMAP_RW_FW_MAIN_A);
	TEST_STR_EQ(version, "", "Load ECRW version invalid");
	free(version);

	free_firmware_image(&image);
}

static void test_reload_firmware_image(void)
{
	struct firmware_image image;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading image");
	free_firmware_image(&image);

	TEST_EQ(reload_firmware_image(IMAGE_MAIN, &image), 0, "Reloading unloaded image");
	free_firmware_image(&image);
}

static void test_system_firmware(void)
{
	struct updater_config_arguments args;
	memset(&args, 0, sizeof(args));
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
	args.emulation = (char *)TARGET;

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
	ptr[0] ^= 255; /* This will change the first byte to a different value. */
	value = ptr[0];
	offset = ptr - cfg->image.data;
	TEST_EQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		"Writing system firmware (partial)");
	ASSERT(load_system_firmware(cfg, &cfg->image_current) == 0);
	TEST_EQ(cfg->image_current.data[offset], value, "Verifying written region");

	regions[0] = "<invalid region>";
	TEST_NEQ(write_system_firmware(cfg, &cfg->image, regions, ARRAY_SIZE(regions)), 0,
		 "Writing invalid region");

	updater_delete_config(cfg);
}

static void test_programmer(void)
{
	struct firmware_image image1, image2;
	ASSERT(load_firmware_image(&image1, IMAGE_MAIN, NULL) == 0);
	ASSERT(load_firmware_image(&image2, IMAGE_MAIN, NULL) == 0);

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

static void test_firmware_sections(void)
{
	struct firmware_image image;
	struct firmware_section section;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);

	TEST_EQ(find_firmware_section(&section, &image, "RO_FRID"), 0, "Find firmware section");
	TEST_EQ(firmware_section_exists(&image, "RO_FRID"), 1, "Firmware section exists");

	memset(image.data, 0, image.size);

	TEST_NEQ(find_firmware_section(&section, &image, "RO_FRID"), 0,
		 "Find missing firmware section");
	TEST_NEQ(firmware_section_exists(&image, "RO_FRID"), 1,
		 "Firmware section doesn't exist");

	free_firmware_image(&image);
}

static void test_preserve_firmware_section(void)
{
	struct firmware_image image_from, image_to;
	FmapAreaHeader *ah;
	uint8_t *ptr, *data, byte;

	ASSERT(load_firmware_image(&image_from, IMAGE_MAIN, NULL) == 0);
	ASSERT(load_firmware_image(&image_to, IMAGE_MAIN, NULL) == 0);

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

	ASSERT(reload_firmware_image(IMAGE_MAIN, &image_to) == 0);
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

static void test_gbb(void)
{
	struct firmware_image image;
	FmapAreaHeader *ah;
	uint8_t *ptr;

	ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);

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

	ASSERT(reload_firmware_image(IMAGE_MAIN, &image) == 0);
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, "GBB", &ah);

	TEST_PTR_NEQ(get_firmware_rootkey_hash(&image), NULL, "Getting firmware rootkey hash");
	TEST_PTR_NEQ(find_gbb(&image), NULL, "Finding GBB");

	free_firmware_image(&image);
}

static void test_misc(void)
{
	char *s1, *s2, *s3, *s4;
	const char *pattern1, *pattern2, *pattern3, *pattern4;
	const char *res1, *res2, *res3, *res4;
	char *res_shell;
	struct updater_config_arguments args;
	memset(&args, 0, sizeof(args));
	struct updater_config *cfg;
	struct dut_property *prop;
	char *model;

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

	ASSERT(cfg != NULL);
	ASSERT(updater_setup_config(cfg, &args) == 0);
	ASSERT(load_firmware_image(&cfg->image, IMAGE_MAIN, NULL) == 0);

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
	TEST_STR_EQ(res_shell, "test", "Host shell echo");
	free(res_shell);

	res_shell = host_shell(")certainly_not_a_valid_thing");
	TEST_STR_EQ(res_shell, "", "Host shell invalid command");
	free(res_shell);

	model = get_model_from_frid("some.frid");
	TEST_STR_EQ(model, "some", "Get model from frid valid");
	free(model);

	model = get_model_from_frid("somefrid");
	TEST_PTR_EQ(model, NULL, "Get model from frid no dot");
	free(model);
}

int main(int argc, char *argv[])
{
	test_temp_file();
	test_load_firmware_image();
	test_parse_firmware_image();
	test_firmware_version();
	test_reload_firmware_image();
	test_system_firmware();
	test_programmer();
	test_firmware_sections();
	test_preserve_firmware_section();
	test_gbb();
	test_misc();

	return !gTestSuccess;
}
