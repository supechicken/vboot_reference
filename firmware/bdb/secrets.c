/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2sysincludes.h"
#include "2hmac.h"
#include "2sha.h"
#include "bdb_api.h"
#include "bdb_struct.h"
#include "bdb.h"
#include "secrets.h"

int vba_derive_secrets(struct vba_context *ctx)
{
	/* Derive BUC secret from boot verified secret */
	if (vb2_sha256_extend(ctx->ro_secrets->boot_verified,
			      secret_constant_c, ctx->rw_secrets->buc))
		return BDB_ERROR_SECRET_BUC;

	return BDB_SUCCESS;
}

int vba_update_secrets(struct vba_context *ctx)
{
	/* Clear NVM-RW secret */
	memset(ctx->ro_secrets->nvm_rw, 0, BDB_SECRET_SIZE);

	/* Extend boot verified secret */
	if (vb2_sha256_extend(ctx->ro_secrets->boot_verified,
			      secret_constant_kv1,
			      ctx->ro_secrets->boot_verified))
		return BDB_ERROR_SECRET_BOOT_VERIFIED;

	/* Extend boot path secret */
	if (vb2_sha256_extend(ctx->ro_secrets->boot_path, digest_of_data_key,
			      ctx->ro_secrets->boot_path))
		return BDB_ERROR_SECRET_BOOT_PATH;

	/* Extend BDB secret */
	if (vb2_sha256_extend(ctx->ro_secrets->bdb, digest_of_bdb_key,
			      ctx->ro_secrets->bdb))
		return BDB_ERROR_SECRET_BDB;

	/* Extend WSR */

	return BDB_SUCCESS;
}
