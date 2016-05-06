/* Copyright 2016 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_FIRMWARE_BDB_BDB_API_H
#define VBOOT_REFERENCE_FIRMWARE_BDB_BDB_API_H

#include <stdint.h>
#include "vboot_register.h"
#include "nvm.h"

struct vba_context {
	/* Indicate which slot is being tried: 0 - primary, 1 - secondary */
	uint8_t slot;

	/* BDB */
	uint8_t *bdb;

	/* Secrets */
	struct bdb_secret *secrets;

	/* NVM-RW buffer */
	struct nvmrw *nvmrw;
};

/**
 * Initialize vboot process
 *
 * @param ctx
 * @return	enum bdb_return_code
 */
int vba_bdb_init(struct vba_context *ctx);

/**
 * Finalize vboot process
 *
 * @param ctx
 * @return	enum bdb_return_code
 */
int vba_bdb_finalize(struct vba_context *ctx);

/**
 * Log failed boot attempt and reset the chip
 *
 * @param ctx
 */
void vba_bdb_fail(struct vba_context *ctx);

int vba_update_kernel_version(struct vba_context *ctx,
			      uint32_t kernel_data_key_version,
			      uint32_t kernel_version);

/**
 * Get vboot register value
 *
 * Implemented by each chip
 *
 * @param type	Type of register to get
 * @return	Register value
 */
uint32_t vbe_get_vboot_register(enum vboot_register type);

/**
 * Set vboot register value
 *
 * Implemented by each chip
 *
 * @param type	Type of register to set
 * @param val	Value to set
 */
void vbe_set_vboot_register(enum vboot_register type, uint32_t val);

/**
 * Reset the SoC
 *
 * Implemented by each chip. This is different from reboot (a.k.a. board reset,
 * cold reset).
 */
void vbe_reset(void);

/**
 * Reads contents of NVM
 *
 * Implemented by each chip.
 *
 * @param type	Type of NVM
 * @param buf	Buffer where NVM content will be stored
 * @param size	(IN) Buffer size (OUT) Number of bytes read from NVM
 * @return
 */
int vbe_read_nvm(enum nvm_type type, uint8_t **buf, uint32_t *size);

int vbe_write_nvm(enum nvm_type type, void *buf, uint32_t size);

#endif
