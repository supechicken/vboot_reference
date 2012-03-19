/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host functions for verified boot.
 */

/* TODO: change all 'return 0', 'return 1' into meaningful return codes */

#include <string.h>
#include "host_common.h"

#include "cryptolib.h"
#include "utility.h"
#include "vboot_common.h"


int GetPreambleHeaderFormat(uint8_t* ptr, uint64_t size) {
  if (size && EXPECTED_VBPREAMBLEHEADER_MINIMUM_SIZE > size)
    return 0;
  return ((VbMinimalPreambleHeader *)ptr)->header_version_major;
}


VbFirmwarePreambleHeader* CreateFirmwarePreamble(
    uint64_t firmware_version,
    const VbPublicKey* kernel_subkey,
    const VbSignature* body_digest,
    const VbPrivateKey* signing_key,
    uint32_t flags,
    const char* name) {

  VbFirmwarePreambleHeader* h;
  uint64_t signed_size = (sizeof(VbFirmwarePreambleHeader) +
                          kernel_subkey->key_size +
                          body_digest->sig_size);
  uint64_t block_size = signed_size + siglen_map[signing_key->algorithm];
  uint8_t* kernel_subkey_dest;
  uint8_t* body_digest_dest;
  uint8_t* block_sig_dest;
  VbSignature *sigtmp;

  /* Allocate key block */
  h = (VbFirmwarePreambleHeader*)malloc(block_size);
  if (!h)
    return NULL;
  Memset(h, 0, block_size);
  kernel_subkey_dest = (uint8_t*)(h + 1);
  body_digest_dest = kernel_subkey_dest + kernel_subkey->key_size;
  block_sig_dest = body_digest_dest + body_digest->sig_size;

  h->header_version_major = FIRMWARE_PREAMBLE_HEADER_VERSION_MAJOR;
  h->header_version_minor = FIRMWARE_PREAMBLE_HEADER_VERSION_MINOR;
  h->preamble_size = block_size;
  h->firmware_version = firmware_version;
  h->flags = flags;
  if (name)
    strncpy(h->name, name, sizeof(h->name));

  /* Copy data key */
  PublicKeyInit(&h->kernel_subkey, kernel_subkey_dest,
                kernel_subkey->key_size);
  PublicKeyCopy(&h->kernel_subkey, kernel_subkey);

  /* Copy body hash */
  SignatureInit(&h->body_digest, body_digest_dest,
                body_digest->sig_size, 0);
  SignatureCopy(&h->body_digest, body_digest);

  /* Set up signature struct so we can calculate the signature */
  SignatureInit(&h->preamble_signature, block_sig_dest,
                siglen_map[signing_key->algorithm], signed_size);

  /* Calculate signature */
  sigtmp = CalculateSignature((uint8_t*)h, signed_size, signing_key);
  SignatureCopy(&h->preamble_signature, sigtmp);
  free(sigtmp);

  /* Return the header */
  return h;
}


/* Creates a kernel preamble, signed with [signing_key].
 * Caller owns the returned pointer, and must free it with free().
 *
 * Returns NULL if error. */
VbKernelPreambleHeader* CreateKernelPreamble(
    uint64_t kernel_version,
    uint64_t body_load_address,
    uint64_t bootloader_address,
    uint64_t bootloader_size,
    const VbSignature* body_digest,
    uint64_t desired_size,
    const VbPrivateKey* signing_key) {

  VbKernelPreambleHeader* h;
  uint64_t signed_size = (sizeof(VbKernelPreambleHeader) +
                          body_digest->sig_size);
  uint64_t block_size = signed_size + siglen_map[signing_key->algorithm];
  uint8_t* body_digest_dest;
  uint8_t* block_sig_dest;
  VbSignature *sigtmp;

  /* If the block size is smaller than the desired size, pad it */
  if (block_size < desired_size)
    block_size = desired_size;

  /* Allocate key block */
  h = (VbKernelPreambleHeader*)malloc(block_size);
  Memset(h, 0, block_size);

  if (!h)
    return NULL;
  body_digest_dest = (uint8_t*)(h + 1);
  block_sig_dest = body_digest_dest + body_digest->sig_size;

  h->header_version_major = KERNEL_PREAMBLE_HEADER_VERSION_MAJOR;
  h->header_version_minor = KERNEL_PREAMBLE_HEADER_VERSION_MINOR;
  h->preamble_size = block_size;
  h->kernel_version = kernel_version;
  h->body_load_address = body_load_address;
  h->bootloader_address = bootloader_address;
  h->bootloader_size = bootloader_size;

  /* Copy body signature */
  SignatureInit(&h->body_digest, body_digest_dest,
                body_digest->sig_size, 0);
  SignatureCopy(&h->body_digest, body_digest);

  /* Set up signature struct so we can calculate the signature */
  SignatureInit(&h->preamble_signature, block_sig_dest,
                siglen_map[signing_key->algorithm], signed_size);

  /* Calculate signature */
  sigtmp = CalculateSignature((uint8_t*)h, signed_size, signing_key);
  SignatureCopy(&h->preamble_signature, sigtmp);
  free(sigtmp);

  /* Return the header */
  return h;
}


VbFirmwarePreambleHeader2_1* CreateFirmwarePreamble2_1(
    uint64_t firmware_version,
    const VbPublicKey* kernel_subkey,
    const VbSignature* body_signature,
    const VbPrivateKey* signing_key,
    uint32_t flags) {

  VbFirmwarePreambleHeader2_1* h;
  uint64_t signed_size = (sizeof(VbFirmwarePreambleHeader2_1) +
                          kernel_subkey->key_size +
                          body_signature->sig_size);
  uint64_t block_size = signed_size + siglen_map[signing_key->algorithm];
  uint8_t* kernel_subkey_dest;
  uint8_t* body_sig_dest;
  uint8_t* block_sig_dest;
  VbSignature *sigtmp;

  /* Allocate key block */
  h = (VbFirmwarePreambleHeader2_1*)malloc(block_size);
  if (!h)
    return NULL;
  Memset(h, 0, block_size);
  kernel_subkey_dest = (uint8_t*)(h + 1);
  body_sig_dest = kernel_subkey_dest + kernel_subkey->key_size;
  block_sig_dest = body_sig_dest + body_signature->sig_size;

  h->header_version_major = 2;
  h->header_version_minor = 1;
  h->preamble_size = block_size;
  h->firmware_version = firmware_version;
  h->flags = flags;

  /* Copy data key */
  PublicKeyInit(&h->kernel_subkey, kernel_subkey_dest,
                kernel_subkey->key_size);
  PublicKeyCopy(&h->kernel_subkey, kernel_subkey);

  /* Copy body signature */
  SignatureInit(&h->body_signature, body_sig_dest,
                body_signature->sig_size, 0);
  SignatureCopy(&h->body_signature, body_signature);

  /* Set up signature struct so we can calculate the signature */
  SignatureInit(&h->preamble_signature, block_sig_dest,
                siglen_map[signing_key->algorithm], signed_size);

  /* Calculate signature */
  sigtmp = CalculateSignature((uint8_t*)h, signed_size, signing_key);
  SignatureCopy(&h->preamble_signature, sigtmp);
  free(sigtmp);

  /* Return the header */
  return h;
}


VbKernelPreambleHeader2_0* CreateKernelPreamble2_0(
    uint64_t kernel_version,
    uint64_t body_load_address,
    uint64_t bootloader_address,
    uint64_t bootloader_size,
    const VbSignature* body_signature,
    uint64_t desired_size,
    const VbPrivateKey* signing_key) {

  VbKernelPreambleHeader2_0* h;
  uint64_t signed_size = (sizeof(VbKernelPreambleHeader2_0) +
                          body_signature->sig_size);
  uint64_t block_size = signed_size + siglen_map[signing_key->algorithm];
  uint8_t* body_sig_dest;
  uint8_t* block_sig_dest;
  VbSignature *sigtmp;

  /* If the block size is smaller than the desired size, pad it */
  if (block_size < desired_size)
    block_size = desired_size;

  /* Allocate key block */
  h = (VbKernelPreambleHeader2_0*)malloc(block_size);
  Memset(h, 0, block_size);

  if (!h)
    return NULL;
  body_sig_dest = (uint8_t*)(h + 1);
  block_sig_dest = body_sig_dest + body_signature->sig_size;

  h->header_version_major = 2;
  h->header_version_minor = 0;
  h->preamble_size = block_size;
  h->kernel_version = kernel_version;
  h->body_load_address = body_load_address;
  h->bootloader_address = bootloader_address;
  h->bootloader_size = bootloader_size;

  /* Copy body signature */
  SignatureInit(&h->body_signature, body_sig_dest,
                body_signature->sig_size, 0);
  SignatureCopy(&h->body_signature, body_signature);

  /* Set up signature struct so we can calculate the signature */
  SignatureInit(&h->preamble_signature, block_sig_dest,
                siglen_map[signing_key->algorithm], signed_size);

  /* Calculate signature */
  sigtmp = CalculateSignature((uint8_t*)h, signed_size, signing_key);
  SignatureCopy(&h->preamble_signature, sigtmp);
  free(sigtmp);

  /* Return the header */
  return h;
}


int VerifyFirmwarePreamble2_x(const void* ptr, uint64_t size,
                              const RSAPublicKey* key) {

  const VbFirmwarePreambleHeader2_1* preamble = ptr;
  const VbSignature* sig = &preamble->preamble_signature;

  /* Sanity checks before attempting signature of data */
  if(size < EXPECTED_VBFIRMWAREPREAMBLEHEADER2_0_SIZE) {
    VBDEBUG(("Not enough data for preamble header 2.0.\n"));
    return VBOOT_PREAMBLE_INVALID;
  }
  if (preamble->header_version_major != 2) {
    VBDEBUG(("Incompatible firmware preamble header version.\n"));
    return VBOOT_PREAMBLE_INVALID;
  }
  if (size < preamble->preamble_size) {
    VBDEBUG(("Not enough data for preamble.\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Check signature */
  if (VerifySignatureInside(preamble, preamble->preamble_size, sig)) {
    VBDEBUG(("Preamble signature off end of preamble\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Make sure advertised signature data sizes are sane. */
  if (preamble->preamble_size < sig->data_size) {
    VBDEBUG(("Signature calculated past end of the block\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  if (VerifyData((const uint8_t*)preamble, size, sig, key)) {
    VBDEBUG(("Preamble signature validation failed\n"));
    return VBOOT_PREAMBLE_SIGNATURE;
  }

  /* Verify we signed enough data */
  if (sig->data_size < sizeof(VbFirmwarePreambleHeader)) {
    VBDEBUG(("Didn't sign enough data\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Verify body signature is inside the signed data */
  if (VerifySignatureInside(preamble, sig->data_size,
                            &preamble->body_signature)) {
    VBDEBUG(("Firmware body signature off end of preamble\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Verify kernel subkey is inside the signed data */
  if (VerifyPublicKeyInside(preamble, sig->data_size,
                            &preamble->kernel_subkey)) {
    VBDEBUG(("Kernel subkey off end of preamble\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* If the preamble header version is at least 2.1, verify we have
   * space for the added fields from 2.1. */
  if (preamble->header_version_minor >= 1) {
    if(size < EXPECTED_VBFIRMWAREPREAMBLEHEADER2_1_SIZE) {
      VBDEBUG(("Not enough data for preamble header 2.1.\n"));
      return VBOOT_PREAMBLE_INVALID;
    }
  }

  /* Success */
  return VBOOT_SUCCESS;
}

int VerifyKernelPreamble2_x(const void *ptr, uint64_t size,
                            const RSAPublicKey* key) {
  const VbKernelPreambleHeader2_0* preamble = ptr;
  const VbSignature* sig = &preamble->preamble_signature;

  /* Sanity checks before attempting signature of data */
  if(size < sizeof(VbKernelPreambleHeader)) {
    VBDEBUG(("Not enough data for preamble header.\n"));
    return VBOOT_PREAMBLE_INVALID;
  }
  if (preamble->header_version_major != 2) {
    VBDEBUG(("Incompatible kernel preamble header (v%d, not v%d).\n",
             preamble->header_version_major, 2));
    return VBOOT_PREAMBLE_INVALID;
  }
  if (size < preamble->preamble_size) {
    VBDEBUG(("Not enough data for preamble.\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Check signature */
  if (VerifySignatureInside(preamble, preamble->preamble_size, sig)) {
    VBDEBUG(("Preamble signature off end of preamble\n"));
    return VBOOT_PREAMBLE_INVALID;
  }
  if (VerifyData((const uint8_t*)preamble, size, sig, key)) {
    VBDEBUG(("Preamble signature validation failed\n"));
    return VBOOT_PREAMBLE_SIGNATURE;
  }

  /* Verify we signed enough data */
  if (sig->data_size < sizeof(VbKernelPreambleHeader)) {
    VBDEBUG(("Didn't sign enough data\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Verify body signature is inside the signed data */
  if (VerifySignatureInside(preamble, sig->data_size,
                            &preamble->body_signature)) {
    VBDEBUG(("Kernel body signature off end of preamble\n"));
    return VBOOT_PREAMBLE_INVALID;
  }

  /* Success */
  return VBOOT_SUCCESS;
}
