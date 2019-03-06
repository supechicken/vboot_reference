/* Copyright 2019 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Recommended sizes of vboot work buffers for verification stages.
 * This is split into a different file so that it can be imported into
 * linker scripts without including the entire vboot API.
 */

#ifndef VBOOT_REFERENCE_WORKBUF_SIZE_H_
#define VBOOT_REFERENCE_WORKBUF_SIZE_H_

/*
 * Recommended size for firmware verification stage.
 *
 * TODO: The recommended size really depends on which key algorithms are
 * used.  Should have a better / more accurate recommendation than this.
 */
#define VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE (12 * 1024)

/* TODO: Should be deprecated and removed once unused in coreboot repository. */
#define VB2_WORKBUF_RECOMMENDED_SIZE VB2_FIRMWARE_WORKBUF_RECOMMENDED_SIZE

/*
 * Recommended size for kernel verification stage.
 *
 * This is bigger because vboot 2.0 kernel preambles are usually padded to
 * 64 KB.
 *
 * TODO: The recommended size really depends on which key algorithms are
 * used.  Should have a better / more accurate recommendation than this.
 */
#define VB2_KERNEL_WORKBUF_RECOMMENDED_SIZE (80 * 1024)

#endif  /* VBOOT_REFERENCE_WORKBUF_SIZE_H_ */
