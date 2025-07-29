/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifdef HAVE_LIBZIPARCHIVE

#include "futility.h"
#include "updater_archive.h"
#include <libziparchive_wrapper/libziparchive_wrapper.h>

/*
 * -- The libziparchive driver (using wrapper). --
 */

/* Callback for archive_open on a ZIP file. */
static void *archive_libziparchive_open(const char *name)
{
	return libziparchive_open(name);
}

/* Callback for archive_close on a ZIP file. */
static int archive_libziparchive_close(void *handle)
{
	if (!handle)
		return 0;
	return libziparchive_close((zip_handle)handle);
}

/* Callback for archive_has_entry on a ZIP file. */
static int archive_libziparchive_has_entry(void *handle, const char *fname)
{
	zip_entry entry = libziparchive_alloc_entry();
	int r = libziparchive_find_entry(handle, fname, entry);
	libziparchive_release_entry(entry);
	return !r;
}

/* Callback for archive_walk on a ZIP file. */
static int archive_libziparchive_walk(void *handle, void *arg,
				      int (*callback)(const char *name, void *arg))
{
	zip_handle reader = (zip_handle)handle;
	zip_cookie cookie;

	if (libziparchive_start_iteration(reader, &cookie)) {
		printf("ERROR: Failed to start iteration over files in the archive.\n");
		return -1;
	}

	zip_entry entry = libziparchive_alloc_entry();
	char *name;
	int r = 0;

	while (r != -1) {
		r = libziparchive_next(cookie, entry, &name);

		if (r < -1) {
			printf("ERROR: Failed while iterating over files in the archive.\n");
			free(name);
			break;
		} else if (r == 0 && name[strlen(name) - 1] != '/') {
			if (callback(name, arg))
				r = -1;
		}

		free(name);
	}

	if (r == -1)
		r = 0;

	libziparchive_stop_iteration(cookie);
	r |= libziparchive_release_entry(entry);

	return r;
}

/* Callback for archive_zip_read_file on a ZIP file. */
static int archive_libziparchive_read_file(void *handle, const char *fname, uint8_t **data,
					   uint32_t *size, int64_t *mtime)
{
	zip_entry entry = libziparchive_alloc_entry();
	if (libziparchive_find_entry(handle, fname, entry)) {
		printf("ERROR: Failed to locate %s in the archive.\n", fname);
		return -1;
	}

	size_t size64;
	if (libziparchive_extract_entry(handle, entry, data, &size64)) {
		printf("ERROR: Failed to extract %s from the archive.\n", fname);
		return -1;
	}
	*size = size64;

	if (mtime)
		*mtime = libziparchive_get_mtime(entry);

	libziparchive_release_entry(entry);

	return 0;
}

/* Callback for archive_zip_write_file on a ZIP file. */
static int archive_libziparchive_write_file(void *handle, const char *fname, uint8_t *data,
					    uint32_t size, int64_t mtime)
{
	return libziparchive_write_entry(handle, fname, data, size, mtime);
}

struct u_archive archive_libziparchive = {
	.open = archive_libziparchive_open,
	.close = archive_libziparchive_close,
	.walk = archive_libziparchive_walk,
	.has_entry = archive_libziparchive_has_entry,
	.read_file = archive_libziparchive_read_file,
	.write_file = archive_libziparchive_write_file,
};

#endif
