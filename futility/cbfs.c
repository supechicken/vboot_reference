/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Minimal implementation for Coreboot File System (CBFS).
 */

#include <assert.h>
#include <string.h>

#include "cbfs.h"

/* Following data structure and magic numbers were copied from cbfs_core.h. */

/** These are standard component types for well known
    components (i.e - those that coreboot needs to consume.
    Users are welcome to use any other value for their
    components */

#define CBFS_TYPE_DELETED    0x00000000
#define CBFS_TYPE_DELETED2   0xffffffff

/* this used to be flexible, but wasn't ever set to something different. */
#define CBFS_ALIGNMENT 64

/** This is a component header - every entry in the CBFS
    will have this header.

    This is how the component is arranged in the ROM:

    --------------   <- 0
    component header
    --------------   <- sizeof(struct component)
    component name
    --------------   <- offset
    data
    ...
    --------------   <- offset + len
*/

#define CBFS_FILE_MAGIC "LARCHIVE"

struct cbfs_file {
	char magic[8];
	uint32_t len;
	uint32_t type;
	uint32_t attributes_offset;
	uint32_t offset;
} __packed;

/* Following APIs are implemented from scratch. */

/* Returns a new offset value aligned by CBFS_ALIGNMENT. */
static uint32_t cbfs_aligned(uint32_t offset)
{
	uint32_t remains = offset % CBFS_ALIGNMENT;
	return remains ? offset - remains + CBFS_ALIGNMENT : offset;
}

/* Decodes a big-endian integer (usually from CBFS header). */
static uint32_t cbfs_get_int(const uint32_t *be_int)
{
	const uint8_t *p = (const uint8_t *)be_int;
	return p[0] << 24 | p[1] << 16 | p[2] << 8 | p[3];
}

/* Returns 1 if the given location has a valid CBFS file entry. */
static int cbfs_is_valid(const uint8_t *start, uint32_t offset, uint32_t size)
{
	const struct cbfs_file *file;
	uint32_t end;

	if (offset + sizeof(*file) >= size)
		return 0;
	file = (const struct cbfs_file *)(start + offset);
	if (strncmp(file->magic, CBFS_FILE_MAGIC, strlen(CBFS_FILE_MAGIC)) != 0)
		return 0;
	if (file->offset <= sizeof(*file))
		return 0;
	end = offset + cbfs_get_int(&file->offset) + cbfs_get_int(&file->len);
	if (end > size)
		return 0;
	return 1;
}

/*
 * Returns the expected 'next entry' from given offset.
 * The offset must point to a valid entry (by cbfs_is_valid).
 */
static uint32_t cbfs_next_offset(const uint8_t *start, uint32_t offset)
{
	const struct cbfs_file *file;
	file = (const struct cbfs_file *)(start + offset);
	return cbfs_aligned(offset + cbfs_get_int(&file->offset) +
			    cbfs_get_int(&file->len));
}

/*
 * Returns the file name pointer from a valid cbfs_file entry. */
static const char *cbfs_get_name(const struct cbfs_file *file)
{
	return (const char *)(file + 1);
}

/*
 * Finds a cbfs_file entry by given file name from a CBFS blob (by start+size).
 * Returns NULL if the file cannot be found.
 */
static const struct cbfs_file *cbfs_find_file(const uint8_t *start, size_t size,
					      const char *file_name)
{
	uint32_t offset = 0;
	const struct cbfs_file *file;

	for (offset = 0; cbfs_is_valid(start, offset, size);
	     offset = cbfs_next_offset(start, offset)) {
		int name_len;

		file = (const struct cbfs_file *)(start + offset);
		name_len = cbfs_get_int(&file->offset) - sizeof(*file);
		if (strncmp(cbfs_get_name(file), file_name, name_len) == 0) {
			return file;
		}
	}
	return NULL;
}

/*
 * Returns 1 if the CBFS blob (by start and size) contains an entry in given
 * file_name, otherwise 0.
 */
int cbfs_has_file(const uint8_t *start, size_t size, const char *file_name)
{
	const struct cbfs_file *file = cbfs_find_file(start, size, file_name);
	return file ? 1 : 0;
}
