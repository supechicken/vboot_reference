/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Implementation of callbacks needed by libavb library
*/

#include <libavb.h>

#include "2avb.h"
#include "2common.h"
#include "2misc.h"
#include "2nvstorage.h"
#include "2secdata.h"
#include "cgptlib.h"
#include "cgptlib_internal.h"

struct vboot_avb_data {
	GptData *gpt;
	struct vb2_context *vb2_ctx;
};

static AvbOps avb_ops;
static struct vboot_avb_data vboot_avb;

static AvbIOResult vboot_avb_read_from_partition(AvbOps *ops,
				       const char *partition_name,
				       int64_t offset_from_partition,
				       size_t num_bytes,
				       void *buf,
				       size_t *out_num_read)
{
	struct vboot_avb_data *avbctx;
	VbExStream_t stream;
	uint64_t part_bytes, part_start_sector;
	uint64_t start_sector, sectors_to_read, pre_misalign;
	uint32_t sector_bytes;
	GptData *gpt;
	GptEntry *e;

	avbctx = (struct vboot_avb_data *)ops->user_data;
	gpt = avbctx->gpt;

	e = GptFindEntryByName(gpt, partition_name, NULL);
	if (e == NULL) {
		VB2_DEBUG("Unable to find %s partition\n", partition_name);
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
	}

	part_bytes = GptGetEntrySizeBytes(gpt, e);
	part_start_sector = e->starting_lba;
	sector_bytes = gpt->sector_bytes;

	if (num_bytes > part_bytes) {
		VB2_DEBUG("Trying to read %zu bytes from %s@%lld, but only %llu bytes long.\n",
			  num_bytes, partition_name, offset_from_partition, part_bytes);
		num_bytes = part_bytes;
	}

	if (part_start_sector * sector_bytes > (part_start_sector * sector_bytes) + part_bytes)
		return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;

	if (offset_from_partition < 0) {
		offset_from_partition += part_bytes;
		if (offset_from_partition < 0 || offset_from_partition > part_bytes)
			return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;
	}

	start_sector = (offset_from_partition / sector_bytes);
	pre_misalign = offset_from_partition % sector_bytes;

	VB2_ASSERT(pre_misalign == 0 && (num_bytes % sector_bytes) == 0);

	start_sector += part_start_sector;
	sectors_to_read = num_bytes / sector_bytes;

	if (VbExStreamOpen(avbctx->disk_handle, start_sector, sectors_to_read,
			   &stream)) {
		VB2_DEBUG("Unable to open disk handle.\n");
		return AVB_IO_RESULT_ERROR_IO;
	}

	if (VbExStreamRead(stream, sectors_to_read * sector_bytes, buf)) {
		VB2_DEBUG("Unable to read ramdisk partition\n");
		return AVB_IO_RESULT_ERROR_IO;
	}

	*out_num_read = num_bytes;

	VbExStreamClose(stream);

	return AVB_IO_RESULT_OK;
}


static AvbIOResult vboot_avb_get_size_of_partition(AvbOps *ops,
					 const char *partition_name,
					 uint64_t *out_size)
{
	struct vboot_avb_data *avbctx = (struct vboot_avb_data *)ops->user_data;
	GptEntry *e;

	e = GptFindEntryByName(avbctx->gpt, partition_name, NULL);
	if (e == NULL) {
		VB2_DEBUG("Unable to find %s partition\n", partition_name);
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
	}

	*out_size = GptGetEntrySizeBytes(avbctx->gpt, e);

	return AVB_IO_RESULT_OK;
}

static AvbIOResult vboot_avb_read_is_device_unlocked(AvbOps *ops, bool *out_is_unlocked)
{
	struct vboot_avb_data *avbctx = (struct vboot_avb_data *)ops->user_data;

	if (vb2_need_kernel_verification(avbctx->vb2_ctx))
		*out_is_unlocked = false;
	else if (avbctx->vb2_ctx->boot_mode == VB2_BOOT_MODE_DEVELOPER &&
	    vb2_secdata_fwmp_get_flag(avbctx->vb2_ctx, VB2_SECDATA_FWMP_DEV_USE_KEY_HASH))
		*out_is_unlocked = false;
	else
		*out_is_unlocked = true;

	VB2_DEBUG("%s\n", *out_is_unlocked ? "true" : "false");

	return AVB_IO_RESULT_OK;
}

static AvbIOResult vboot_avb_read_rollback_index(AvbOps *ops,
				       size_t rollback_index_slot,
				       uint64_t *out_rollback_index) {
	/*
	 * TODO(b/324230492): Implement rollback protection
	 * For now we always return 0 as the stored rollback index.
	 */
	VB2_DEBUG("TODO: implement read_rollback_index().\n");
	if (out_rollback_index != NULL)
		*out_rollback_index = 0;

	return AVB_IO_RESULT_OK;
}

static AvbIOResult get_unique_guid_for_partition(AvbOps *ops,
						 const char *partition,
						 char *guid_buf,
						 size_t guid_buf_size)
{
	struct vboot_avb_data *data;
	GptData *gpt;
	GptEntry *e;

	VB2_ASSERT((guid_buf_size >= GUID_STRLEN) && ops && ops->user_data);

	data = (struct vboot_avb_data *)ops->user_data;
	gpt = data->gpt;
	VB2_ASSERT(gpt);

	e = GptFindEntryByName(gpt, partition, NULL);
	if (e == NULL)
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;

	snprintf(guid_buf, guid_buf_size,
		 "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		 le32toh(e->unique.u.Uuid.time_low),
		 le16toh(e->unique.u.Uuid.time_mid),
		 le16toh(e->unique.u.Uuid.time_high_and_version),
		 e->unique.u.Uuid.clock_seq_high_and_reserved,
		 e->unique.u.Uuid.clock_seq_low,
		 e->unique.u.Uuid.node[0], e->unique.u.Uuid.node[1],
		 e->unique.u.Uuid.node[2], e->unique.u.Uuid.node[3],
		 e->unique.u.Uuid.node[4], e->unique.u.Uuid.node[5]);

	return AVB_IO_RESULT_OK;
}

static AvbIOResult validate_vbmeta_public_key(AvbOps *ops,
					      const uint8_t *public_key_data,
					      size_t public_key_length,
					      const uint8_t *public_key_metadata,
					      size_t public_key_metadata_length,
					      bool *out_key_is_trusted)
{
	struct vboot_avb_data *avbctx = (struct vboot_avb_data *)ops->user_data;
	struct vb2_shared_data *sd = vb2_get_sd(avbctx->vb2_ctx);
	struct vb2_public_key kernel_key;
	AvbRSAPublicKeyHeader h;
	uint8_t *key_data;
	uint32_t key_size;
	uint32_t arrsize;
	const uint32_t *avb_n, *avb_rr;
	vb2_error_t vb2_err;

	if (out_key_is_trusted == NULL)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

	*out_key_is_trusted = false;
	key_data = vb2_member_of(sd, sd->kernel_key_offset);
	key_size = sd->kernel_key_size;
	vb2_err = vb2_unpack_key_buffer(&kernel_key, key_data, key_size);
	if (vb2_err != VB2_SUCCESS) {
		VB2_DEBUG("Problem with unpacking key buffer: %d\n", vb2_err);
		goto out;
	}

	/*
	 * Convert key format stored in the vbmeta image - it has different
	 * endianness and size units compared to the kernel_subkey stored in
	 * flash
	 */
	if (!avb_rsa_public_key_header_validate_and_byteswap(
		(const AvbRSAPublicKeyHeader *)public_key_data, &h)) {
		VB2_DEBUG("Invalid vbmeta public key\n");
		goto out;
	}

	if (public_key_length < sizeof(AvbRSAPublicKeyHeader) + h.key_num_bits / 8 * 2) {
		VB2_DEBUG("Invalid vbmeta public key length\n");
		goto out;
	}

	arrsize = kernel_key.arrsize;
	if (arrsize != (h.key_num_bits / 32)) {
		VB2_DEBUG("Mismatch in key length!\n");
		goto out;
	}

	if (kernel_key.n0inv != h.n0inv) {
		VB2_DEBUG("Mismatch in n0inv value!\n");
		goto out;
	}

	avb_n = (uint32_t *)(public_key_data + sizeof(AvbRSAPublicKeyHeader));
	avb_rr = (uint32_t *)(public_key_data + sizeof(AvbRSAPublicKeyHeader)) + arrsize;
	for (int i = 0; i < arrsize; i++) {
		if (kernel_key.n[i] != be32toh(avb_n[arrsize - 1 - i])) {
			VB2_DEBUG("Mismatch in n key component!\n");
			goto out;
		}
		if (kernel_key.rr[i] != be32toh(avb_rr[arrsize - 1 - i])) {
			VB2_DEBUG("Mismatch in rr key component!\n");
			goto out;
		}
	}

	*out_key_is_trusted = true;
out:
	return AVB_IO_RESULT_OK;
}

/*
 * Initialize platform callbacks used within libavb.
 *
 * @param  vb2_ctx     Vboot context
 * @param  gpt         Pointer to gpt struct correlated with boot disk
 * @return pointer to AvbOps structure which should be used for invocation of
 *         libavb methods.
 */
AvbOps *vboot_avb_ops_new(struct vb2_context *vb2_ctx,
			  GptData *gpt)
{
	vboot_avb.gpt = gpt;
	vboot_avb.vb2_ctx = vb2_ctx;
	avb_ops.user_data = &vboot_avb;

	avb_ops.read_from_partition = vboot_avb_read_from_partition;
	avb_ops.get_size_of_partition = vboot_avb_get_size_of_partition;
	avb_ops.read_is_device_unlocked = vboot_avb_read_is_device_unlocked;
	avb_ops.read_rollback_index = vboot_avb_read_rollback_index;
	avb_ops.get_unique_guid_for_partition = get_unique_guid_for_partition;
	avb_ops.validate_vbmeta_public_key = validate_vbmeta_public_key;

	return &avb_ops;
}
