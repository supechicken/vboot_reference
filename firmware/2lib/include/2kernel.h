/**
 * Verify the kernel keyblock using the previously-loaded kernel key.
 *
 * After this call, the data key is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_load_kernel_keyblock(struct vb2_context *ctx);

/**
 * Verify the kernel preamble using the data subkey from the keyblock.
 *
 * After this call, the preamble is stored in the work buffer.
 *
 * @param ctx		Vboot context
 * @return VB2_SUCCESS, or error code on error.
 */
int vb2_load_kernel_preamble(struct vb2_context *ctx);

/**
 * Check the sanity of a kernel preamble using a public key.
 *
 * The signature in the preamble is destroyed during the check.
 *
 * @param preamble     	Preamble to verify
 * @param size		Size of preamble buffer
 * @param key		Key to use to verify preamble
 * @param wb		Work buffer
 * @return VB2_SUCCESS, or non-zero error code if error.
 */
int vb2_verify_kernel_preamble(struct vb2_kernel_preamble *preamble,
			       uint32_t size,
			       const struct vb2_public_key *key,
			       const struct vb2_workbuf *wb);

/**
 * Retrieve the 16-bit vmlinuz header address and size from the preamble.
 *
 * Size 0 means there is no 16-bit vmlinuz header present.  Old preamble
 * versions (<2.1) return 0 for both fields.
 *
 * @param preamble	Preamble to check
 * @param vmlinuz_header_address	Destination for header address
 * @param vmlinuz_header_size		Destination for header size
 */
void vb2_kernel_get_vmlinuz_header(const struct vb2_kernel_preamble *preamble,
				   uint64_t *vmlinuz_header_address,
				   uint32_t *vmlinuz_header_size);

/**
 * Get the flags for the kernel preamble.
 *
 * @param preamble	Preamble to check
 * @return Flags for the preamble.  Old preamble versions (<2.2) return 0.
 */
uint32_t vb2_kernel_get_flags(const struct vb2_kernel_preamble *preamble);
