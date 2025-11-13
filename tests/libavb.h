/* Copyright 2025 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file contain minimum number of define, which allows
 * to compile in vboot_avb_ops.c file and tests AVB callbacks.
 */

#ifndef VBOOT_REFERENCE_TESTS_LIBAVB_H_
#define VBOOT_REFERENCE_TESTS_LIBAVB_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	AVB_IO_RESULT_OK,
	AVB_IO_RESULT_ERROR_IO,
	AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION,
	AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION,
	AVB_IO_RESULT_ERROR_INSUFFICIENT_SPACE,
} AvbIOResult;

typedef struct AvbRSAPublicKeyHeader {
	uint32_t key_num_bits;
	uint32_t n0inv;
} __attribute__((packed)) AvbRSAPublicKeyHeader;

struct AvbOps;
typedef struct AvbOps AvbOps;

struct AvbOps {
	void *user_data;
	AvbIOResult (*read_from_partition)(AvbOps *ops, const char *partition_name,
					   int64_t offset_from_partition, size_t num_bytes,
					   void *buf, size_t *out_num_read);
	AvbIOResult (*get_preloaded_partition)(AvbOps *ops, const char *partition,
					       size_t num_bytes, uint8_t **out_pointer,
					       size_t *out_num_bytes_preloaded);
	AvbIOResult (*read_rollback_index)(AvbOps *ops, size_t rollback_index_slot,
					   uint64_t *out_rollback_index);
	AvbIOResult (*read_is_device_unlocked)(AvbOps *ops, bool *out_is_unlocked);
	AvbIOResult (*get_unique_guid_for_partition)(AvbOps *ops, const char *partition,
						     char *guid_buf, size_t guid_buf_size);
	AvbIOResult (*get_size_of_partition)(AvbOps *ops, const char *partition_name,
					     uint64_t *out_size);
	AvbIOResult (*validate_vbmeta_public_key)(AvbOps *ops, const uint8_t *public_key_data,
						  size_t public_key_length,
						  const uint8_t *public_key_metadata,
						  size_t public_key_metadata_length,
						  bool *out_key_is_trusted);
};

bool avb_rsa_public_key_header_validate_and_byteswap(const AvbRSAPublicKeyHeader *src,
						     AvbRSAPublicKeyHeader *dest);
#endif // VBOOT_REFERENCE_TESTS_LIBAVB_H_
