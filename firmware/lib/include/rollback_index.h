/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Functions for querying, manipulating and locking rollback indices
 * stored in the TPM NVRAM.
 */

#ifndef VBOOT_REFERENCE_ROLLBACK_INDEX_H_
#define VBOOT_REFERENCE_ROLLBACK_INDEX_H_

#include "2common.h"
#include "2constants.h"
#include "2return_codes.h"
#include "2secdata.h"
#include "sysincludes.h"
#include "tss_constants.h"

/* TPM NVRAM location indices. */
#define FIRMWARE_NV_INDEX 0x1007
#define KERNEL_NV_INDEX 0x1008
#define FWMP_NV_INDEX 0x100a
#define REC_HASH_NV_INDEX 0x100b
#define REC_HASH_NV_SIZE VB2_SHA256_DIGEST_SIZE

/* Structure definitions for TPM spaces */

/* Kernel space - KERNEL_NV_INDEX, locked with physical presence. */
typedef struct RollbackSpaceKernel {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Unique ID to detect space redefinition */
	uint32_t uid;
	/* Kernel versions */
	uint32_t kernel_versions;
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* Checksum (v2 and later only) */
	uint8_t crc8;
} __attribute__((packed)) RollbackSpaceKernel;

/* Firmware space - FIRMWARE_NV_INDEX, locked with global lock. */
typedef struct RollbackSpaceFirmware {
	/* Struct version, for backwards compatibility */
	uint8_t struct_version;
	/* Flags (see FLAG_* above) */
	uint8_t flags;
	/* Firmware versions */
	uint32_t fw_versions;
	/* Reserved for future expansion */
	uint8_t reserved[3];
	/* Checksum (v2 and later only) */
	uint8_t crc8;
} __attribute__((packed)) RollbackSpaceFirmware;

/* Firmware management parameters */
struct RollbackSpaceFwmp {
	/* CRC-8 of fields following struct_size */
	uint8_t crc;
	/* Structure size in bytes */
	uint8_t struct_size;
	/* Structure version */
	uint8_t struct_version;
	/* Reserved; ignored by current reader */
	uint8_t reserved0;
	/* Flags; see enum vb2_secdata_fwmp_flags */
	uint32_t flags;
	/* Hash of developer kernel key */
	uint8_t dev_key_hash[VB2_SECDATA_FWMP_HASH_SIZE];
} __attribute__((packed));

/* All functions return TPM_SUCCESS (zero) if successful, non-zero if error */

/*
 * These functions are callable from VbSelectAndLoadKernel().  They may use
 * global variables.
 */

uint32_t ReadSpaceFirmware(RollbackSpaceFirmware *rsf);
uint32_t WriteSpaceFirmware(RollbackSpaceFirmware *rsf);
uint32_t ReadSpaceKernel(RollbackSpaceKernel *rsk);
uint32_t WriteSpaceKernel(RollbackSpaceKernel *rsk);

/**
 * Lock kernel space.
 */
uint32_t RollbackKernelLock(void);

/**
 * Read firmware management parameters.
 *
 * Returns non-zero if error.
 */
uint32_t RollbackFwmpRead(struct vb2_context *ctx);

/****************************************************************************/

/*
 * The following functions are internal apis, listed here for use by unit tests
 * only.
 */

/**
 * Issue a TPM_Clear and reenable/reactivate the TPM.
 */
uint32_t TPMClearAndReenable(void);

/**
 * Like TlclWrite(), but checks for write errors due to hitting the 64-write
 * limit and clears the TPM when that happens.  This can only happen when the
 * TPM is unowned, so it is OK to clear it (and we really have no choice).
 * This is not expected to happen frequently, but it could happen.
 */
uint32_t SafeWrite(uint32_t index, const void *data, uint32_t length);

/**
 * Utility function to turn the virtual dev-mode flag on or off. 0=off, 1=on.
 */
vb2_error_t SetVirtualDevMode(struct vb2_context *ctx, int value);

#endif  /* VBOOT_REFERENCE_ROLLBACK_INDEX_H_ */
