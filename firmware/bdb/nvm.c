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
#include "nvm.h"
#include "secrets.h"

static int nvmrw_validate(const struct nvmrw *nvm)
{
	if (nvm->struct_magic != NVM_RW_MAGIC)
		return BDB_ERROR_NVM_RW_MAGIC;

	if (nvm->struct_major_version != NVM_HEADER_VERSION_MAJOR)
		return BDB_ERROR_NVM_STRUCT_VERSION;

	/*
	 * We need to handle different minor versions found in a EEPROM
	 * and update it with the current version. We use different sizes for
	 * each version. We probably want something like nvm_get_struct_size,
	 * which takes minor version and returns the expected struct size.
	 */
	if (nvm->struct_size != sizeof(*nvm))
		return BDB_ERROR_NVM_STRUCT_SIZE;

	return BDB_SUCCESS;
}

static int nvmrw_verify(const struct bdb_ro_secrets *secrets, const void *buf)
{
	uint8_t mac[NVM_HMAC_SIZE];
	struct nvmrw *nvm = (struct nvmrw *)buf;
	int rv;

	if (!secrets || !buf)
		return BDB_ERROR_NVM_INVALID_PARAMETER;

	rv = nvmrw_validate(nvm);
	if (rv)
		return rv;

	/* Compute and verify HMAC */
	if (hmac(VB2_HASH_SHA256, secrets->nvm_rw, BDB_SECRET_SIZE,
		 buf, nvm->struct_size - sizeof(mac), mac, sizeof(mac)))
		return BDB_ERROR_NVM_RW_HMAC;
	if (memcmp(mac, nvm->hmac, sizeof(mac)))
		return BDB_ERROR_NVM_RW_INVALID_HMAC;

	return BDB_SUCCESS;
}

int nvmrw_write(struct vba_context *ctx, enum nvm_type type)
{
	struct nvmrw *nvm = &ctx->nvmrw;
	int retry = 2;
	int rv;

	if (!ctx)
		return BDB_ERROR_NVM_INVALID_PARAMETER;

	if (!ctx->ro_secrets)
		return BDB_ERROR_NVM_INVALID_SECRET;

	rv = nvmrw_validate(nvm);
	if (rv)
		return rv;

	/* Update HMAC */
	hmac(VB2_HASH_SHA256, ctx->ro_secrets->nvm_rw, BDB_SECRET_SIZE,
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

static int read_verify_nvmrw(enum nvm_type type,
			     const struct bdb_ro_secrets *secrets,
			     struct nvmrw *nvm)
{
	/* Read minimum amount */
	if (vbe_read_nvm(type, (uint8_t *)nvm, NVM_MIN_STRUCT_SIZE))
		return BDB_ERROR_NVM_VBE_READ;

	/* Validate the content */
	if (nvmrw_validate(nvm))
		return BDB_ERROR_NVM_VALIDATE;

	/* Read full amount */
	if (vbe_read_nvm(type, (uint8_t *)nvm, nvm->struct_size))
		return BDB_ERROR_NVM_VBE_READ;

	/* Verify the content */
	if (nvmrw_verify(secrets, (uint8_t *)nvm))
		return BDB_ERROR_NVM_VERIFY;

	return BDB_SUCCESS;
}

int nvmrw_read(struct vba_context *ctx)
{
	struct nvmrw nvm1, nvm2;
	int rv1, rv2;

	/* Read and verify the 1st copy */
	rv1 = read_verify_nvmrw(NVM_TYPE_RW_PRIMARY, ctx->ro_secrets, &nvm1);

	/* Read and verify the 2nd copy */
	rv2 = read_verify_nvmrw(NVM_TYPE_RW_SECONDARY, ctx->ro_secrets, &nvm2);

	if (rv1 == BDB_SUCCESS && rv2 == BDB_SUCCESS) {
		/* Sync primary and secondary based on update_count. */
		if (nvm1.update_count > nvm2.update_count)
			rv2 = !BDB_SUCCESS;
		else if (nvm1.update_count < nvm2.update_count)
			rv1 = !BDB_SUCCESS;
	} else if (rv1 != BDB_SUCCESS && rv2 != BDB_SUCCESS){
		/* Abort. Neither was successful. */
		return rv1;
	}

	/* Overwrite one with the other. We don't care about write failure. */
	if (rv1 != BDB_SUCCESS) {
		memcpy(&ctx->nvmrw, &nvm2, nvm2.struct_size);
		nvmrw_write(ctx, NVM_TYPE_RW_PRIMARY);
	} else if (rv2 != BDB_SUCCESS){
		memcpy(&ctx->nvmrw, &nvm1, nvm1.struct_size);
		nvmrw_write(ctx, NVM_TYPE_RW_SECONDARY);
	} else {
		/* Pick primary copy, assuming the copies are equal */
		memcpy(&ctx->nvmrw, &nvm1, nvm1.struct_size);
	}

	return BDB_SUCCESS;
}

static int nvmrw_init(struct vba_context *ctx)
{
	if (nvmrw_read(ctx))
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
	struct nvmrw *nvm = &ctx->nvmrw;

	if (nvmrw_verify(ctx->ro_secrets, nvm)) {
		if (nvmrw_init(ctx))
			return BDB_ERROR_NVM_INIT;
	}

	if (nvm->min_kernel_data_key_version < kernel_data_key_version ||
			nvm->min_kernel_version < kernel_version) {
		int rv1, rv2;

		/* Roll forward versions */
		nvm->min_kernel_data_key_version = kernel_data_key_version;
		nvm->min_kernel_version = kernel_version;

		/* Increment update counter */
		nvm->update_count++;

		/* Update both copies */
		rv1 = nvmrw_write(ctx, NVM_TYPE_RW_PRIMARY);
		rv2 = nvmrw_write(ctx, NVM_TYPE_RW_SECONDARY);
		if (rv1 && rv2)
			return BDB_ERROR_RECOVERY_REQUEST;
	}

	return BDB_SUCCESS;
}
