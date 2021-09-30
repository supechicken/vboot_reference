/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef VBOOT_REFERENCE_TLCL_TPM2_H_
#define VBOOT_REFERENCE_TLCL_TPM2_H_

#include <stddef.h>
#include <stdint.h>

#include "tss_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t TlclTpm2LibInit(void);

uint32_t TlclTpm2LibClose(void);

uint32_t TlclTpm2SendReceive(const uint8_t *request, uint8_t *response,
			     int max_length);

int TlclTpm2PacketSize(const uint8_t *packet);

uint32_t TlclTpm2Startup(void);

uint32_t TlclTpm2SaveState(void);

uint32_t TlclTpm2Resume(void);

uint32_t TlclTpm2SelfTestFull(void);

uint32_t TlclTpm2ContinueSelfTest(void);

uint32_t TlclTpm2DefineSpace(uint32_t index, uint32_t perm, uint32_t size);

uint32_t TlclTpm2DefineSpaceEx(const uint8_t *owner_auth,
			       uint32_t owner_auth_size, uint32_t index,
			       uint32_t perm, uint32_t size,
			       const void *auth_policy,
			       uint32_t auth_policy_size);

uint32_t TlclTpm2InitNvAuthPolicy(uint32_t pcr_selection_bitmap,
				  const uint8_t pcr_values[][TPM_PCR_DIGEST],
				  void *auth_policy,
				  uint32_t *auth_policy_size);

uint32_t TlclTpm2Write(uint32_t index, const void *data, uint32_t length);

uint32_t TlclTpm2Read(uint32_t index, void *data, uint32_t length);

uint32_t TlclTpm2PCRRead(uint32_t index, void *data, uint32_t length);

uint32_t TlclTpm2WriteLock(uint32_t index);

uint32_t TlclTpm2ReadLock(uint32_t index);

uint32_t TlclTpm2AssertPhysicalPresence(void);

uint32_t TlclTpm2PhysicalPresenceCMDEnable(void);

uint32_t TlclTpm2FinalizePhysicalPresence(void);

uint32_t TlclTpm2AssertPhysicalPresenceResult(void);

uint32_t TlclTpm2LockPhysicalPresence(void);

uint32_t TlclTpm2SetNvLocked(void);

int TlclTpm2IsOwned(void);

uint32_t TlclTpm2ForceClear(void);

uint32_t TlclTpm2SetEnable(void);

uint32_t TlclTpm2ClearEnable(void);

uint32_t TlclTpm2SetDeactivated(uint8_t flag);

uint32_t TlclTpm2GetFlags(uint8_t *disable, uint8_t *deactivated,
			  uint8_t *nvlocked);

uint32_t TlclTpm2SetGlobalLock(void);

uint32_t TlclTpm2Extend(int pcr_num, const uint8_t *in_digest,
			uint8_t *out_digest);

uint32_t TlclTpm2GetPermissions(uint32_t index, uint32_t *permissions);

uint32_t TlclTpm2GetSpaceInfo(uint32_t index, uint32_t *attributes,
			      uint32_t *size, void *auth_policy,
			      uint32_t *auth_policy_size);

uint32_t TlclTpm2GetPermanentFlags(TPM_PERMANENT_FLAGS *pflags);

uint32_t TlclTpm2GetSTClearFlags(TPM_STCLEAR_FLAGS *pflags);

uint32_t TlclTpm2GetOwnership(uint8_t *owned);

uint32_t TlclTpm2GetRandom(uint8_t *data, uint32_t length, uint32_t *size);

uint32_t TlclTpm2GetVersion(uint32_t *vendor, uint64_t *firmware_version,
			    uint8_t *vendor_specific_buf,
			    size_t *vendor_specific_buf_size);

uint32_t TlclTpm2IFXFieldUpgradeInfo(TPM_IFX_FIELDUPGRADEINFO *info);

#ifdef CHROMEOS_ENVIRONMENT

uint32_t TlclTpm2UndefineSpace(uint32_t index);

uint32_t TlclTpm2UndefineSpaceEx(const uint8_t *owner_auth,
				 uint32_t owner_auth_size, uint32_t index);

#endif /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif /* VBOOT_REFERENCE_TLCL_TPM2_H_ */
