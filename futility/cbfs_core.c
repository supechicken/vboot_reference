/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Minimal implementation for accessing a Coreboot File System (CBFS) in memory.
 */

#include <string.h>
#include "cbfs_core.h"

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
static int cbfs_is_valid(const uint8_t *buf, uint32_t offset, uint32_t size)
{
	const struct cbfs_file *file;
	uint32_t end;

	if (offset + sizeof(*file) >= size)
		return 0;
	file = (const struct cbfs_file *)(buf + offset);
	if (memcmp(file->magic, CBFS_FILE_MAGIC, sizeof(file->magic)) != 0)
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
static uint32_t cbfs_next_offset(const uint8_t *buf, uint32_t offset)
{
	const struct cbfs_file *file;
	file = (const struct cbfs_file *)(buf + offset);
	return cbfs_aligned(offset + cbfs_get_int(&file->offset) +
			    cbfs_get_int(&file->len));
}

/*
 * Finds a cbfs_file entry by given file name from a CBFS blob (by buf+size).
 * Returns NULL if the file cannot be found.
 */
const struct cbfs_file *cbfs_find(const char *name, const uint8_t *buf,
				  size_t size)
{
	uint32_t offset = 0;
	int name_len = strlen(name);

	for (offset = 0; cbfs_is_valid(buf, offset, size);
	     offset = cbfs_next_offset(buf, offset)) {
		const struct cbfs_file *file = (
				const struct cbfs_file *)(buf + offset);
		int file_name_len = cbfs_get_int(&file->offset) - sizeof(*file);

		if (file_name_len >= name_len &&
		    strncmp(file->filename, name, file_name_len) == 0) {
			return file;
		}
	}
	return NULL;
}

/* Returns pointer to file data inside CBFS */
void *cbfs_get_file(const char *name, const uint8_t *buf, size_t size)
{
	const struct cbfs_file *file = cbfs_find(name, buf, size);
	if (!file)
		return NULL;
	return (uint8_t *)file + cbfs_get_int(&file->offset);
}

/* Returns pointer to file data inside CBFS after if type is correct */
void *cbfs_find_file(
		const char *name, int type, const uint8_t *buf, size_t size)
{
	const struct cbfs_file *file = cbfs_find(name, buf, size);
	if (!file || cbfs_get_int(&file->type) != type)
		return NULL;
	return (uint8_t *)file + cbfs_get_int(&file->offset);
}

/* Decompress from src to dst using algo; returns 0 on success, -1 on failure */
int cbfs_decompress(int algo, const void *src, void *dst, int len)
{
	switch (algo) {
	case CBFS_COMPRESS_NONE:
		memcpy(dst, src, len);
		return 0;
#ifdef CBFS_CORE_WITH_LZMA
	case CBFS_COMPRESS_LZMA:
		if (ulzma(src, dst) != 0) {
			return 0;
		}
		return -1;
#endif
	default:
		return -1;
        }
}
