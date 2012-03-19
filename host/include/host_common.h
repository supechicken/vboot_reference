/* Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Host-side functions for verified boot.
 */

#ifndef VBOOT_REFERENCE_HOST_COMMON_H_
#define VBOOT_REFERENCE_HOST_COMMON_H_

/* Host is allowed direct use of stdlib funcs such as malloc() and free(),
 * since it's using the stub implementation from firmware/lib/stub. */
#define _STUB_IMPLEMENTATION_

#include "cryptolib.h"
#include "host_key.h"
#include "host_keyblock.h"
#include "host_misc.h"
#include "host_signature.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_struct.h"

/* The current version is 3. Specifying 0 here will create new headers using v3
 * (unless overridden) but will verify or repack either v2 or v3. Systems
 * shipped with RO firmware that uses v2 will require v2 headers forever. */
#ifndef DEFAULT_PREAMBLE_HEADER_VERSION
#define DEFAULT_PREAMBLE_HEADER_VERSION 0
#endif


typedef union {
  VbMinimalPreambleHeader     m;
  VbFirmwarePreambleHeader2_1 v2;
  VbFirmwarePreambleHeader    v3;
} VbFirmwarePreambleUnion;

typedef union {
  VbMinimalPreambleHeader     m;
  VbKernelPreambleHeader2_0   v2;
  VbKernelPreambleHeader      v3;
} VbKernelPreambleUnion;


/* Return the header_version_major field, or 0 on error.
 * Works for both Firware and Kernel preamble headers.
 * If you already know it's good, just pass 0 for the size arg */
int GetPreambleHeaderFormat(uint8_t* ptr, uint64_t size); // HEY
/* Quick preamble header version tests */
#define P_VERSION(P) (((VbMinimalPreambleHeader *)(P))->header_version_major)
#define IS_V3(P) (P_VERSION(P) == 3)

/* Generic access to shared firmware preamble members */
#define FP_MEMBER(P,M) (\
  { VbFirmwarePreambleUnion *___p = (VbFirmwarePreambleUnion *)(P); \
    IS_V3(___p) ? ___p->v3.M : ___p->v2.M; })
/* Conditional access to renamed kernel preamble members */
#define FP_MEMBER_V3_V2(P,M3,M2) (\
  { VbFirmwarePreambleUnion *___p = (VbFirmwarePreambleUnion *)(P); \
    IS_V3(___p) ? ___p->v3.M3 : ___p->v2.M2; })

/* Generic access to shared kernel preamble members */
#define KP_MEMBER(P,M) (\
  { VbKernelPreambleUnion *___p = (VbKernelPreambleUnion *)(P); \
    IS_V3(___p) ? ___p->v3.M : ___p->v2.M; })
/* Conditional access to renamed kernel preamble members */
#define KP_MEMBER_V3_V2(P,M3,M2) (\
  { VbKernelPreambleUnion *___p = (VbKernelPreambleUnion *)(P); \
    IS_V3(___p) ? ___p->v3.M3 : ___p->v2.M2; })


/* Creates a firmware preamble, signed with [signing_key].
 * Caller owns the returned pointer, and must free it with Free().
 *
 * Returns NULL if error. */
VbFirmwarePreambleHeader* CreateFirmwarePreamble(
  uint64_t firmware_version,
  const VbPublicKey* kernel_subkey,
  const VbSignature* body_signature,
  const VbPrivateKey* signing_key,
  uint32_t flags,
  const char* name);


/* Creates a kernel preamble, signed with [signing_key].
 * Caller owns the returned pointer, and must free it with Free().
 *
 * Returns NULL if error. */
VbKernelPreambleHeader* CreateKernelPreamble(
  uint64_t kernel_version,
  uint64_t body_load_address,
  uint64_t bootloader_address,
  uint64_t bootloader_size,
  const VbSignature* body_signature,
  uint64_t desired_size,
  const VbPrivateKey* signing_key);


/* For backwards compatibility, we may need to create or verify blobs that use
 * the older version 2.x preamble headers. */

VbFirmwarePreambleHeader2_1* CreateFirmwarePreamble2_1(
  uint64_t firmware_version,
  const VbPublicKey* kernel_subkey,
  const VbSignature* body_signature,
  const VbPrivateKey* signing_key,
  uint32_t flags);

VbKernelPreambleHeader2_0* CreateKernelPreamble2_0(
  uint64_t kernel_version,
  uint64_t body_load_address,
  uint64_t bootloader_address,
  uint64_t bootloader_size,
  const VbSignature* body_signature,
  uint64_t desired_size,
  const VbPrivateKey* signing_key);

int VerifyFirmwarePreamble2_x(const void* ptr, uint64_t size,
                              const RSAPublicKey* key);


int VerifyKernelPreamble2_x(const void* ptr, uint64_t size,
                            const RSAPublicKey* key);

#endif  /* VBOOT_REFERENCE_HOST_COMMON_H_ */
