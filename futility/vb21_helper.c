/*
 * Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2sysincludes.h"
#include "2common.h"
#include "2guid.h"
#include "2rsa.h"
#include "vb21_common.h"
#include "vb21_struct.h"

#include "host_common.h"
#include "host_key2.h"
#include "host_misc2.h"

#include "futility.h"
#include "traversal.h"
#include "vb21_helper.h"

enum futil_file_type futil_vb21_what_file_type_buf(uint8_t *buf, uint32_t len)
{
	struct vb2_public_key key;
	if (VB2_SUCCESS == vb21_unpack_key(&key, buf, len))
		return FILE_TYPE_VB21_PUBKEY;

	return FILE_TYPE_UNKNOWN;
}

int futil_cb_vb21_show_pubkey(struct futil_traverse_state_s *state)
{
	struct vb2_public_key key;
	char buf[VB2_GUID_MIN_STRLEN];

	if (VB2_SUCCESS != vb21_unpack_key(&key, state->my_area->buf,
					   state->my_area->len))
		return 1;

	if (VB2_SUCCESS != vb2_guid_to_str(key.guid, buf, sizeof(buf)))
		return 1;

	printf("Vbpubk2:                 %s\n", state->in_filename);
	printf("  Version:               0x%08x\n", key.version);
	printf("  Desc:                  \"%s\"\n", key.desc);
	printf("  Signature Algorithm:   %d %s\n", key.sig_alg,
	       (key.sig_alg < kNumAlgorithms ?
		algo_strings[key.sig_alg] : "(invalid)"));
	printf("  Hash Algorithm:        %d\n", key.hash_alg);
	printf("  GUID:                  %s\n", buf);

	return 0;
}
