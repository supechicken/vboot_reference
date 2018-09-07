/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Minimal implementation for Coreboot File System (CBFS).
 */

#include <arpa/inet.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "cbfs.h"

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

static uint32_t cbfs_aligned(uint32_t offset)
{
	uint32_t remains = offset % CBFS_ALIGNMENT;
	return remains ? offset - remains + CBFS_ALIGNMENT : offset;
}

static uint32_t cbfs_get_int(uint32_t be_int)
{
	return ntohl(be_int);
}

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
	end = offset + cbfs_get_int(file->offset) + cbfs_get_int(file->len);
	if (end > size)
		return 0;
	return 1;
}

static uint32_t cbfs_next_offset(const uint8_t *start, uint32_t offset)
{
	const struct cbfs_file *file;
	file = (const struct cbfs_file *)(start + offset);
	return cbfs_aligned(offset + cbfs_get_int(file->offset) +
			    cbfs_get_int(file->len));
}

static const char *cbfs_get_name(const struct cbfs_file *file)
{
	return (const char *)(file + 1);
}

static const struct cbfs_file *cbfs_find_file(const uint8_t *start, size_t size,
					      const char *file_name)
{
	uint32_t offset = 0;
	const struct cbfs_file *file;

	for (offset = 0; cbfs_is_valid(start, offset, size);
	     offset = cbfs_next_offset(start, offset)) {
		int name_len;

		file = (const struct cbfs_file *)(start + offset);
		name_len = cbfs_get_int(file->offset) - sizeof(*file);
		if (strncmp(cbfs_get_name(file), file_name, name_len) == 0) {
			return file;
		}
	}
	return NULL;
}

int cbfs_has_file(const uint8_t *start, size_t size, const char *file_name)
{
	const struct cbfs_file *file = cbfs_find_file(start, size, file_name);
	return file ? 1 : 0;
}
