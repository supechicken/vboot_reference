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

void vboot_avb_ops_free(AvbOps *ops)
{
	if (ops == NULL)
		return;

	avb_free(ops->user_data);
	avb_free(ops);
}

static AvbIOResult vboot_avb_read_from_partition(AvbOps *ops,
				       const char *partition_name,
				       int64_t offset_from_partition,
				       size_t num_bytes,
				       void *buf,
				       size_t *out_num_read)
{
	struct vboot_avb_data *avbctx;
	VbExStream_t stream;
	uint64_t part_size, part_start;
	uint64_t start_sector, sectors_to_read, pre_misalign;
	uint32_t sector_bytes;
	uint8_t *tmp_buf;
	GptData *gpt;
	GptEntry *e;

	avbctx = (struct vboot_avb_data *)ops->user_data;
	gpt = avbctx->gpt;

	e = GptFindEntryByName(gpt, partition_name, NULL);
	if (e == NULL) {
		VB2_DEBUG("Unable to find %s partition\n", partition_name);
		return AVB_IO_RESULT_ERROR_NO_SUCH_PARTITION;
	}

	part_size = GptGetEntrySizeBytes(gpt, e);
	part_start = e->starting_lba;
	sector_bytes = gpt->sector_bytes;

	if (num_bytes > part_size) {
		VB2_DEBUG("Trying to read %zu bytes from %s@%lld, but only %llu bytes long.\n",
			  num_bytes, partition_name, offset_from_partition, part_size);
		return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;
	}

	if (part_start * sector_bytes > (part_start * sector_bytes) + part_size)
		return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;

	if (offset_from_partition < 0) {
		offset_from_partition += part_size;
		if (offset_from_partition < 0 || offset_from_partition > part_size)
			return AVB_IO_RESULT_ERROR_RANGE_OUTSIDE_PARTITION;
	}

	start_sector = (offset_from_partition / sector_bytes);
	pre_misalign = offset_from_partition % sector_bytes;

	start_sector += part_start;
	sectors_to_read = (pre_misalign + num_bytes) / sector_bytes;

	if ((pre_misalign + num_bytes) % sector_bytes)
		sectors_to_read += 1;

	if (VbExStreamOpen(avbctx->disk_handle, start_sector, sectors_to_read,
			   &stream)) {
		VB2_DEBUG("Unable to open disk handle.\n");
		return AVB_IO_RESULT_ERROR_IO;
	}

	if (pre_misalign != 0 || (num_bytes % sector_bytes)) {
		tmp_buf = malloc(sectors_to_read * sector_bytes);
		if (tmp_buf == NULL) {
			VB2_DEBUG("Cannot allocate buffer for unaligned read\n");
			return AVB_IO_RESULT_ERROR_OOM;
		}
	} else {
		tmp_buf = buf;
	}

	if (VbExStreamRead(stream, sectors_to_read * sector_bytes, tmp_buf)) {
		VB2_DEBUG("Unable to read ramdisk partition\n");
		return AVB_IO_RESULT_ERROR_IO;
	}

	/*
	 * TODO(b/331881159): "Add support for non-sector size reads in
	 * depthcharge block driver
	 */
	if (pre_misalign != 0 || (num_bytes % sector_bytes)) {
		memcpy(buf, tmp_buf + pre_misalign, num_bytes);
		free(tmp_buf);
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


	if (need_kernel_verification(avbctx->vb2_ctx))
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
	int ret;

	if (guid_buf_size < GUID_STRLEN || !ops || !ops->user_data)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;

	data = (struct vboot_avb_data *)ops->user_data;
	gpt = data->gpt;
	if (!gpt)
		return AVB_IO_RESULT_ERROR_NO_SUCH_VALUE;


	e = GptFindEntryByName(gpt, partition, NULL);
	if (e == NULL)
		return AVB_IO_RESULT_ERROR_IO;

	ret = snprintf(guid_buf, guid_buf_size,
		       "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
		       le32toh(e->unique.u.Uuid.time_low),
		       le16toh(e->unique.u.Uuid.time_mid),
		       le16toh(e->unique.u.Uuid.time_high_and_version),
		       e->unique.u.Uuid.clock_seq_high_and_reserved,
		       e->unique.u.Uuid.clock_seq_low,
		       e->unique.u.Uuid.node[0], e->unique.u.Uuid.node[1],
		       e->unique.u.Uuid.node[2], e->unique.u.Uuid.node[3],
		       e->unique.u.Uuid.node[4], e->unique.u.Uuid.node[5]);

	if (ret != (GUID_STRLEN - 1))
		return AVB_IO_RESULT_ERROR_IO;

	return AVB_IO_RESULT_OK;
}

/*
 * Initialize platform callbacks used within libavb.
 *
 * @param  vb2_ctx     Vboot context
 * @param  gpt         Pointer to gpt struct correlated with boot disk
 * @return pointer to AvbOps structure which should be used for invocation of
 *         libavb methods. This should be freed using vboot_avb_ops_free().
 *         NULL in case of error.
 */
AvbOps *vboot_avb_ops_new(struct vb2_context *vb2_ctx,
			  GptData *gpt)
{
	struct vboot_avb_data *data;
	AvbOps *ops;

	ops = malloc(sizeof(AvbOps));
	if (ops == NULL)
		return NULL;

	data = malloc(sizeof(struct vboot_avb_data));
	if (data == NULL) {
		avb_free(ops);
		return NULL;
	}

	ops->user_data = data;

	data->gpt = gpt;
	data->vb2_ctx = vb2_ctx;

	ops->read_from_partition = vboot_avb_read_from_partition;
	ops->get_size_of_partition = vboot_avb_get_size_of_partition;
	ops->read_is_device_unlocked = vboot_avb_read_is_device_unlocked;
	ops->read_rollback_index = vboot_avb_read_rollback_index;
	ops->get_unique_guid_for_partition = get_unique_guid_for_partition;

	return ops;
}
