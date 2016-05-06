/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2sysincludes.h"
#include "bdb_api.h"
#include "bdb_struct.h"
#include "nvm.h"
#include "2hmac.h"
#include "2sha.h"
#include "bdb.h"

static int nvm_verify(struct vba_context *ctx, const void *buf, uint32_t size)
{
	uint8_t mac[NVM_HMAC_SIZE];
	struct nvmrw *nvm = (struct nvmrw *)buf;

	if (!ctx || !buf)
		return BDB_ERROR_NVM_INVALID_PARAMETER;

	if (!ctx->secrets)
		return BDB_ERROR_NVM_INVALID_SECRET;

	/* Check size */
	if (nvm->struct_size != size)
		return BDB_ERROR_NVM_STRUCT_SIZE;

	/* Compute and verify HMAC */
	if (hmac(VB2_HASH_SHA256, ctx->secrets->nvm_rw_secret, BDB_SECRET_SIZE,
		 buf, size - sizeof(mac), mac, sizeof(mac)))
		return BDB_ERROR_NVM_RW_HMAC;
	if (memcmp(mac, nvm->hmac, sizeof(mac)))
		return BDB_ERROR_NVM_RW_INVALID_HMAC;

	/* TODO: Validate struct fields (magic, size, version, ...) */

	return BDB_SUCCESS;
}

int nvm_write(struct vba_context *ctx, enum nvm_type type)
{
	struct nvmrw *nvm = &ctx->nvmrw;
	int retry = 2;

	if (!ctx)
		return BDB_ERROR_NVM_INVALID_PARAMETER;

	if (!ctx->secrets)
		return BDB_ERROR_NVM_INVALID_SECRET;

	/* Update HMAC */
	hmac(VB2_HASH_SHA256, ctx->secrets->nvm_rw_secret, BDB_SECRET_SIZE,
	     nvm, nvm->struct_size - sizeof(nvm->hmac),
	     nvm->hmac, sizeof(nvm->hmac));

	while (retry--) {
		uint8_t buf[sizeof(struct nvmrw)];
		if (vbe_write_nvm(type, nvm, nvm->struct_size))
			continue;
		if (vbe_read_nvm(type, buf, sizeof(buf)))
			continue;
		if (memcmp(buf, nvm, sizeof(buf)))
			continue;
		/* Write success */
		return BDB_SUCCESS;
	}

	/* NVM seems corrupted. Go to chip recovery mode */
	return BDB_ERROR_NVM_WRITE;
}

int nvm_rw_read(struct vba_context *ctx)
{
	struct nvmrw nvm1, nvm2;
	int rv1, rv2;

	/* Read and verify the 1st copy */
	rv1 = vbe_read_nvm(NVM_TYPE_RW_PRIMARY, (uint8_t *)&nvm1,
			   NVM_MIN_STRUCT_SIZE);
	if (rv1 == BDB_SUCCESS) {
		rv1 = vbe_read_nvm(NVM_TYPE_RW_PRIMARY, (uint8_t *)&nvm1,
				   nvm1.struct_size);
		if (rv1 == BDB_SUCCESS)
			rv1 = nvm_verify(ctx, (uint8_t *)&nvm1,
					 nvm1.struct_size);
	}

	/* Read and verify the 2nd copy */
	rv2 = vbe_read_nvm(NVM_TYPE_RW_SECONDARY, (uint8_t *)&nvm2,
			   NVM_MIN_STRUCT_SIZE);
	if (rv2 == BDB_SUCCESS) {
		rv2 = vbe_read_nvm(NVM_TYPE_RW_SECONDARY, (uint8_t *)&nvm2,
				   nvm2.struct_size);
		if (rv2 == BDB_SUCCESS)
			rv2 = nvm_verify(ctx, (uint8_t *)&nvm2,
					 nvm2.struct_size);
	}

	if (rv1 == BDB_SUCCESS && rv2 == BDB_SUCCESS) {
		/* Sync primary and secondary based on update_count. */
		if (nvm1.update_count > nvm2.update_count)
			rv2 = !BDB_SUCCESS;
		else if (nvm1.update_count < nvm2.update_count)
			rv1 = !BDB_SUCCESS;
	} else if (rv1 != BDB_SUCCESS && rv2 != BDB_SUCCESS){
		/* Abort */
		return BDB_ERROR_NVM_RW_BOTH;
	}

	/* Overwrite one with the other. We don't care about write failure. */
	if (rv1 != BDB_SUCCESS) {
		memcpy(&ctx->nvmrw, &nvm2, nvm2.struct_size);
		nvm_write(ctx, NVM_TYPE_RW_PRIMARY);
	} else if (rv2 != BDB_SUCCESS){
		memcpy(&ctx->nvmrw, &nvm1, nvm1.struct_size);
		nvm_write(ctx, NVM_TYPE_RW_SECONDARY);
	} else {
		/* Pick primary copy, assuming the copies are equal */
		memcpy(&ctx->nvmrw, &nvm1, nvm1.struct_size);
	}

	return BDB_SUCCESS;
}

static int nvm_init(struct vba_context *ctx)
{
	if (nvm_rw_read(ctx))
		return BDB_ERROR_NVM_INIT;

	return BDB_SUCCESS;
}

/**
 * This is the function called from SP-RW, which receives a kernel version
 * from an AP-RW after successful verification of a kernel.
 *
 * It checks whether the
 * version in NVM-RW is older than the reported version or not. If so,
 * it updates the version in NVM-RW.
 */
int vba_update_kernel_version(struct vba_context *ctx,
			      uint32_t kernel_data_key_version,
			      uint32_t kernel_version)
{
	struct nvmrw *nvm;

	if (nvm_verify(ctx, &ctx->nvmrw, sizeof(ctx->nvmrw))) {
		if (nvm_init(ctx))
			return BDB_ERROR_NVM_INIT;
	}

	nvm = &ctx->nvmrw;
	if (nvm->min_kernel_data_key_version < kernel_data_key_version ||
			nvm->min_kernel_version < kernel_version) {
		nvm->min_kernel_data_key_version = kernel_data_key_version;
		nvm->min_kernel_version = kernel_version;

		/* Update counter */
		nvm->update_count++;
		if (nvm_write(ctx, NVM_TYPE_RW_PRIMARY) ||
				nvm_write(ctx, NVM_TYPE_RW_SECONDARY)) {
			return BDB_ERROR_RECOVERY_REQUEST;
		}
	}

	return BDB_SUCCESS;
}
