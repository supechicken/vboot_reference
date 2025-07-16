/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stddef.h>
#include <stdint.h>

#define ENV_CBMEM "CBMEM"
#define DEFAULT_CBMEM "cbmem"

/*
 * Extracts rawdump of a specific section.
 *
 * @param id        Id of the section.
 * @param buffer    Buffer to write the extracted rawdump.
 * @param count     Size of the allocated buffer. Will be set to the number of bytes read.
 *
 * @return 0 on success, non-zero on failure.
 */
int cbmem_get_rawdump(const char *id, uint8_t *buffer, size_t *count);
