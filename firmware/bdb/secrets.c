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

static int get_constant(const uint8_t *buf, uint32_t buf_size,
			const uint8_t *constant, uint8_t *out)
{
	int digest_size = vb2_digest_size(VB2_HASH_SHA256);
	const struct bdb_key *key = (const struct bdb_key *)buf;

	if (!buf)
		return !BDB_SUCCESS;

	if (bdb_check_key(key, buf_size))
		return !BDB_SUCCESS;

	if (vb2_digest_buffer(buf, buf_size, VB2_HASH_SHA256, out, digest_size))
		return !BDB_SUCCESS;

	memcpy(out + digest_size, constant,
	       BDB_CONSTANT_BLOCK_SIZE - digest_size);

	return BDB_SUCCESS;
}

int vba_derive_secret_ro(struct vba_context *ctx, enum bdb_secret_type type,
			 uint8_t *wsr, const uint8_t *buf, uint32_t buf_size,
			 void (*extend)(const uint8_t *from, const uint8_t *by,
					 uint8_t *to))
{
	uint8_t c[BDB_CONSTANT_BLOCK_SIZE];
	uint8_t *from;
	const uint8_t *by = (const uint8_t *)c;
	uint8_t *to;

	switch (type) {
	case BDB_SECRET_TYPE_WSR:
		from = to = wsr;
		by = secret_constant_x;
		break;
	case BDB_SECRET_TYPE_BDB:
		from = wsr;
		to = ctx->secrets->bdb;
		if (get_constant(buf, buf_size, secret_constant_p, c))
			return BDB_ERROR_SECRET_BDB;
		break;
	case BDB_SECRET_TYPE_BOOT_PATH:
		from = wsr;
		to = ctx->secrets->boot_path;
		if (get_constant(buf, buf_size, secret_constant_k, c))
			return BDB_ERROR_SECRET_BOOT_PATH;
		break;
	case BDB_SECRET_TYPE_BOOT_VERIFIED:
		from = wsr;
		to = ctx->secrets->boot_verified;
		if (ctx->flags & VBA_CONTEXT_FLAG_BDB_KEY_EFUSED)
			by = secret_constant_fv0;
		else
			by = secret_constant_fv1;
		break;
	case BDB_SECRET_TYPE_NVM_WP:
		from = wsr;
		by = secret_constant_a;
		to = ctx->secrets->nvm_wp;
		break;
	case BDB_SECRET_TYPE_NVM_RW:
		from = ctx->secrets->nvm_wp;
		by = secret_constant_b;
		to = ctx->secrets->nvm_rw;
		break;
	default:
		return BDB_ERROR_SECRET_TYPE;
	}

	if (extend)
		extend(from, by, to);
	else
		vb2_sha256_extend(from, by, to);

	return BDB_SUCCESS;
}

int vba_derive_secret(struct vba_context *ctx, enum bdb_secret_type type,
		      uint8_t *wsr, const uint8_t *buf, uint32_t buf_size)
{
	uint8_t c[BDB_CONSTANT_BLOCK_SIZE];
	uint8_t *from;
	const uint8_t *by = (const uint8_t *)c;
	uint8_t *to;

	switch (type) {
	case BDB_SECRET_TYPE_WSR:
		from = to = wsr;
		//by = secret_constant_y;
		break;
	case BDB_SECRET_TYPE_BDB:
		from = to = ctx->secrets->bdb;
		if (get_constant(buf, buf_size, secret_constant_q, c))
			return BDB_ERROR_SECRET_BDB;
		break;
	case BDB_SECRET_TYPE_BOOT_PATH:
		from = to = ctx->secrets->boot_path;
		if (get_constant(buf, buf_size, secret_constant_l, c))
			return BDB_ERROR_SECRET_BOOT_PATH;
		break;
	case BDB_SECRET_TYPE_BOOT_VERIFIED:
		from = to = ctx->secrets->boot_verified;
		if (ctx->flags & VBA_CONTEXT_FLAG_KERNEL_DATA_KEY_VERIFIED)
			by = secret_constant_kv1;
		else
			by = secret_constant_kv0;
		break;
	case BDB_SECRET_TYPE_BUC:
		from = ctx->secrets->boot_verified;
		by = secret_constant_c;
		to = ctx->secrets->buc;
		break;
	default:
		return BDB_ERROR_SECRET_TYPE;
	}

	vb2_sha256_extend(from, by, to);

	return BDB_SUCCESS;
}

int vba_clear_secret(struct vba_context *ctx, enum bdb_secret_type type)
{
	uint8_t *s;

	switch (type) {
	case BDB_SECRET_TYPE_NVM_RW:
		s = ctx->secrets->nvm_rw;
		break;
	case BDB_SECRET_TYPE_BDB:
		s = ctx->secrets->bdb;
		break;
	case BDB_SECRET_TYPE_BOOT_PATH:
		s = ctx->secrets->boot_path;
		break;
	case BDB_SECRET_TYPE_BOOT_VERIFIED:
		s = ctx->secrets->boot_verified;
		break;
	case BDB_SECRET_TYPE_BUC:
		s = ctx->secrets->buc;
		break;
	default:
		return BDB_ERROR_SECRET_TYPE;
	}

	memset(s, 0, BDB_SECRET_SIZE);
	return BDB_SUCCESS;
}
