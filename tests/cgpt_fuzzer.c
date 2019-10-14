// Copyright 2019 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "cgptlib.h"
#include "gpt.h"

struct MockDisk {
	GptData gpt;
	const uint8_t* data;
	size_t size;
};

static struct MockDisk mock_disk;

vb2_error_t VbExDiskRead(VbExDiskHandle_t h, uint64_t lba_start,
			 uint64_t lba_count, void *buffer)
{
	size_t lba_size = mock_disk.size / mock_disk.gpt.sector_bytes;
	if (lba_start > lba_size || lba_size - lba_start < lba_count) {
		return VB2_ERROR_UNKNOWN;
	}

	size_t start = lba_start * mock_disk.gpt.sector_bytes;
	size_t size = lba_count * mock_disk.gpt.sector_bytes;

	memcpy(buffer, &mock_disk.data[start], size);
	return VB2_SUCCESS;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
	size_t gpt_size = size < sizeof(GptData) ? size : sizeof(GptData);
	memcpy(&mock_disk.gpt, data, gpt_size);
	mock_disk.data = data + gpt_size;
	mock_disk.size = size - gpt_size;

	if (0 != AllocAndReadGptData(0, &mock_disk.gpt)) {
		return 0;
	}

	if (GPT_SUCCESS != GptInit(&mock_disk.gpt)) {
		return 0;
	}

	int result;
	do {
		uint64_t part_start, part_size;
		result = GptNextKernelEntry(&mock_disk.gpt, &part_start,
					    &part_size);
	} while (GPT_SUCCESS == result);

	return 0;
}
