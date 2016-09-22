/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Boot descriptor block helper functions
 */

#include <stdio.h>

#include "2sha.h"
#include "bdb.h"
#include "bdb_struct.h"
#include "file_type.h"

enum futil_file_type ft_recognize_bdb(uint8_t *buf, uint32_t len)
{
	const struct bdb_header *header = bdb_get_header(buf);

	if (bdb_check_header(header, len))
		return FILE_TYPE_UNKNOWN;

	return FILE_TYPE_BDB;
}

int ft_show_bdb(const char *name, uint8_t *buf, uint32_t len, void *data)
{
	const struct bdb_header *header = bdb_get_header(buf);
	const struct bdb_key *key = bdb_get_bdbkey(buf);
	uint8_t digest[BDB_SHA256_DIGEST_SIZE];
	int i;

	/* We can get here because of '--type' option */
	if (bdb_check_header(header, len)) {
		fprintf(stderr, "ERROR: Invalid BDB blob\n");
		return 1;
	}

	printf("Boot Descriptor Block: %s\n", name);
	printf("Struct Version:        0x%x:0x%x\n",
	       header->struct_major_version, header->struct_minor_version);

	vb2_digest_buffer((uint8_t *)key, key->struct_size, VB2_HASH_SHA256,
			  digest, sizeof(digest));
	printf("BDB key digest:       ");
	for (i = 0; i < sizeof(digest); i++)
		printf(" %02x", digest[i]);
	printf("\n");

	return 0;
}
