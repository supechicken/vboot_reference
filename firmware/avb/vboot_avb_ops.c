/* Copyright 2025 The ChromiumOS Authors
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
	struct vb2_context *vb2_ctx;
};

static AvbOps avb_ops;
static struct vboot_avb_data vboot_avb;

static AvbIOResult read_is_device_unlocked(AvbOps *ops, bool *out_is_unlocked)
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

static AvbIOResult read_rollback_index(AvbOps *ops,
				       size_t rollback_index_slot,
				       uint64_t *out_rollback_index)
{
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
	struct vboot_avb_data *avbctx;
	GptData *gpt;
	GptEntry *e;

	VB2_ASSERT((guid_buf_size >= GUID_STRLEN) && ops && ops->user_data);

	avbctx = (struct vboot_avb_data *)ops->user_data;
	gpt = avbctx->gpt;
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

/*
 * Initialize platform callbacks used within libavb.
 *
 * @param  vb2_ctx     Vboot context
 * @return pointer to AvbOps structure which should be used for invocation of
 *         libavb methods.
 */
AvbOps *vboot_avb_ops_new(struct vb2_context *vb2_ctx)
{
	vboot_avb.vb2_ctx = vb2_ctx;
	avb_ops.user_data = &vboot_avb;

	avb_ops.read_is_device_unlocked = read_is_device_unlocked;
	avb_ops.read_rollback_index = read_rollback_index;
	avb_ops.get_unique_guid_for_partition = get_unique_guid_for_partition;

	return &avb_ops;
}
