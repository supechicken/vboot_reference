/* Copyright 2022 The ChromiumOS Authors.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2return_codes.h"
#include "2sha.h"

#define ENV_CBFSTOOL "CBFSTOOL"
#define DEFAULT_CBFSTOOL "cbfstool"

vb2_error_t cbfstool_truncate(const char *file, const char *region,
			      size_t *new_size);

/* Check whether image under `file` path supports CBFS_VERIFICATION,
 * and contains metadata hash. Function sets `*hash_found` to zero if metadata
 * hash was not found. If hash was found, `*hash_found` is set to non-zero
 * value, and if `hash` is not NULL, then it's set to metadata hash value.
 *
 * If `region` is NULL, then region option will not be passed. (default region)
 */
vb2_error_t cbfstool_get_metadata_hash(const char *file, const char *region,
				       int *hash_found, struct vb2_hash *hash);
