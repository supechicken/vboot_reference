/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

/*
 * Lazy read / write wrapper over libziparchive.
 *
 * Only one kind of operations can be executed on
 * an archive (simultaneous reading and writing are not supported).
 *
 * The archive is opened when
 * the first determining (whether this archive will be read or written) operation is executed.
 */

#ifdef __cplusplus
extern "C" {
#endif

typedef void *zip_reader;
typedef void *zip_writer;

/*
 * Until the archive is opened, all fields except from `path` will be NULL.
 * When opened for reading, `reader` is set to an instance of ZipArchive.
 * When opened for writing, `writer` is set to an instance of ZipWriter, and `file` is the
 * opened archive file.
 */
struct zip_handle_t {
	void *reader;
	void *writer;
	FILE *file;
	const char *path;
};
typedef struct zip_handle_t *zip_handle;

typedef void *zip_cookie;
typedef void *zip_entry;

/*
 * Lazily opens the archive file. Filename is stored, but the actual file is not yet opened.
 * Returns NULL if failed.
 */
zip_handle libziparchive_open(const char *filename);

/*
 * Closes the opened archive. Returns 0 if success, something else if failure.
 */
int libziparchive_close(zip_handle handle);

/*
 * Allocates a new entry on the heap.
 */
zip_entry libziparchive_alloc_entry(void);

/*
 * Deallocates the entry. Returns 0 if success, something else if failure.
 */
int libziparchive_release_entry(zip_entry entry);

/*
 * Starts iteration over entries in the archive. `cookie` is set to an allocated cookie.
 * Returns 0 if success, something else if failure.
 */
int libziparchive_start_iteration(zip_handle handle, zip_cookie *cookie);

/*
 * Stops iteration. Deallocates `cookie`.
 */
void libziparchive_stop_iteration(zip_cookie cookie);

/*
 * Advances to the next entry in the archive. Sets `name` to the name of the entry.
 * Returns 0 if success, -1 if there are no more entries, other negative numbers if failure.
 */
int libziparchive_next(zip_cookie cookie, zip_entry entry, char **name);

/*
 * Locates an entry in the archive with the given name. Returns 0 if success, something else if
 * failed or didn't find the entry.
 */
int libziparchive_find_entry(zip_handle handle, const char *name, zip_entry entry);

/*
 * Returns modification time of the entry.
 */
int32_t libziparchive_get_mtime(zip_entry entry);

/*
 * Extracts contents of the entry. `data` is set to the allocated data buffer, `size` is set to
 * the size of the entry. Returns 0 if success, something else if failure.
 */
int libziparchive_extract_entry(zip_handle handle, zip_entry entry, uint8_t **data,
				size_t *size);

/*
 * Writes a new entry in the archive.
 * Returns 0 if success, something else if failure.
 */
int libziparchive_write_entry(zip_handle handle, const char *name, uint8_t *data, size_t size,
			      int32_t mtime);

#ifdef __cplusplus
}
#endif
