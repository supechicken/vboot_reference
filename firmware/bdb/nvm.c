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
	uint8_t mac[VB2_SHA256_DIGEST_SIZE];
	struct nvmrw *nvm = (struct nvmrw *)buf;

	/* Verify HMAC */
	hmac(VB2_HASH_SHA256, ctx->secrets->nvm_rw_secret, BDB_SECRET_SIZE,
	     buf, size - sizeof(mac), mac, sizeof(mac));
	if (memcmp(mac, nvm->hmac, sizeof(mac)))
		return BDB_ERROR_NVM_RW_INVALID_HMAC;

	/* Check struct fields (size, version, ...) */

	return BDB_SUCCESS;
}

static int nvm_read(struct vba_context *ctx)
{
	struct nvmrw *nvm1, *nvm2;
	uint32_t size;
	int rv1, rv2;

	rv1 = vbe_read_nvm(NVM_TYPE_RW_PRIMARY, (uint8_t **)&nvm1, &size);
	if (rv1 == BDB_SUCCESS)
		rv1 = nvm_verify(ctx, nvm1, size);

	rv2 = vbe_read_nvm(NVM_TYPE_RW_SECONDARY, (uint8_t **)&nvm2, &size);
	if (rv2 == BDB_SUCCESS)
		rv2 = nvm_verify(ctx, nvm2, size);

	if (rv1 == BDB_SUCCESS && rv2 == BDB_SUCCESS) {
		/* Sync primary and secondary based on update_count.
		 * TODO: What if they are equal? We don't want to update
		 * at every single boot. */
		if (nvm1->update_count > nvm2->update_count)
			rv2 = !BDB_SUCCESS;
		else
			rv1 = !BDB_SUCCESS;
	} else if (rv1 != BDB_SUCCESS && rv2 != BDB_SUCCESS){
		/* Abort */
		return BDB_ERROR_NVM_RW_BOTH;
	}

	/* Overwrite one with the other. We don't care about write failure. */
	if (rv1 == BDB_SUCCESS) {
		vbe_write_nvm(NVM_TYPE_RW_SECONDARY, nvm1, nvm1->struct_size);
		ctx->nvmrw = nvm1;
	} else {
		vbe_write_nvm(NVM_TYPE_RW_PRIMARY, nvm2, nvm2->struct_size);
		ctx->nvmrw = nvm2;
	}

	return BDB_SUCCESS;
}

static int nvm_write(struct vba_context *ctx)
{
	uint8_t mac[VB2_SHA256_DIGEST_SIZE];
	struct nvmrw *nvm = ctx->nvmrw;

	/* Update HMAC */
	hmac(VB2_HASH_SHA256, ctx->secrets->nvm_rw_secret, BDB_SECRET_SIZE,
	     nvm, nvm->struct_size - sizeof(mac), mac, sizeof(mac));
	memcpy(nvm->hmac, mac, sizeof(mac));

	/* Update counter */
	nvm->update_count++;

	return vbe_write_nvm(NVM_TYPE_RW_PRIMARY, nvm, nvm->struct_size);
}

static int nvm_init(struct vba_context *ctx)
{
	if (nvm_read(ctx))
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

	if (!ctx->nvmrw) {
		if (nvm_init(ctx))
			return BDB_ERROR_NVM_INIT;
	}

	nvm = ctx->nvmrw;
	if (nvm->min_kernel_data_key_version < kernel_data_key_version ||
			nvm->min_kernel_version < kernel_version) {
		nvm->min_kernel_data_key_version = kernel_data_key_version;
		nvm->min_kernel_version = kernel_version;
		/* TODO: Probably want to try a couple of times before fail */
		if (nvm_write(ctx))
			return BDB_ERROR_NVM_WRITE;
	}

	return BDB_SUCCESS;
}
