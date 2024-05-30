/* Copyright 2015 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Exports a vmlinuz from a kernel partition in memory.
 */

#include <stdlib.h>
#include <string.h>

#include "2common.h"
#include "2struct.h"
#include "vboot_host.h"
#include "vboot_struct.h"

int ExtractVmlinuz(void *kpart_data, size_t kpart_size,
		   void **vmlinuz_out, size_t *vmlinuz_size) {
	// We're going to be extracting `vmlinuz_header` and
	// `kblob_data`, and returning the concatenation of them.
	//
	// kpart_data = +-[kpart_size]------------------------------------+
	//              |                                                 |
	//  keyblock  = | +-[keyblock->keyblock_size]-------------------+ |
	//              | | struct vb2_keyblock          keyblock       | |
	//              | | char []                      ...data...     | |
	//              | +---------------------------------------------+ |
	//              |                                                 |
	//  preamble  = | +-[preamble->preamble_size]-------------------+ |
	//              | | struct vb2_kernel_preamble   preamble       | |
	//              | | char []                       ...data...    | |
	//              | | char []                      vmlinuz_header | |
	//              | | char []                       ...data...    | |
	//              | +---------------------------------------------+ |
	//              |                                                 |
	//  kblob_data= | +-[preamble->body_signature.data_size]--------+ |
	//              | | char []                       ...data...    | |
	//              | +---------------------------------------------+ |
	//              |                                                 |
	//              +-------------------------------------------------+

	size_t now = 0;
	// The 3 sections of kpart_data.
	struct vb2_keyblock *keyblock = NULL;
	struct vb2_kernel_preamble *preamble = NULL;
	uint8_t *kblob_data = NULL;
	uint32_t kblob_size = 0;
	// vmlinuz_header
	uint8_t *vmlinuz_header = NULL;
	uint32_t vmlinuz_header_size = 0;
	// The concatenated result.
	void *vmlinuz = NULL;

	// Isolate the 3 sections of kpart_data.

	keyblock = (struct vb2_keyblock *)kpart_data;
	now += keyblock->keyblock_size;
	if (now > kpart_size)
		return 1;

	preamble = (struct vb2_kernel_preamble *)(kpart_data + now);
	now += preamble->preamble_size;
	if (now > kpart_size)
		return 1;

	kblob_data = kpart_data + now;
	kblob_size = preamble->body_signature.data_size;
	now += kblob_size;
	if (now > kpart_size)
		return 1;

	// Find `vmlinuz_header` within `preamble`.

	if (preamble->header_version_minor > 0) {
		// calculate the vmlinuz_header offset from
		// the beginning of the kpart_data.  The kblob doesn't
		// include the body_load_offset, but does include
		// the keyblock and preamble sections.
		size_t vmlinuz_header_offset =
			preamble->vmlinuz_header_address -
			preamble->body_load_address +
			keyblock->keyblock_size +
			preamble->preamble_size;

		vmlinuz_header = kpart_data + vmlinuz_header_offset;
		vmlinuz_header_size = preamble->vmlinuz_header_size;
	}

	if (!vmlinuz_header ||
	    !vmlinuz_header_size ||
	    vmlinuz_header + vmlinuz_header_size > kblob_data) {
		return 1;
	}

	// Concatenate and return.

	vmlinuz = malloc(vmlinuz_header_size + kblob_size);
	if (vmlinuz == NULL)
		return 1;
	memcpy(vmlinuz, vmlinuz_header, vmlinuz_header_size);
	memcpy(vmlinuz + vmlinuz_header_size, kblob_data, kblob_size);

	*vmlinuz_out = vmlinuz;
	*vmlinuz_size = vmlinuz_header_size + kblob_size;

	return 0;
}
