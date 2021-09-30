/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef VBOOT_REFERENCE_TLCL_TPM1_H_
#define VBOOT_REFERENCE_TLCL_TPM1_H_

#include <stddef.h>
#include <stdint.h>

#include "tpm1_tss_constants.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t TlclTpm1LibInit(void);

uint32_t TlclTpm1LibClose(void);

uint32_t TlclTpm1SendReceive(const uint8_t *request, uint8_t *response,
			     int max_length);

int TlclTpm1PacketSize(const uint8_t *packet);

uint32_t TlclTpm1Startup(void);

uint32_t TlclTpm1SaveState(void);

uint32_t TlclTpm1Resume(void);

uint32_t TlclTpm1SelfTestFull(void);

uint32_t TlclTpm1ContinueSelfTest(void);

uint32_t TlclTpm1DefineSpace(uint32_t index, uint32_t perm, uint32_t size);

uint32_t TlclTpm1DefineSpaceEx(const uint8_t *owner_auth,
			       uint32_t owner_auth_size, uint32_t index,
			       uint32_t perm, uint32_t size,
			       const void *auth_policy,
			       uint32_t auth_policy_size);

uint32_t TlclTpm1InitNvAuthPolicy(uint32_t pcr_selection_bitmap,
				  const uint8_t pcr_values[][TPM1_PCR_DIGEST],
				  void *auth_policy,
				  uint32_t *auth_policy_size);

uint32_t TlclTpm1Write(uint32_t index, const void *data, uint32_t length);

uint32_t TlclTpm1Read(uint32_t index, void *data, uint32_t length);

uint32_t TlclTpm1PCRRead(uint32_t index, void *data, uint32_t length);

uint32_t TlclTpm1WriteLock(uint32_t index);

uint32_t TlclTpm1ReadLock(uint32_t index);

uint32_t TlclTpm1AssertPhysicalPresence(void);

uint32_t TlclTpm1PhysicalPresenceCMDEnable(void);

uint32_t TlclTpm1FinalizePhysicalPresence(void);

uint32_t TlclTpm1AssertPhysicalPresenceResult(void);

uint32_t TlclTpm1LockPhysicalPresence(void);

uint32_t TlclTpm1SetNvLocked(void);

int TlclTpm1IsOwned(void);

uint32_t TlclTpm1ForceClear(void);

uint32_t TlclTpm1SetEnable(void);

uint32_t TlclTpm1ClearEnable(void);

uint32_t TlclTpm1SetDeactivated(uint8_t flag);

uint32_t TlclTpm1GetFlags(uint8_t *disable, uint8_t *deactivated,
			  uint8_t *nvlocked);

uint32_t TlclTpm1SetGlobalLock(void);

uint32_t TlclTpm1Extend(int pcr_num, const uint8_t *in_digest,
			uint8_t *out_digest);

uint32_t TlclTpm1GetPermissions(uint32_t index, uint32_t *permissions);

uint32_t TlclTpm1GetSpaceInfo(uint32_t index, uint32_t *attributes,
			      uint32_t *size, void *auth_policy,
			      uint32_t *auth_policy_size);

uint32_t TlclTpm1GetPermanentFlags(TPM1_PERMANENT_FLAGS *pflags);

uint32_t TlclTpm1GetSTClearFlags(TPM1_STCLEAR_FLAGS *pflags);

uint32_t TlclTpm1GetOwnership(uint8_t *owned);

uint32_t TlclTpm1GetRandom(uint8_t *data, uint32_t length, uint32_t *size);

uint32_t TlclTpm1GetVersion(uint32_t *vendor, uint64_t *firmware_version,
			    uint8_t *vendor_specific_buf,
			    size_t *vendor_specific_buf_size);

uint32_t TlclTpm1IFXFieldUpgradeInfo(TPM1_IFX_FIELDUPGRADEINFO *info);

#ifdef CHROMEOS_ENVIRONMENT

uint32_t TlclTpm1UndefineSpace(uint32_t index);

uint32_t TlclTpm1UndefineSpaceEx(const uint8_t *owner_auth,
				 uint32_t owner_auth_size, uint32_t index);

uint32_t TlclTpm1ReadPubek(uint32_t *public_exponent, uint8_t *modulus,
			   uint32_t *modulus_size);

uint32_t TlclTpm1TakeOwnership(uint8_t enc_owner_auth[TPM_RSA_2048_LEN],
			       uint8_t enc_srk_auth[TPM_RSA_2048_LEN],
			       uint8_t owner_auth[TPM_AUTH_DATA_LEN]);

uint32_t TlclTpm1CreateDelegationFamily(uint8_t family_label);

uint32_t TlclTpm1ReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY *table,
					   uint32_t *table_size);

#endif /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif /* VBOOT_REFERENCE_TLCL_TPM1_H_ */
