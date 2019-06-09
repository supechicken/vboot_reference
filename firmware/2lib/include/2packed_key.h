/*
 * Helper functions to get data pointed to by a public key or signature.
 */
const uint8_t *vb2_packed_key_data(const struct vb2_packed_key *key);

/**
 * Verify a packed key is fully contained in its parent data
 *
 * @param parent	Parent data
 * @param parent_size	Parent size in bytes
 * @param key		Packed key pointer
 * @return VB2_SUCCESS, or non-zero if error.
 */
int vb2_verify_packed_key_inside(const void *parent,
				 uint32_t parent_size,
				 const struct vb2_packed_key *key);

/**
 * Unpack a vboot1-format key buffer for use in verification
 *
 * The elements of the unpacked key will point into the source buffer, so don't
 * free the source buffer until you're done with the key.
 *
 * @param key		Destintion for unpacked key
 * @param buf		Source buffer containing packed key
 * @param size		Size of buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_unpack_key_buffer(struct vb2_public_key *key,
			  const uint8_t *buf,
			  uint32_t size);

/**
 * Unpack a vboot1-format key for use in verification
 *
 * The elements of the unpacked key will point into the source packed key, so
 * don't free the source until you're done with the public key.
 *
 * @param key		Destintion for unpacked key
 * @param packed_key	Source packed key
 * @param size		Size of buffer in bytes
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_unpack_key(struct vb2_public_key *key,
		   const struct vb2_packed_key *packed_key);
