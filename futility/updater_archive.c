/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Accessing updater resources from an archive.
 */

#include <assert.h>
#include <ctype.h>
#include <fts.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

#include "futility.h"
#include "host_misc.h"
#include "updater.h"
#include "vb2_common.h"

static const char * const VPD_CUSTOMIZATION_ID = "customization_id",
		  * const VPD_WHITELABEL_TAG = "whitelabel_tag",
		  * const SETVAR_IMAGE_MAIM = "IMAGE_MAIN",
		  * const SETVAR_IMAGE_EC = "IMAGE_EC",
		  * const SETVAR_IMAGE_PD = "IMAGE_PD",
		  * const SETVAR_SIGNATURE_ID = "SIGNATURE_ID",
		  * const SIG_ID_IN_VPD_PREFIX = "sig-id-in",
		  * const DIR_KEYSET = "keyset",
		  * const DIR_MODELS = "models";

struct image_config {
	char *image;
	char *ro_version, *rw_version;
};

struct key_hash_data {
	char *root, *recovery;
};

struct patch_config {
	char *rootkey;
	char *vblock_a;
	char *vblock_b;
};

struct model_config {
	char *name;
	struct image_config host, ec, pd;
	struct patch_config patches;
	struct key_hash_data keys;
	char *quirks;
	char *signature_id;
};

struct archive_manifest {
	int num;
	int default_model;
	int has_keyset;
	struct model_config *models;
};

struct archive {
	void *handle;

	void * (*open)(const char *name);
	int (*close)(void *handle);

	int (*walk)(void *handle, void *arg,
		    int (*callback)(const char *name, void *arg));
	int (*has_entry)(void *handle, const char *name);
	int (*read_file)(void *handle, const char *fname,
			 uint8_t **data, uint32_t *size);
};

/* Callback for archive_open on a general file system. */
static void *archive_fallback_open(const char *name)
{
	assert(name && *name);
	return strdup(name);
}

/* Callback for archive_close on a general file system. */
static int archive_fallback_close(void *handle)
{
	free(handle);
	return 0;
}

static int archive_fallback_walk(void *handle, void *arg,
				 int (*callback)(const char *name, void *arg))
{
	FTS *fts_handle;
	FTSENT *ent;
	char *fts_argv[2] = {};
	char default_path[] = ".";
	char *root = default_path;
	size_t root_len;

	if (handle)
		root = (char *)handle;
	root_len = strlen(root);
	fts_argv[0] = root;

	fts_handle = fts_open(fts_argv, 0, NULL);
	if (!fts_handle)
		return -1;

	while ((ent = fts_read(fts_handle)) != NULL) {
		char *path = ent->fts_path + root_len;
		if (*path == '/')
			path++;
		if (!*path)
			continue;
		if (callback(path, arg))
			break;
	}
	return 0;
}

/* Callback for archive_has_entry on a general file system. */
static int archive_fallback_has_entry(void *handle, const char *fname)
{
	int r;
	const char *path = fname;
	char *temp_path = NULL;

	if (handle && *fname != '/') {
		ASPRINTF(&temp_path, "%s/%s", (char *)handle, fname);
		path = temp_path;
	}

	DEBUG("Checking %s", path);
	r = access(path, R_OK);
	free(temp_path);
	return r == 0;
}

/* Callback for archive_read_file on a general file system. */
static int archive_fallback_read_file(void *handle, const char *fname,
				      uint8_t **data, uint32_t *size)
{
	int r;
	const char *path = fname;
	char *temp_path = NULL;

	*data = NULL;
	*size = 0;
	if (handle && *fname != '/') {
		ASPRINTF(&temp_path, "%s/%s", (char *)handle, fname);
		path = temp_path;
	}
	DEBUG("Reading %s", path);
	r = vb2_read_file(path, data, size) != VB2_SUCCESS;
	free(temp_path);
	return r;
}

#ifdef HAVE_LIBZIP

/* Callback for archive_open on a ZIP file. */
static void *archive_zip_open(const char *name)
{
	return zip_open(name, 0, NULL);
}

/* Callback for archive_close on a ZIP file. */
static int archive_zip_close(void *handle)
{
	struct zip *zip = (struct zip *)handle;

	if (zip)
		return zip_close(zip);
	return 0;
}

/* Callback for archive_has_entry on a ZIP file. */
static int archive_zip_has_entry(void *handle, const char *fname)
{
	struct zip *zip = (struct zip *)handle;
	assert(zip);
	return zip_name_locate(zip, fname, 0) != -1;
}

static int archive_zip_walk(void *handle, void *arg,
			    int (*callback)(const char *name, void *arg))
{
	zip_int64_t num, i;
	struct zip *zip = (struct zip *)handle;
	assert(zip);

	num = zip_get_num_entries(zip, 0);
	if (num < 0)
		return 1;
	for (i = 0; i < num; i++) {
		if (callback(zip_get_name(zip, i, 0), arg))
			break;
	}
	return 0;
}

/* Callback for archive_zip_read_file on a ZIP file. */
static int archive_zip_read_file(void *handle, const char *fname,
			     uint8_t **data, uint32_t *size)
{
	struct zip *zip = (struct zip *)handle;
	struct zip_file *fp;
	struct zip_stat stat;

	assert(zip);
	*data = NULL;
	*size = 0;
	zip_stat_init(&stat);
	if (zip_stat(zip, fname, 0, &stat)) {
		ERROR("Fail to stat entry in ZIP: %s", fname);
		return 1;
	}
	fp = zip_fopen(zip, fname, 0);
	if (!fp) {
		ERROR("Failed to open entry in ZIP: %s", fname);
		return 1;
	}
	*data = (uint8_t *)malloc(stat.size);
	if (*data) {
		if (zip_fread(fp, *data, stat.size) == stat.size) {
			*size = stat.size;
		} else {
			ERROR("Failed to read entry in zip: %s", fname);
			free(*data);
			*data = NULL;
		}
	}
	zip_fclose(fp);
	return *data == NULL;
}
#endif

/*
 * Opens an archive from given path.
 * The type of archive will be determined automatically.
 * Returns a pointer to reference to archive (must be released by archive_close
 * when not used), otherwise NULL on error.
 */
struct archive *archive_open(const char *path)
{
	struct stat path_stat;
	struct archive *ar;

	if (stat(path, &path_stat) != 0) {
		ERROR("Cannot identify type of path: %s", path);
		return NULL;
	}

	ar = (struct archive *)malloc(sizeof(*ar));
	if (!ar) {
		ERROR("Internal error: allocation failure.");
		return NULL;
	}

	if (S_ISDIR(path_stat.st_mode)) {
		DEBUG("Found directory, use fallback (fs) driver: %s", path);
		/* Regular file system. */
		ar->open = archive_fallback_open;
		ar->close = archive_fallback_close;
		ar->walk = archive_fallback_walk;
		ar->has_entry = archive_fallback_has_entry;
		ar->read_file = archive_fallback_read_file;
	} else {
#ifdef HAVE_LIBZIP
		DEBUG("Found file, use ZIP driver: %s", path);
		ar->open = archive_zip_open;
		ar->close = archive_zip_close;
		ar->walk = archive_zip_walk;
		ar->has_entry = archive_zip_has_entry;
		ar->read_file = archive_zip_read_file;
#else
		ERROR("Found file, but no drivers were enabled: %s", path);
		free(ar);
		return NULL;
#endif
	}
	ar->handle = ar->open(path);
	if (!ar->handle) {
		ERROR("Failed to open archive: %s", path);
		free(ar);
		return NULL;
	}
	return ar;
}

/*
 * Closes an archive reference.
 * Returns 0 on success, otherwise non-zero as failure.
 */
int archive_close(struct archive *ar)
{
	int r = ar->close(ar->handle);
	free(ar);
	return r;
}

/*
 * Checks if an entry (either file or directory) exists in archive.
 * If entry name (fname) is an absolute path (/file), always check
 * with real file system.
 * Returns 1 if exists, otherwise 0
 */
int archive_has_entry(struct archive *ar, const char *name)
{
	if (!ar || *name == '/')
		return archive_fallback_has_entry(NULL, name);
	return ar->has_entry(ar->handle, name);
}

int archive_walk(struct archive *ar, void *arg,
		 int (*callback)(const char *name, void *arg))
{
	if (!ar)
		return archive_fallback_walk(NULL, arg, callback);
	return ar->walk(ar->handle, arg, callback);
}

/*
 * Reads a file from archive.
 * If entry name (fname) is an absolute path (/file), always read
 * from real file system.
 * Returns 0 on success (data and size reflects the file content),
 * otherwise non-zero as failure.
 */
int archive_read_file(struct archive *ar, const char *fname,
		      uint8_t **data, uint32_t *size)
{
	if (!ar || *fname == '/')
		return archive_fallback_read_file(NULL, fname, data, size);
	return ar->read_file(ar->handle, fname, data, size);
}

/* End of archive system. */

/* Utility function to convert a string to all uppercase. */
static void str_toupper(char *s)
{
	int c;

	for (; *s; s++) {
		c = *s;
		if (!isascii(c) || !isalpha(c))
			continue;
		*s = toupper(c);
	}
}

static int str_endswith(const char *name, const char *pattern)
{
	size_t name_len = strlen(name), pattern_len = strlen(pattern);
	if (name_len < pattern_len)
		return 0;
	return strcmp(name + name_len - pattern_len, pattern) == 0;
}

static int str_startswith(const char *name, const char *pattern)
{
	return strncmp(name, pattern, strlen(pattern)) == 0;
}

/* Returns the VPD value from given key name, or NULL on faliure. */
char *vpd_get_value(const char *name)
{
	char *command, *result;
	char *image = NULL; // TODO fixme.

#if 1
	if (asprintf(&command, "vpd -f %s -i RO_VPD -g %s", image, name) < 0)
		return NULL;
#else

	if (asprintf(&command, "vpd_get_value %s", name) < 0)
		return NULL;
#endif

	result = host_shell(command);
	free(command);
	return result;
}

static int model_config_parse_setvars(char *content, struct model_config *cfg)
{
	char *ptr_line, *ptr_token;
	char *line, *k, *v;
	int valid = 0;

	for (line = strtok_r(content, "\n\r", &ptr_line); line;
	     line = strtok_r(NULL, "\n\r", &ptr_line)) {
		/* Format: KEY="value" */
		k = strtok_r(line, "=", &ptr_token);
		if (!k)
			continue;
		v = strtok_r(NULL, "\"", &ptr_token);
		if (!v)
			continue;

		if (strcmp(k, SETVAR_IMAGE_MAIM) == 0)
			cfg->host.image = strdup(v);
		else if (strcmp(k, SETVAR_IMAGE_EC) == 0)
			cfg->ec.image = strdup(v);
		else if (strcmp(k, SETVAR_IMAGE_PD) == 0)
			cfg->pd.image = strdup(v);
		else if (strcmp(k, SETVAR_SIGNATURE_ID) == 0)
			cfg->signature_id = strdup(v);
		else
			continue;
		valid++;
	}
	return valid == 0;
}

static char *get_signature_id(struct model_config *model,
			      const char *model_name)
{
	char *sid, *wl_tag;

	if (!model->signature_id) {
		/* Legacy configuration. Try VPD. */
		sid = vpd_get_value(VPD_WHITELABEL_TAG);
		if (sid) {
			str_toupper(sid);
			return sid;
		}
		sid = vpd_get_value(VPD_CUSTOMIZATION_ID);
		if (sid) {
			char *dash = strchr(sid, '-');
			if (dash)
				*dash = '\0';
			return sid;
		}
	}

	if (!str_startswith(model->signature_id, SIG_ID_IN_VPD_PREFIX))
		return strdup(model->signature_id);

	wl_tag = vpd_get_value(VPD_WHITELABEL_TAG);
	if (!wl_tag) {
		ERROR("No WL set yet.");
		return strdup(model_name);
	}

	if (asprintf(&sid, "%s.%s", model_name, wl_tag) < 0) {
		ERROR("Internal allocation");
		return NULL;
	}
	return sid;
}

static int change_gbb_rootkey(struct firmware_image *image,
			      const char *section_name,
			      const uint8_t *rootkey, uint32_t rootkey_len)
{
	struct vb2_gbb_header *gbb = firmware_find_gbb(image);
	uint8_t *gbb_rootkey;
	if (!gbb) {
		ERROR("Cannot find GBB in image %s.", image->file_name);
		return -1;
	}
	if (gbb->rootkey_size < rootkey_len) {
		ERROR("New root key (%u bytes) larger than GBB (%u bytes).",
		      rootkey_len, gbb->rootkey_size);
		return -1;
	}

	gbb_rootkey = (uint8_t *)gbb + gbb->rootkey_offset;
	/* See cmd_gbb_utility: root key must be first cleared with zero. */
	memset(gbb_rootkey, 0, gbb->rootkey_size);
	memcpy(gbb_rootkey, rootkey, rootkey_len);
	return 0;
}

static int change_vblock(struct firmware_image *image, const char *section_name,
			 const uint8_t *vblock, uint32_t vblock_len)
{
	struct firmware_section section;

	find_firmware_section(&section, image, section_name);
	if (!section.data) {
		ERROR("Need section %s in image %s.", section_name,
		      image->file_name);
		return -1;
	}
	if (section.size < vblock_len) {
		ERROR("Section %s too small (%zu bytes) for vblock (%u bytes).",
		      section_name, section.size, vblock_len);
		return -1;
	}
	memcpy(section.data, vblock, vblock_len);
	return 0;
}

static int apply_key_file(
		struct archive *archive, struct firmware_image *image,
		const char *path, const char *section_name,
		int (*apply)(struct firmware_image *image, const char *section,
			     const uint8_t *data, uint32_t len))
{
	int r = 0;
	uint8_t *data = NULL;
	uint32_t len;

	r = archive_read_file(archive, path, &data, &len);
	if (r == 0) {
		DEBUG("Loaded file: %s", path);
		r = apply(image, section_name, data, len);
		if (r)
			ERROR("Failed applying %s to %s", path, section_name);
	} else {
		ERROR("Failed reading: %s", path);
	}
	free(data);
	return r;
}

static int archive_patch_image(
		struct archive *archive, struct firmware_image *image,
		const struct model_config *model)
{
	int err;
	if (model->patches.rootkey)
		err += !!apply_key_file(
				archive, image, model->patches.rootkey,
				FMAP_RO_GBB, change_gbb_rootkey);
	if (model->patches.vblock_a)
		err += !!apply_key_file(
				archive, image, model->patches.vblock_a,
				FMAP_RW_VBLOCK_A, change_vblock);
	if (model->patches.vblock_b)
		err += !!apply_key_file(
				archive, image, model->patches.vblock_b,
				FMAP_RW_VBLOCK_B, change_vblock);
	return err;
}

struct add_model_arg {
	struct archive *archive;
	struct archive_manifest *manifest;
};

static struct model_config *manifest_add_model(
		struct archive_manifest *manifest,
		const struct model_config *cfg)
{
	struct model_config *model;
	manifest->num++;
	manifest->models = (struct model_config *)realloc(
			manifest->models, manifest->num * sizeof(*model));
	if (!manifest->models) {
		ERROR("Internal error: failed to allocate buffer.");
		return NULL;
	}
	model = &manifest->models[manifest->num - 1];
	memcpy(model, cfg, sizeof(*model));
	return model;
}

static void archive_find_keyset_file(struct archive *archive,
				     struct model_config *model,
				     const char *name,
				     char **patch_file)
{
	char *path;
	ASPRINTF(&path, "%s/%s.%s", DIR_KEYSET, name, model->signature_id);
	if (archive_has_entry(archive, path))
		*patch_file = path;
	else
		free(path);
}


static int _archive_add_setvar_model(const char *name, void *arg)
{
	struct add_model_arg *marg = (struct add_model_arg *)arg;
	struct archive *archive = marg->archive;
	struct archive_manifest *manifest = marg->manifest;
	struct model_config model = {0};
	char *slash;

	uint8_t *data;
	uint32_t len;

	if (str_startswith(name, "keyset/"))
		manifest->has_keyset = 1;
	if (!str_endswith(name, "/setvars.sh"))
		return 0;

	/* name: models/$MODEL/setvars.sh */
	model.name = strdup(strchr(name, '/') + 1);
	slash = strchr(model.name, '/');
	if (slash)
		*slash = '\0';

	DEBUG("Found model <%s> setvar: %s", model.name, name);
	if (archive_read_file(archive, name, &data, &len) != 0) {
		ERROR("Failed reading: %s", name);
		return 0;
	}

	/* Valid content should end with \n, or \"; ensure ASCIIZ for parsing */
	if (len)
		data[len - 1] = '\0';

	if (model_config_parse_setvars((char *)data, &model)) {
		ERROR("Invalid setvar file: %s", name);
		free(data);
		return 0;
	}
	free(data);

	/* Check if there are patches available. */


	/* In legacy setvar.sh, the ec_image and pd_image may not exist. */
	if (model.ec.image && !archive_has_entry(archive, model.ec.image)) {
		DEBUG("Ignore non-exist EC image: %s", model.ec.image);
		free(model.ec.image);
		model.ec.image = NULL;
	}
	if (model.pd.image && !archive_has_entry(archive, model.pd.image)) {
		DEBUG("Ignore non-exist PD image: %s", model.pd.image);
		free(model.pd.image);
		model.pd.image = NULL;
	}

	/* Find patch files. */
	if (model.signature_id) {
		archive_find_keyset_file(archive, &model, "rootkey",
					 &model.patches.rootkey);
		archive_find_keyset_file(archive, &model, "vblock_A",
					 &model.patches.vblock_a);
		archive_find_keyset_file(archive, &model, "vblock_B",
					 &model.patches.vblock_b);
	}

	if (!manifest_add_model(manifest, &model))
		return 1;
	return 0;
}

int archive_load_images(struct archive *archive,
			struct archive_manifest *manifest,
			struct updater_config *cfg,
			const char *model_name)
{
	int errorcnt = 0;
	char *os_model = NULL, *sid;
	struct model_config *model = NULL;
	int i;

	if (!model_name) {
		os_model = host_shell("mosys platform model");
		model_name = os_model;
	}
	if (!model_name) {
		ERROR("Need a valid model to determine which images to load.");
		return 1;
	}
	for (i = 0; !model && i < manifest->num; i++) {
		if (strcmp(model_name, manifest->models[i].name) == 0)
			model = &manifest->models[i];
	}
	if (!model && manifest->default_model >= 0) {
		model = &manifest->models[manifest->default_model - 1];
		DEBUG("No exact match for model <%s>, use <%s>", model_name,
		      model->name);
	}

	if (!model) {
		ERROR("Unsupported model: %s", model_name);
		return 1;
	}

	if (cfg->emulation)
		errorcnt += updater_load_images(
				cfg, archive, model->host.image, NULL, NULL);
	else
		errorcnt += updater_load_images(
				cfg, archive, model->host.image,
				model->ec.image, model->pd.image);

	if (!manifest->has_keyset)
		return errorcnt;

	DEBUG("Found keyset, start loading additional keys");

	sid = get_signature_id(model, model_name);

	/* TODO(hungte) Allow ignoring if we're in factory mode. */
	if (!sid) {
		ERROR("Missing signature to find keyset.");
		return -1;
	}
	DEBUG("Detected signature id = %s", sid);

	/* TODO(hungte) Apply key files sfor non-unibuild */
	errorcnt += archive_patch_image(
			cfg->archive, &cfg->image, model);

	free(os_model);
	free(sid);
	return errorcnt;
}

/* Scans resources from archive and try to build a manifest file. */
struct archive_manifest *archive_create_manifest(struct archive *archive)
{
	struct archive_manifest manifest = {0}, *new_manifest;
	struct model_config model = {0};
	struct add_model_arg arg = {
		.archive = archive,
		.manifest = &manifest,
	};
	const char *image_name = "bios.bin",
	           *ec_name = "ec.bin",
		   *pd_name = "pd.bin";

	manifest.default_model = -1;
	archive_walk(archive, &arg, _archive_add_setvar_model);
	if (manifest.num == 0) {
		/* Try to load from current folder. */
		if (!archive_has_entry(archive, image_name))
			return 0;
		model.host.image = strdup(image_name);
		if (archive_has_entry(archive, ec_name))
			model.ec.image = strdup(ec_name);
		if (archive_has_entry(archive, pd_name))
			model.pd.image = strdup(pd_name);
		model.name = strdup("default");
		manifest_add_model(&manifest, &model);
		manifest.default_model = manifest.num - 1;
	}
	DEBUG("%d model(s) loaded.", manifest.num);
	if (!manifest.num) {
		ERROR("No valid configurations found from archive.");
		return NULL;
	}

	new_manifest = (struct archive_manifest *)malloc(sizeof(manifest));
	if (!new_manifest) {
		ERROR("Internal error: memory allocation error.");
		return NULL;
	}
	memcpy(new_manifest, &manifest, sizeof(manifest));
	return new_manifest;
}

void archive_delete_manifest(struct archive_manifest *manifest)
{
	int i;

	if (!manifest)
		return;

	for (i = 0; i < manifest->num; i++) {
		struct model_config *model = &manifest->models[i];
		free(model->name);
		free(model->host.image);
		free(model->ec.image);
		free(model->pd.image);
	}
	free(manifest->models);
	free(manifest);
}

static void archive_print_manifest_image_meta(
		struct archive *archive,
		const char *name, const struct model_config *model)
{
	struct firmware_image image = {0};
	const char *key_hash;
	load_image(archive, name, &image);
	if (model)
		archive_patch_image(archive, &image, model);

	printf("      , \"versions\": {\n");
	printf("        \"ro\": \"%s\",\n", image.ro_version);
	printf("        \"rw\": \"%s\" }\n", image.rw_version_a);
	if (model) {
		printf("      , \"keys\": {\n");
		key_hash = firmware_get_gbb_key_hash(&image, 1);
		printf("        \"root\": \"%s\",\n", key_hash);
		key_hash = firmware_get_gbb_key_hash(&image, 0);
		printf("        \"recovery\": \"%s\" }\n", key_hash);
	}
	free_image(&image);
}

void archive_print_manifest(struct archive_manifest *manifest,
			    struct archive *archive)
{
	int i;

	printf("{\n");
	for (i = 0; i < manifest->num; i++) {
		struct model_config *m = &manifest->models[i];
		printf("  %s\"%s\": {\n", i == 0 ? "" : ", ", m->name);
		printf("    \"host\": {\n");
		printf("      \"image\": \"%s\"\n", m->host.image);
		archive_print_manifest_image_meta(archive, m->host.image, m);
		printf("    }\n");
		if (m->ec.image) {
			printf("    , \"ec\": {\n");
			printf("      \"image\": \"%s\"\n", m->ec.image);
			archive_print_manifest_image_meta(
					archive, m->ec.image, NULL);
			printf("    }\n");
		}
		if (m->pd.image) {
			printf("    , \"pd\": {\n");
			printf("      \"image\": \"%s\"\n", m->pd.image);
			archive_print_manifest_image_meta(
					archive, m->pd.image, NULL);
			printf("    }\n");
		}
		if (m->patches.rootkey) {
			printf("    , \"patches\": {\n");
			printf("      \"rootkey\": \"%s\"\n",
			       m->patches.rootkey);
			printf("      , \"vblock_a\": \"%s\"\n",
			       m->patches.vblock_a);
			printf("      , \"vblock_b\": \"%s\"\n",
			       m->patches.vblock_b);
			printf("    }\n");
		}
		if (m->signature_id)
			printf("    , \"signature_id\": \"%s\"\n",
			       m->signature_id);
		if (m->quirks)
			printf("    , \"quirks\": \"%s\"\n",
			       m->quirks);
		printf("  }\n");
	}
	printf("}\n");
}
