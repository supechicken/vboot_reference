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

static int get_l(struct vba_context *ctx, uint8_t *l)
{
	int digest_size = vb2_digest_size(VB2_HASH_SHA256);

	if (!ctx->kernel_data_key)
		return !BDB_SUCCESS;

	if (bdb_check_key(ctx->kernel_data_key, ctx->kernel_data_key_size))
		return !BDB_SUCCESS;

	if (vb2_digest_buffer((const uint8_t *)ctx->kernel_data_key,
			      ctx->kernel_data_key->struct_size,
			      VB2_HASH_SHA256, l, digest_size))
		return !BDB_SUCCESS;

	memcpy(l + digest_size, secret_constant_l, sizeof(secret_constant_l));

	return BDB_SUCCESS;
}

static int get_q(struct vba_context *ctx, uint8_t *q)
{
	int digest_size = vb2_digest_size(VB2_HASH_SHA256);

	if (!ctx->kdb_key)
		return !BDB_SUCCESS;

	if (bdb_check_key(ctx->kdb_key, ctx->kdb_key_size))
		return !BDB_SUCCESS;

	if (vb2_digest_buffer((const uint8_t *)ctx->kdb_key,
			      ctx->kdb_key->struct_size,
			      VB2_HASH_SHA256, q, digest_size))
		return !BDB_SUCCESS;
	memcpy(q + digest_size, secret_constant_q, sizeof(secret_constant_q));

	return BDB_SUCCESS;
}

int vba_derive_secrets(struct vba_context *ctx)
{
	/* Derive BUC secret from boot verified secret */
	vb2_sha256_extend(ctx->ro_secrets->boot_verified,
			  secret_constant_c, ctx->rw_secrets->buc);
	return BDB_SUCCESS;
}

/*
 * TODO: Should this function fail if any of extensions fail?
 */
int vba_update_secrets(struct vba_context *ctx)
{
	uint8_t c[BDB_CONSTANT_BLOCK_SIZE];

	/* Clear NVM-RW secret */
	memset(ctx->ro_secrets->nvm_rw, 0, BDB_SECRET_SIZE);

	/* Extend boot verified secret */
	vb2_sha256_extend(ctx->ro_secrets->boot_verified, get_kvx(ctx),
			  ctx->ro_secrets->boot_verified);

	/* Extend boot path secret */
	if (get_l(ctx, c) == BDB_SUCCESS)
		vb2_sha256_extend(ctx->ro_secrets->boot_path, c,
				  ctx->ro_secrets->boot_path);
	else
		memset(ctx->ro_secrets->boot_path, 0, BDB_SECRET_SIZE);

	/* Extend BDB secret */
	if (get_q(ctx, c) == BDB_SUCCESS)
		vb2_sha256_extend(ctx->ro_secrets->bdb, c,
				  ctx->ro_secrets->bdb);
	else
		memset(ctx->ro_secrets->bdb, 0, BDB_SECRET_SIZE);

	/* Extend WSR */

	return BDB_SUCCESS;
}
