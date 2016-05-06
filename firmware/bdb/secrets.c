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

static const uint8_t *get_kvx(struct vba_context *ctx)
{
	if (ctx->kernel_data_key_verified)
		return secret_constant_kv1;
	else
		return secret_constant_kv0;
}

static void get_l(struct vba_context *ctx, uint8_t *l)
{
	struct bdb_key *kernel_data_key;

	memcpy(l, secret_constant_l, sizeof(secret_constant_l));
	vb2_digest_buffer(kernel_data_key, sizeof(*kernel_data_key),
			  VB2_HASH_SHA256, l, vb2_digest_size(VB2_HASH_SHA256));
}

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
	uint8_t l[BDB_CONSTANT_BLOCK_SIZE];

	/* Clear NVM-RW secret */
	memset(ctx->ro_secrets->nvm_rw, 0, BDB_SECRET_SIZE);

	/* Extend boot verified secret */
	if (vb2_sha256_extend(ctx->ro_secrets->boot_verified, get_kvx(ctx),
			      ctx->ro_secrets->boot_verified))
		return BDB_ERROR_SECRET_BOOT_VERIFIED;

	/* Extend boot path secret */
	if (vb2_sha256_extend(ctx->ro_secrets->boot_path, get_l(ctx, l),
			      ctx->ro_secrets->boot_path))
		return BDB_ERROR_SECRET_BOOT_PATH;

	/* Extend BDB secret */
	if (vb2_sha256_extend(ctx->ro_secrets->bdb, digest_of_bdb_key,
			      ctx->ro_secrets->bdb))
		return BDB_ERROR_SECRET_BDB;

	/* Extend WSR */

	return BDB_SUCCESS;
}
