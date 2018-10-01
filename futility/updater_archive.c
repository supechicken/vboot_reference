/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Accessing updater resources from an archive.
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_LIBZIP
#include <zip.h>
#endif

#include "host_misc.h"
#include "updater.h"
#include "vb2_common.h"

struct archive {
	void *handle;

	void * (*open)(const char *name);
	int (*close)(void *handle);

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

/* Callback for archive_has_entry on a general file system. */
static int archive_fallback_has_entry(void *handle, const char *fname)
{
	int r;
	const char *path = fname;
	char *temp_path = NULL;

	if (handle && *fname != '/') {
		if (asprintf(&temp_path, "%s/%s", (char *)handle, fname) < 0)
			return 0;
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
		if (asprintf(&temp_path, "%s/%s", (char *)handle, fname) < 0)
			return -1;
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
		ar->has_entry = archive_fallback_has_entry;
		ar->read_file = archive_fallback_read_file;
	} else {
#ifdef HAVE_LIBZIP
		DEBUG("Found file, use ZIP driver: %s", path);
		ar->open = archive_zip_open;
		ar->close = archive_zip_close;
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
