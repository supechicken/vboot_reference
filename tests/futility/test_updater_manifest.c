/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "unit_tests.h"
#include "host_misc.h"
#include "updater_manifest.c"

#define IMAGE_BAD			GET_WORK_COPY_TEST_DATA_FILE_PATH("image-bad.bin")
#define IMAGE_MAIN			GET_WORK_COPY_TEST_DATA_FILE_PATH("image.bin")
#define NONEXISTENT_FILE		GET_WORK_COPY_TEST_DATA_FILE_PATH("nonexistent")
#define SMALL_FILE			GET_WORK_COPY_TEST_DATA_FILE_PATH("small-file")
#define ROOTKEY_PATCH			"keyset/rootkey.testmodel"
#define VBLOCK_A_PATCH			"keyset/vblock_A.testmodel"
#define VBLOCK_B_PATCH			"keyset/vblock_B.testmodel"
#define RO_GSCVD_PATCH			"keyset/gscvd.testmodel"
#define FIRMWARE_ARCHIVE		GET_WORK_COPY_TEST_DATA_FILE_PATH("firmware")
#define LEGACY_ARCHIVE			GET_WORK_COPY_TEST_DATA_FILE_PATH("legacy_firmware")
#define EMPTY_FOLDER			GET_WORK_COPY_TEST_DATA_FILE_PATH("empty_folder")
#define SIGNER_CONFIG			FIRMWARE_ARCHIVE "/signer_config.csv"
#define SIGNER_CONFIG_INVALID_HEADER	"signer-config-invalid-header.csv"
#define SIGNER_CONFIG_INVALID_ENTRY	"signer-config-invalid-entry.csv"
#define SIGNER_CONFIG_ONLY_BASE_MODELS	"signer-config-only-base-models.csv"
#define SIGNER_CONFIG_WITH_CUSTOM_LABEL "signer-config-with-custom-label.csv"

static enum unit_result prepare_test_data(void)
{
	UNIT_TEST_BEGIN;

	FmapAreaHeader *ah = NULL;
	uint8_t *ptr = NULL;
	uint8_t *data = NULL;
	uint32_t len = 0;
	struct firmware_image image = {0};
	const struct vb2_gbb_header *gbb_header = NULL;

	UNIT_ASSERT(system("rm -rf " WORK_COPY_TEST_DATA_DIR) == 0);
	UNIT_ASSERT(system("mkdir -p " WORK_COPY_TEST_DATA_DIR) == 0);

	UNIT_ASSERT(futil_copy_file(GET_SOURCE_TEST_DATA_FILE_PATH("image-steelix.bin"),
				    IMAGE_MAIN) != -1);
	UNIT_ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);

	len = 16 * 1024;
	data = calloc(1, len);
	UNIT_ASSERT(vb2_write_file(IMAGE_BAD, data, len) == VB2_SUCCESS);
	len = 1024;
	UNIT_ASSERT(vb2_write_file(SMALL_FILE, data, len) == VB2_SUCCESS);
	free(data);
	data = NULL;

	UNIT_ASSERT(system("mkdir " GET_WORK_COPY_TEST_DATA_FILE_PATH("keyset")) == 0);

	/* Rootkey patch. */
	gbb_header = find_gbb(&image);
	UNIT_ASSERT(gbb_header != NULL);
	memset((uint8_t *)gbb_header + gbb_header->rootkey_offset, 0,
	       gbb_header->rootkey_offset);
	UNIT_ASSERT(vb2_write_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(ROOTKEY_PATCH), gbb_header,
				   gbb_header->rootkey_size) == VB2_SUCCESS);

	/* VBLOCK_A patch. */
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_VBLOCK_A,
				&ah);
	UNIT_ASSERT(ptr != NULL);
	memset(ptr, 0, ah->area_size);
	UNIT_ASSERT(vb2_write_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(VBLOCK_A_PATCH), ptr,
				   ah->area_size) == VB2_SUCCESS);

	/* VBLOCK_B patch. */
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RW_VBLOCK_B,
				&ah);
	UNIT_ASSERT(ptr != NULL);
	memset(ptr, 0, ah->area_size);
	UNIT_ASSERT(vb2_write_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(VBLOCK_B_PATCH), ptr,
				   ah->area_size) == VB2_SUCCESS);

	/* RO_GSCVD patch. */
	ptr = fmap_find_by_name(image.data, image.size, image.fmap_header, FMAP_RO_GSCVD, &ah);
	UNIT_ASSERT(ptr != NULL);
	memset(ptr, 0, ah->area_size);
	UNIT_ASSERT(vb2_write_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(RO_GSCVD_PATCH), ptr,
				   ah->area_size) == VB2_SUCCESS);

	UNIT_ASSERT(system("mkdir -p " FIRMWARE_ARCHIVE "/model") == 0);
	UNIT_ASSERT(vb2_write_file(FIRMWARE_ARCHIVE "/image-model.bin", "a", 1) == VB2_SUCCESS);
	UNIT_ASSERT(vb2_write_file(FIRMWARE_ARCHIVE "/model/ec.bin", "a", 1) == VB2_SUCCESS);

	UNIT_ASSERT(futil_copy_file(
			    GET_SOURCE_TEST_DATA_FILE_PATH(SIGNER_CONFIG_INVALID_HEADER),
			    GET_WORK_COPY_TEST_DATA_FILE_PATH(SIGNER_CONFIG_INVALID_HEADER)) !=
		    -1);
	UNIT_ASSERT(futil_copy_file(GET_SOURCE_TEST_DATA_FILE_PATH(SIGNER_CONFIG_INVALID_ENTRY),
				    GET_WORK_COPY_TEST_DATA_FILE_PATH(
					    SIGNER_CONFIG_INVALID_ENTRY)) != -1);
	UNIT_ASSERT(
		futil_copy_file(GET_SOURCE_TEST_DATA_FILE_PATH(SIGNER_CONFIG_ONLY_BASE_MODELS),
				GET_WORK_COPY_TEST_DATA_FILE_PATH(
					SIGNER_CONFIG_ONLY_BASE_MODELS)) != -1);
	UNIT_ASSERT(
		futil_copy_file(GET_SOURCE_TEST_DATA_FILE_PATH(SIGNER_CONFIG_WITH_CUSTOM_LABEL),
				GET_WORK_COPY_TEST_DATA_FILE_PATH(
					SIGNER_CONFIG_WITH_CUSTOM_LABEL)) != -1);

	UNIT_ASSERT(system("mkdir " GET_WORK_COPY_TEST_DATA_FILE_PATH("legacy_firmware")) == 0);
	UNIT_ASSERT(system("mkdir " GET_WORK_COPY_TEST_DATA_FILE_PATH("empty_folder")) == 0);

unit_cleanup:
	free_firmware_image(&image);
	free(data);
	UNIT_TEST_RETURN;
}

static int foo_convert(int c)
{
	return c + 1;
}

static void test_str_convert(void)
{
	char *s = strdup("abcdef");
	str_convert(s, foo_convert);
	TEST_STR_EQ(s, "bcdefg", "str_convert");
}

/* vpd command is not available on gLinux. Also we need to mock vpd_get_value to test
 * get_custom_label_tag, as well as manifest_find_custom_label_model */
enum host_shell_switch {
	SHELL_RETURN_TAG1,
	SHELL_RETURN_TAG2,
	SHELL_RETURN_NULL,
	SHELL_RETURN_TAG3_LABEL,
	SHELL_RETURN_MODEL
};
static enum host_shell_switch vpd_get_value_switch = SHELL_RETURN_NULL;
char *host_shell(const char *command)
{
	if (strstr(command, "unit_tests_good_key") != NULL)
		return (char *)"good_value";
	else if (vpd_get_value_switch == SHELL_RETURN_TAG1 &&
		 strstr(command, VPD_CUSTOM_LABEL_TAG))
		return strdup("tag1");
	else if (vpd_get_value_switch == SHELL_RETURN_TAG2 &&
		 strstr(command, VPD_CUSTOM_LABEL_TAG_LEGACY))
		return strdup("tag2");
	else if (vpd_get_value_switch == SHELL_RETURN_TAG3_LABEL &&
		 strstr(command, VPD_CUSTOMIZATION_ID))
		return strdup("tag3-label");
	else if (vpd_get_value_switch == SHELL_RETURN_MODEL)
		return strdup("model");
	else
		return NULL;
}

static void test_vpd_get_value(void)
{
	TEST_STR_EQ(vpd_get_value(NONEXISTENT_FILE, "unit_tests_good_key"), "good_value",
		    "VPD get value good key");
	TEST_PTR_EQ(vpd_get_value(NONEXISTENT_FILE, "unit_tests_bad_key"), NULL,
		    "VPD get value bad key");
}

static enum unit_result test_change_gbb_rootkey(void)
{
	UNIT_TEST_BEGIN;
	struct firmware_image image = {0};
	struct vb2_gbb_header *gbb = NULL;
	size_t rootkey_len = 0;
	uint8_t *new_rootkey = NULL;

	UNIT_ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	gbb = (struct vb2_gbb_header *)find_gbb(&image);
	UNIT_ASSERT(gbb != NULL);

	rootkey_len = gbb->rootkey_size;
	new_rootkey = malloc(rootkey_len);
	memset(new_rootkey, 0x7, rootkey_len); /* Some data. */

	TEST_EQ(change_gbb_rootkey(&image, NULL, new_rootkey, rootkey_len), 0,
		"Change gbb valid");
	TEST_EQ(memcmp((uint8_t *)gbb + gbb->rootkey_offset, new_rootkey, rootkey_len), 0,
		"    Verifying");

	rootkey_len--;
	TEST_EQ(change_gbb_rootkey(&image, NULL, new_rootkey, rootkey_len), 0,
		"Change gbb smaller rootkey");
	TEST_EQ(memcmp((uint8_t *)gbb + gbb->rootkey_offset, new_rootkey, rootkey_len), 0,
		"    Verifying");
	TEST_EQ(*((uint8_t *)gbb + gbb->rootkey_offset + rootkey_len), 0, "    Verifying");

	rootkey_len += 2;
	TEST_NEQ(change_gbb_rootkey(&image, NULL, new_rootkey, rootkey_len), 0,
		 "Change gbb too large");

	gbb->signature[0] = '\0';
	TEST_NEQ(change_gbb_rootkey(&image, NULL, new_rootkey, rootkey_len), 0,
		 "Change gbb missing gbb");

unit_cleanup:
	free_firmware_image(&image);
	free(new_rootkey);
	UNIT_TEST_RETURN;
}

static enum unit_result test_change_section(void)
{
	UNIT_TEST_BEGIN;
	struct firmware_image image = {0};
	struct firmware_section section = {0};
	size_t data_len = 0;
	uint8_t *data = NULL;
	FmapAreaHeader *ah = NULL;

	UNIT_ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	UNIT_ASSERT(find_firmware_section(&section, &image, FMAP_RW_LEGACY) == 0);
	data_len = section.size;
	data = malloc(data_len);

	memset(data, 0x7, data_len); /* Some data. */

	TEST_EQ(change_section(&image, FMAP_RW_LEGACY, data, data_len), 0,
		"Change section valid");
	TEST_EQ(memcmp(section.data, data, data_len), 0, "    Verifying");

	data_len--;

	TEST_EQ(change_section(&image, FMAP_RW_LEGACY, data, data_len), 0,
		"Change section smaller");
	TEST_EQ(memcmp(section.data, data, data_len), 0, "    Verifying");
	TEST_EQ(section.data[data_len], 0xff, "    Verifying");

	data_len += 2;
	TEST_NEQ(change_section(&image, FMAP_RW_LEGACY, data, data_len), 0,
		 "Change section too large");

	fmap_find_by_name(NULL, 0, image.fmap_header, FMAP_RW_LEGACY, &ah);
	UNIT_ASSERT(ah != NULL);
	ah->area_name[0] = '\0';
	TEST_NEQ(change_section(&image, FMAP_RW_LEGACY, data, data_len), 0,
		 "Change section missing");

unit_cleanup:
	free_firmware_image(&image);
	free(data);
	UNIT_TEST_RETURN;
}

static int foo_apply(struct firmware_image *image, const char *section, const uint8_t *data,
		     uint32_t len)
{
	if (!strcmp(section, "GOOD_SECTION"))
		return 0;
	return -1;
}

static void test_apply_key_file(void)
{
	TEST_NEQ(apply_key_file(NULL, NONEXISTENT_FILE, NULL, "", foo_apply), 0,
		 "Apply key file nonexistent file");
	TEST_NEQ(apply_key_file(NULL, SMALL_FILE, NULL, "BAD_SECTION", foo_apply), 0,
		 "Apply key file apply failed");
	TEST_EQ(apply_key_file(NULL, SMALL_FILE, NULL, "GOOD_SECTION", foo_apply), 0,
		"Apply key file valid");
}

static enum unit_result test_model_patches(void)
{
	UNIT_TEST_BEGIN;
	struct firmware_image image = {0};
	struct u_archive *ar = NULL;
	int fd1 = -1, fd2 = -1, fd3 = -1, fd4 = -1;
	uint8_t *rootkey = NULL, *vblock_a = NULL, *vblock_b = NULL, *ro_gscvd = NULL;
	uint32_t rootkey_size = 0, vblock_a_size = 0, vblock_b_size = 0, ro_gscvd_size = 0;
	struct model_config model = {0};
	struct vb2_gbb_header *gbb = NULL;

	UNIT_ASSERT(load_firmware_image(&image, IMAGE_MAIN, NULL) == 0);
	ar = archive_open(WORK_COPY_TEST_DATA_DIR);
	UNIT_ASSERT(ar != NULL);

	UNIT_ASSERT(futil_open_and_map_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(ROOTKEY_PATCH),
					    &fd1, FILE_RO, &rootkey,
					    &rootkey_size) == FILE_ERR_NONE);
	UNIT_ASSERT(futil_open_and_map_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(VBLOCK_A_PATCH),
					    &fd2, FILE_RO, &vblock_a,
					    &vblock_a_size) == FILE_ERR_NONE);
	UNIT_ASSERT(futil_open_and_map_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(VBLOCK_B_PATCH),
					    &fd3, FILE_RO, &vblock_b,
					    &vblock_b_size) == FILE_ERR_NONE);
	UNIT_ASSERT(futil_open_and_map_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(RO_GSCVD_PATCH),
					    &fd4, FILE_RO, &ro_gscvd,
					    &ro_gscvd_size) == FILE_ERR_NONE);

	gbb = (struct vb2_gbb_header *)find_gbb(&image);
	UNIT_ASSERT(gbb != NULL);
	model.name = (char *)"testmodel";

	model.patches.rootkey = NULL;
	model.patches.vblock_a = NULL;
	model.patches.vblock_b = NULL;
	model.patches.gscvd = NULL;
	TEST_EQ(patch_image_by_model(&image, &model, NULL), 0,
		"Patch image by model no patches");

	find_patches_for_model(&model, ar);
	TEST_STR_EQ(model.patches.rootkey, ROOTKEY_PATCH, "Find patches for model: rootkey");
	TEST_STR_EQ(model.patches.vblock_a, VBLOCK_A_PATCH, "Find patches for model: vblock_a");
	TEST_STR_EQ(model.patches.vblock_b, VBLOCK_B_PATCH, "Find patches for model: vblock_b");
	TEST_STR_EQ(model.patches.gscvd, RO_GSCVD_PATCH, "Find patches for model: gscvd");
	TEST_EQ(patch_image_by_model(&image, &model, ar), 0, "Patch image by model full");
	TEST_EQ(memcmp((uint8_t *)gbb + gbb->rootkey_offset, rootkey, rootkey_size), 0,
		"    Verifying rootkey");
	TEST_EQ(memcmp(fmap_find_by_name(image.data, image.size, image.fmap_header,
					 FMAP_RW_VBLOCK_A, NULL),
		       vblock_a, vblock_a_size),
		0, "    Verifying VBLOCK_A");
	TEST_EQ(memcmp(fmap_find_by_name(image.data, image.size, image.fmap_header,
					 FMAP_RW_VBLOCK_B, NULL),
		       vblock_b, vblock_b_size),
		0, "    Verifying VBLOCK_B");
	TEST_EQ(memcmp(fmap_find_by_name(image.data, image.size, image.fmap_header,
					 FMAP_RO_GSCVD, NULL),
		       ro_gscvd, ro_gscvd_size),
		0, "    Verifying RO_GSCVD");

	model.patches.vblock_a = (char *)NONEXISTENT_FILE;
	model.patches.gscvd = (char *)NONEXISTENT_FILE;
	TEST_EQ(patch_image_by_model(&image, &model, ar), 2,
		"Patch image by model with errors");

unit_cleanup:
	if (fd1 != -1)
		UNIT_ASSERT(futil_unmap_and_close_file(fd1, FILE_RO, rootkey, rootkey_size) ==
			    FILE_ERR_NONE);
	if (fd2 != -1)
		UNIT_ASSERT(futil_unmap_and_close_file(fd2, FILE_RO, vblock_a, vblock_a_size) ==
			    FILE_ERR_NONE);
	if (fd3 != -1)
		UNIT_ASSERT(futil_unmap_and_close_file(fd3, FILE_RO, vblock_b, vblock_b_size) ==
			    FILE_ERR_NONE);
	if (fd4 != -1)
		UNIT_ASSERT(futil_unmap_and_close_file(fd4, FILE_RO, ro_gscvd, ro_gscvd_size) ==
			    FILE_ERR_NONE);
	if (ar)
		archive_close(ar);
	free_firmware_image(&image);
	UNIT_TEST_RETURN;
}

static enum unit_result setup_manifest(struct manifest **manifest, struct u_archive *archive)
{
	UNIT_TEST_BEGIN;
	*manifest = (struct manifest *)malloc(sizeof(struct manifest));
	UNIT_ASSERT(*manifest != NULL);
	**manifest = (struct manifest){.archive = archive};
unit_cleanup:
	UNIT_TEST_RETURN;
}

static enum unit_result test_manifest_add_get_model(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct model_config model = {0};

	UNIT_ASSERT(setup_manifest(&manifest, NULL));
	model.name = strdup("testmodel");

	TEST_EQ(memcmp(manifest_add_model(manifest, &model), &model, sizeof(model)), 0,
		"Manifest add model");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_STR_EQ(manifest->models[0].name, model.name, "    Verifying name");

	model = (struct model_config){0};
	model.name = strdup("model2");

	TEST_EQ(memcmp(manifest_add_model(manifest, &model), &model, sizeof(model)), 0,
		"Manifest add model2");
	TEST_EQ(manifest->num, 2, "    Verifying num");
	TEST_STR_EQ(manifest->models[1].name, model.name, "    Verifying name");

	TEST_PTR_NEQ(manifest_get_model_config(manifest, "testmodel"), NULL,
		     "Manifest get model config 1");
	TEST_PTR_NEQ(manifest_get_model_config(manifest, "model2"), NULL,
		     "Manifest get model config 2");
	TEST_PTR_EQ(manifest_get_model_config(manifest, "<missing model>"), NULL,
		    "Manifest get model config missing");

unit_cleanup:
	if (manifest)
		delete_manifest(manifest);
	UNIT_TEST_RETURN;
}

static enum unit_result test_manifest_scan_raw_entries(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;

	UNIT_ASSERT(setup_manifest(&manifest, NULL));

	TEST_EQ(manifest_scan_raw_entries("<invalid model>", manifest), 0,
		"Manifest scan raw entries invalid model");

	TEST_EQ(manifest_scan_raw_entries("image-model.modifier.bin", manifest), 0,
		"Manifest scan raw entries ignore modifier");

	TEST_EQ(manifest_scan_raw_entries("image-missing_model.bin", manifest), 0,
		"Manifest scan raw entries missing model");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_PTR_EQ(manifest->models[0].ec_image, NULL, "    Verifying ec_image");

	delete_manifest(manifest);
	UNIT_ASSERT(setup_manifest(&manifest, archive_open(FIRMWARE_ARCHIVE)));
	UNIT_ASSERT(manifest->archive != NULL);

	TEST_EQ(manifest_scan_raw_entries("image-model.bin", manifest), 0,
		"Manifest scan raw entries");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_STR_EQ(manifest->models[0].ec_image, "model/ec.bin", "    Verifying ec_image");

unit_cleanup:
	if (manifest) {
		if (manifest->archive)
			archive_close(manifest->archive);
		delete_manifest(manifest);
	}
	UNIT_TEST_RETURN;
}

static enum unit_result test_manifest_from_signer_config(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct model_config *model = NULL;
	struct u_archive *archive = NULL;

	archive = archive_open(FIRMWARE_ARCHIVE);
	UNIT_ASSERT(archive != NULL);

	remove(SIGNER_CONFIG);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_NEQ(manifest_from_signer_config(manifest), 0,
		 "Manifest from signer config: missing manifest");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(
		futil_copy_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(SIGNER_CONFIG_INVALID_HEADER),
				SIGNER_CONFIG) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_NEQ(manifest_from_signer_config(manifest), 0,
		 "Manifest from signer config: invalid header");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(
		futil_copy_file(GET_WORK_COPY_TEST_DATA_FILE_PATH(SIGNER_CONFIG_INVALID_ENTRY),
				SIGNER_CONFIG) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_signer_config(manifest), 0,
		"Manifest from signer config: invalid entry");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_PTR_NEQ(manifest_get_model_config(manifest, "model"), NULL,
		     "    Verifying correct model");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(futil_copy_file(
			    GET_WORK_COPY_TEST_DATA_FILE_PATH(SIGNER_CONFIG_ONLY_BASE_MODELS),
			    SIGNER_CONFIG) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_signer_config(manifest), 0,
		"Manifest from signer config: only base models");
	TEST_EQ(manifest->num, 2, "    Verifying num");
	model = manifest_get_model_config(manifest, "model");
	TEST_PTR_NEQ(model, NULL, "    Verifying model 1");
	TEST_STR_EQ(model->name, "model", "    Verifying model 1: name");
	TEST_STR_EQ(model->image, "image", "    Verifying model 1: image");
	TEST_STR_EQ(model->ec_image, "ec", "    Verifying model 1: ec");
	model = manifest_get_model_config(manifest, "model2");
	TEST_PTR_NEQ(model, NULL, "    Verifying model 2");
	TEST_STR_EQ(model->name, "model2", "    Verifying model 2: name");
	TEST_STR_EQ(model->image, "image2", "    Verifying model 2: image");
	TEST_PTR_EQ(model->ec_image, NULL, "    Verifying model 2: ec");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(futil_copy_file(
			    GET_WORK_COPY_TEST_DATA_FILE_PATH(SIGNER_CONFIG_WITH_CUSTOM_LABEL),
			    SIGNER_CONFIG) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_signer_config(manifest), 0,
		"Manifest from signer config: with custom label");
	TEST_EQ(manifest->num, 3, "    Verifying num");
	model = manifest_get_model_config(manifest, "model");
	TEST_PTR_NEQ(model, NULL, "    Verifying model 1");
	TEST_STR_EQ(model->name, "model", "    Verifying model 1: name");
	TEST_STR_EQ(model->image, "image", "    Verifying model 1: image");
	TEST_STR_EQ(model->ec_image, "ec", "    Verifying model 1: ec");
	TEST_TRUE(model->has_custom_label, "    Verifying model 1: has_custom_label");
	model = manifest_get_model_config(manifest, "model-label");
	TEST_PTR_NEQ(model, NULL, "    Verifying model-label");
	TEST_STR_EQ(model->name, "model-label", "    Verifying model-label: name");
	TEST_STR_EQ(model->image, "image-label", "    Verifying model-label: image");
	TEST_STR_EQ(model->ec_image, "ec-label", "    Verifying model-label: ec");
	model = manifest_get_model_config(manifest, "model2-label");
	TEST_PTR_NEQ(model, NULL, "    Verifying model2-label");
	TEST_STR_EQ(model->name, "model2-label", "    Verifying model2-label: name");
	TEST_STR_EQ(model->image, "image2-label", "    Verifying model2-label: image");
	TEST_PTR_EQ(model->ec_image, NULL, "    Verifying model2-label: ec");
	TEST_FALSE(model->has_custom_label, "    Verifying model2-label: has_custom_label");

unit_cleanup:
	if (archive)
		archive_close(archive);
	if (manifest)
		delete_manifest(manifest);

	UNIT_TEST_RETURN;
}

static enum unit_result test_manifest_from_simple_folder(void)
{
	UNIT_TEST_BEGIN;
	const char *const image_bin = LEGACY_ARCHIVE "/image.bin";
	const char *const bios_bin = LEGACY_ARCHIVE "/bios.bin";
	const char *const ec_bin = LEGACY_ARCHIVE "/ec.bin";
	const char *const model_name = "steelix"; /* image-steelix.bin, in lowercase. */
	struct manifest *manifest = NULL;
	struct model_config *model = NULL;
	struct u_archive *archive = NULL;

	archive = archive_open(LEGACY_ARCHIVE);
	UNIT_ASSERT(archive != NULL);

	remove(image_bin);
	remove(bios_bin);
	remove(ec_bin);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_NEQ(manifest_from_simple_folder(manifest), 0,
		 "Manifest from simple folder: missing images");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(futil_copy_file(IMAGE_MAIN, bios_bin) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_simple_folder(manifest), 0,
		"Manifest from simple folder: old host image");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_PTR_NEQ(manifest_get_model_config(manifest, model_name), NULL,
		     "    Verifying model");
	delete_manifest(manifest);
	manifest = NULL;

	remove(bios_bin);
	UNIT_ASSERT(futil_copy_file(IMAGE_MAIN, image_bin) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_simple_folder(manifest), 0,
		"Manifest from simple folder: new host image");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_PTR_NEQ(manifest_get_model_config(manifest, model_name), NULL,
		     "    Verifying model");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(futil_copy_file(SMALL_FILE, ec_bin) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_simple_folder(manifest), 0,
		"Manifest from simple folder: with ec image");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	model = manifest_get_model_config(manifest, model_name);
	TEST_PTR_NEQ(model, NULL, "    Verifying model");
	TEST_STR_EQ(model->ec_image, "ec.bin", "    Verifying ec.bin");
	delete_manifest(manifest);
	manifest = NULL;

	UNIT_ASSERT(futil_copy_file(IMAGE_BAD, image_bin) != -1);
	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_simple_folder(manifest), 0,
		"Manifest from simple folder: invalid image");
	TEST_EQ(manifest->num, 1, "    Verifying num");
	TEST_PTR_NEQ(manifest_get_model_config(manifest, "default"), NULL,
		     "    Verifying model");
unit_cleanup:
	if (archive)
		archive_close(archive);
	if (manifest)
		delete_manifest(manifest);
	UNIT_TEST_RETURN;
}

static enum unit_result dut_get_manifest_key_switch = 0;
int dut_get_manifest_key(char **manifest_key_out, struct updater_config *cfg)
{
	if (dut_get_manifest_key_switch == 0)
		return -1;
	*manifest_key_out = strdup("model");
	return 0;
}

static enum unit_result test_manifest_find_model(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct model_config model = {0};
	const struct model_config *model_ptr = NULL;
	struct updater_config cfg = {0}; /* Not initialized. */

	UNIT_ASSERT(setup_manifest(&manifest, NULL));
	model.name = strdup("model");
	UNIT_ASSERT(manifest_add_model(manifest, &model) != NULL);

	TEST_PTR_EQ(manifest_find_model(&cfg, manifest, "model"), &manifest->models[0],
		    "Manifest find model: single model");

	model.name = strdup("model2");
	UNIT_ASSERT(manifest_add_model(manifest, &model) != NULL);

	TEST_PTR_EQ(manifest_find_model(&cfg, manifest, NULL), NULL,
		    "Manifest find model: dut_get_manifest_key fails");

	dut_get_manifest_key_switch = 1;
	model_ptr = manifest_find_model(&cfg, manifest, NULL);
	TEST_PTR_NEQ(model_ptr, NULL, "Manifest find model: dut_get_manifest_key succeeds");
	TEST_STR_EQ(model_ptr->name, "model", "    Verifying");

	model_ptr = manifest_find_model(&cfg, manifest, "model2");
	TEST_PTR_NEQ(model_ptr, NULL, "Manifest find model: success");
	TEST_STR_EQ(model_ptr->name, "model2", "    Verifying");

	model_ptr = manifest_find_model(&cfg, manifest, "<missing model>");
	TEST_PTR_EQ(model_ptr, NULL, "Manifest find model: missing model");

unit_cleanup:
	if (manifest)
		delete_manifest(manifest);
	UNIT_TEST_RETURN;
}

static enum unit_result load_system_frid_switch = 0;
char *load_system_frid(struct updater_config *cfg)
{
	if (load_system_frid_switch == 0)
		return NULL;
	else if (load_system_frid_switch == 1)
		return strdup("");
	else
		return strdup("Google_Steelix.0.0.0");
}

static enum unit_result test_manifest_detect_model_from_frid(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct model_config model = {0};
	const struct model_config *model_ptr = NULL;
	struct updater_config cfg = {0};

	UNIT_ASSERT(setup_manifest(&manifest, NULL));

	TEST_PTR_EQ(manifest_detect_model_from_frid(&cfg, manifest), NULL,
		    "Manifest detect model from frid: bad frid");

	load_system_frid_switch = 1; /* Model: "" */
	TEST_PTR_EQ(manifest_detect_model_from_frid(&cfg, manifest), NULL,
		    "Manifest detect model from frid: bad model");

	load_system_frid_switch = 2; /* Model: "Google_Steelix.0.0.0 "*/
	TEST_PTR_EQ(manifest_detect_model_from_frid(&cfg, manifest), NULL,
		    "Manifest detect model from frid: empty manifest");

	model.image = strdup(NONEXISTENT_FILE);
	UNIT_ASSERT(manifest_add_model(manifest, &model) != NULL);
	TEST_PTR_EQ(manifest_detect_model_from_frid(&cfg, manifest), NULL,
		    "Manifest detect model from frid: invalid model");

	delete_manifest(manifest);
	manifest = NULL;
	UNIT_ASSERT(setup_manifest(&manifest, NULL));

	model.name = strdup("steelix");
	model.image = strdup(IMAGE_MAIN);
	UNIT_ASSERT(manifest_add_model(manifest, &model) != NULL);
	model_ptr = manifest_detect_model_from_frid(&cfg, manifest);
	TEST_PTR_NEQ(model_ptr, NULL, "Manifest detect model from frid: valid");
	TEST_STR_EQ(model.name, "steelix", "Verifying");

unit_cleanup:
	delete_manifest(manifest);
	UNIT_TEST_RETURN;
}

static void test_get_custom_label_tag(void)
{
	char *str = NULL;

	vpd_get_value_switch = SHELL_RETURN_TAG1;
	str = get_custom_label_tag("");
	TEST_STR_EQ(str, "tag1", "Get custom lab label tag: VPD_CUSTOM_LABEL_TAG");
	free(str);

	vpd_get_value_switch = SHELL_RETURN_TAG2;
	str = get_custom_label_tag("");
	TEST_STR_EQ(str, "tag2", "Get custom lab label tag: VPD_CUSTOM_LABEL_TAG_LEGACY");
	free(str);

	vpd_get_value_switch = SHELL_RETURN_NULL;
	str = get_custom_label_tag("");
	TEST_PTR_EQ(str, NULL, "Get custom lab label tag: none");
	free(str);

	vpd_get_value_switch = SHELL_RETURN_TAG3_LABEL;
	str = get_custom_label_tag("");
	TEST_STR_EQ(str, "TAG3", "Get custom lab label tag: VPD_CUSTOMIZATION_ID");
	free(str);
}

const struct model_config *quirk_override_custom_label(struct updater_config *cfg,
						       const struct manifest *manifest,
						       const struct model_config *model)
{
	return model;
}

static enum unit_result test_manifest_find_custom_label_model(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct model_config model = {0};
	const struct model_config *model_ptr = NULL;
	struct updater_config *cfg = updater_new_config();

	UNIT_ASSERT(cfg != NULL);
	UNIT_ASSERT(load_firmware_image(&cfg->image_current, IMAGE_MAIN, NULL) == 0);
	model.name = strdup("model");

	UNIT_ASSERT(setup_manifest(&manifest, NULL));

	cfg->quirks[QUIRK_OVERRIDE_CUSTOM_LABEL].value = 1;
	model_ptr = manifest_find_custom_label_model(cfg, manifest, &model);
	TEST_PTR_NEQ(model_ptr, NULL,
		     "Manifest find custom label mode: override quirk succeeded");

	cfg->quirks[QUIRK_OVERRIDE_CUSTOM_LABEL].value = 0;

	vpd_get_value_switch = SHELL_RETURN_NULL;
	model_ptr = manifest_find_custom_label_model(cfg, manifest, &model);
	TEST_PTR_EQ(model_ptr, &model, "Manifest find custom label mode: no custom label");

	vpd_get_value_switch = SHELL_RETURN_MODEL;

	model_ptr = manifest_find_custom_label_model(cfg, manifest, &model);
	TEST_PTR_EQ(model_ptr, NULL, "Manifest find custom label mode: empty manifest");

	UNIT_ASSERT(manifest_add_model(manifest, &model) != NULL);

	model_ptr = manifest_find_custom_label_model(cfg, manifest, &model);
	TEST_PTR_NEQ(model_ptr, NULL, "Manifest find custom label mode: success");
	TEST_STR_EQ(model_ptr->name, "model", "Verifying");

unit_cleanup:
	if (manifest)
		delete_manifest(manifest);
	if (cfg)
		updater_delete_config(cfg);
	UNIT_TEST_RETURN;
}

static enum unit_result test_new_manifest(void)
{
	UNIT_TEST_BEGIN;
	struct manifest *manifest = NULL;
	struct u_archive *archive = NULL;

	archive = archive_open(EMPTY_FOLDER);
	UNIT_ASSERT(archive != NULL);
	remove(SIGNER_CONFIG);

	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_build_artifacts(manifest), 0,
		"New manifest from artifacts: bad archive");
	TEST_EQ(manifest->num, 0, "Verifying num");
	delete_manifest(manifest);
	manifest = NULL;

	TEST_PTR_EQ(new_manifest_from_archive(archive), NULL,
		    "New manifest from archive: bad archive");

	archive_close(archive);
	archive = archive_open(FIRMWARE_ARCHIVE);
	UNIT_ASSERT(archive != NULL);

	UNIT_ASSERT(setup_manifest(&manifest, archive));
	TEST_EQ(manifest_from_build_artifacts(manifest), 0,
		"New manifest from artifacts: valid");
	TEST_EQ(manifest->num, 1, "Verifying num");
	TEST_STR_EQ(manifest->models[0].name, "model", "Verifying model");
	delete_manifest(manifest);

	manifest = new_manifest_from_archive(archive);
	TEST_PTR_NEQ(manifest, NULL, "New manifest from archive: valid");
	TEST_EQ(manifest->num, 1, "Verifying num");
	TEST_STR_EQ(manifest->models[0].name, "model", "Verifying model");

unit_cleanup:
	if (archive)
		archive_close(archive);
	if (manifest)
		delete_manifest(manifest);
	UNIT_TEST_RETURN;
}

int main(int argc, char *argv[])
{
	if (prepare_test_data() == UNIT_FAIL) {
		ERROR("Failed to prepare data.\n");
		return 1;
	}

	test_str_convert();
	test_vpd_get_value();
	test_change_gbb_rootkey();
	test_change_section();
	test_apply_key_file();
	test_model_patches();
	test_manifest_add_get_model();
	test_manifest_scan_raw_entries();
	test_manifest_from_signer_config();
	test_manifest_from_simple_folder();
	test_manifest_find_model();
	test_manifest_detect_model_from_frid();
	test_get_custom_label_tag();
	test_manifest_find_custom_label_model();
	test_new_manifest();

	return !gTestSuccess;
}
