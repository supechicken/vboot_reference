/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Intel Flash Descriptor (IFD) tools
 */
#ifndef VBOOT_REFERENCE_FUTILITY_IFD_H_
#define VBOOT_REFERENCE_FUTILITY_IFD_H_

#include <stdint.h>
#include "updater_utils.h"

/* Structure from coreboot util/ifdtool/ifdtool.h */
// flash descriptor
struct fdbar {
	uint32_t flvalsig;
	uint32_t flmap0;
	uint32_t flmap1;
	uint32_t flmap2;
	uint32_t flmap3; // Exist for 500 series onwards
} __attribute__((packed));

// flash master
struct fmba {
	uint32_t flmstr1;
	uint32_t flmstr2;
	uint32_t flmstr3;
	uint32_t flmstr4;
	uint32_t flmstr5;
	uint32_t flmstr6;
} __attribute__((packed));

struct fmba *find_fmba(struct firmware_image *image);

bool is_flash_descriptor_locked(struct firmware_image *image);

#endif  /* VBOOT_REFERENCE_FUTILITY_IFD_H_ */
