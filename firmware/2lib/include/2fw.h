
/**
 * Decide which firmware slot to try this boot.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_select_fw_slot(struct vb2_context *ctx);
