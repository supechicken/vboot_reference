/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This defines Android device tree table and entry headers as documented in:
 * https://source.android.com/docs/core/architecture/dto/partitions
 */
#ifndef VBOOT_REFERENCE_VB2_ANDROID_DTIMG_H_
#define VBOOT_REFERENCE_VB2_ANDROID_DTIMG_H_

#include <stdint.h>

#define DT_TABLE_MAGIC 0xd7b7ab1e
#define DT_TABLE_MAGIC_SIZE 4

struct dt_table_header {
	uint32_t magic;			// DT_TABLE_MAGIC
	uint32_t total_size;		// includes dt_table_header + all dt_table_entry
					// and all dtb/dtbo
	uint32_t header_size;		// sizeof(dt_table_header)

	uint32_t dt_entry_size;		// sizeof(dt_table_entry)
	uint32_t dt_entry_count;	// number of dt_table_entry
	uint32_t dt_entries_offset;	// offset to the first dt_table_entry
					// from head of dt_table_header

	uint32_t page_size;		// flash page size we assume
	uint32_t version;		// DTBO image version, the current version is 0.
					// The version is incremented when the
					// dt_table_header struct is updated.
};

struct dt_table_entry {
	uint32_t dt_size;
	uint32_t dt_offset;	// offset from head of dt_table_header

	uint32_t id;		// optional, must be zero if unused
	uint32_t rev;		// optional, must be zero if unused
	uint32_t custom[4];	// optional, must be zero if unused
};

#endif /* VBOOT_REFERENCE_VB2_ANDROID_DTIMG_H_ */
