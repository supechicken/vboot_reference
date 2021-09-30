/* Copyright (c) 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef VBOOT_REFERENCE_TLCL_TPM1_STATIC_H_
#define VBOOT_REFERENCE_TLCL_TPM1_STATIC_H_

#include <stddef.h>
#include <stdint.h>

#include "tss_constants.h"
#include "tlcl_tpm1.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t TlclLibInit(void) { return TlclTpm1LibInit(); }

static inline uint32_t TlclLibClose(void) { return TlclTpm1LibClose(); }

static inline uint32_t TlclSendReceive(const uint8_t *request,
				       uint8_t *response, int max_length)
{
	return TlclTpm1SendReceive(request, response, max_length);
}

static inline int TlclPacketSize(const uint8_t *packet)
{
	return TlclTpm1PacketSize(packet);
}

static inline uint32_t TlclStartup(void) { return TlclTpm1Startup(); }

static inline uint32_t TlclSaveState(void) { return TlclTpm1SaveState(); }

static inline uint32_t TlclResume(void) { return TlclTpm1Resume(); }

static inline uint32_t TlclSelfTestFull(void) { return TlclTpm1SelfTestFull(); }

static inline uint32_t TlclContinueSelfTest(void)
{
	return TlclTpm1ContinueSelfTest();
}

static inline uint32_t TlclDefineSpace(uint32_t index, uint32_t perm,
				       uint32_t size)
{
	return TlclTpm1DefineSpace(index, perm, size);
}

static inline uint32_t TlclDefineSpaceEx(const uint8_t *owner_auth,
					 uint32_t owner_auth_size,
					 uint32_t index, uint32_t perm,
					 uint32_t size, const void *auth_policy,
					 uint32_t auth_policy_size)
{
	return TlclTpm1DefineSpaceEx(owner_auth, owner_auth_size, index, perm,
				     size, auth_policy, auth_policy_size);
}

static inline uint32_t
TlclInitNvAuthPolicy(uint32_t pcr_selection_bitmap,
		     const uint8_t pcr_values[][TPM_PCR_DIGEST],
		     void *auth_policy, uint32_t *auth_policy_size)
{
	return TlclTpm1InitNvAuthPolicy(pcr_selection_bitmap, pcr_values,
					auth_policy, auth_policy_size);
}

static inline uint32_t TlclWrite(uint32_t index, const void *data,
				 uint32_t length)
{
	return TlclTpm1Write(index, data, length);
}

static inline uint32_t TlclRead(uint32_t index, void *data, uint32_t length)
{
	return TlclTpm1Read(index, data, length);
}

static inline uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length)
{
	return TlclTpm1PCRRead(index, data, length);
}

static inline uint32_t TlclWriteLock(uint32_t index)
{
	return TlclTpm1WriteLock(index);
}

static inline uint32_t TlclReadLock(uint32_t index)
{
	return TlclTpm1ReadLock(index);
}

static inline uint32_t TlclAssertPhysicalPresence(void)
{
	return TlclTpm1AssertPhysicalPresence();
}

static inline uint32_t TlclPhysicalPresenceCMDEnable(void)
{
	return TlclTpm1PhysicalPresenceCMDEnable();
}

static inline uint32_t TlclFinalizePhysicalPresence(void)
{
	return TlclTpm1FinalizePhysicalPresence();
}

static inline uint32_t TlclAssertPhysicalPresenceResult(void)
{
	return TlclTpm1AssertPhysicalPresenceResult();
}

static inline uint32_t TlclLockPhysicalPresence(void)
{
	return TlclTpm1LockPhysicalPresence();
}

static inline uint32_t TlclSetNvLocked(void) { return TlclTpm1SetNvLocked(); }

static inline int TlclIsOwned(void) { return TlclTpm1IsOwned(); }

static inline uint32_t TlclForceClear(void) { return TlclTpm1ForceClear(); }

static inline uint32_t TlclSetEnable(void) { return TlclTpm1SetEnable(); }

static inline uint32_t TlclClearEnable(void) { return TlclTpm1ClearEnable(); }

static inline uint32_t TlclSetDeactivated(uint8_t flag)
{
	return TlclTpm1SetDeactivated(flag);
}

static inline uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
				    uint8_t *nvlocked)
{
	return TlclTpm1GetFlags(disable, deactivated, nvlocked);
}

static inline uint32_t TlclSetGlobalLock(void)
{
	return TlclTpm1SetGlobalLock();
}

static inline uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest,
				  uint8_t *out_digest)
{
	return TlclTpm1Extend(pcr_num, in_digest, out_digest);
}

static inline uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions)
{
	return TlclTpm1GetPermissions(index, permissions);
}

static inline uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes,
					uint32_t *size, void *auth_policy,
					uint32_t *auth_policy_size)
{
	return TlclTpm1GetSpaceInfo(index, attributes, size, auth_policy,
				    auth_policy_size);
}

static inline uint32_t TlclGetPermanentFlags(TPM_PERMANENT_FLAGS *pflags)
{
	return TlclTpm1GetPermanentFlags(pflags);
}

static inline uint32_t TlclGetSTClearFlags(TPM_STCLEAR_FLAGS *pflags)
{
	return TlclTpm1GetSTClearFlags(pflags);
}

static inline uint32_t TlclGetOwnership(uint8_t *owned)
{
	return TlclTpm1GetOwnership(owned);
}

static inline uint32_t TlclGetRandom(uint8_t *data, uint32_t length,
				     uint32_t *size)
{
	return TlclTpm1GetRandom(data, length, size);
}

static inline uint32_t TlclGetVersion(uint32_t *vendor,
				      uint64_t *firmware_version,
				      uint8_t *vendor_specific_buf,
				      size_t *vendor_specific_buf_size)
{
	return TlclTpm1GetVersion(vendor, firmware_version, vendor_specific_buf,
				  vendor_specific_buf_size);
}

static inline uint32_t TlclIFXFieldUpgradeInfo(TPM_IFX_FIELDUPGRADEINFO *info)
{
	return TlclTpm1IFXFieldUpgradeInfo(info);
}

#ifdef CHROMEOS_ENVIRONMENT

static inline uint32_t TlclUndefineSpace(uint32_t index)
{
	return TlclTpm1UndefineSpace(index);
}

static inline uint32_t TlclUndefineSpaceEx(const uint8_t *owner_auth,
					   uint32_t owner_auth_size,
					   uint32_t index)
{
	return TlclTpm1UndefineSpaceEx(owner_auth, owner_auth_size, index);
}

static inline uint32_t TlclReadPubek(uint32_t *public_exponent,
				     uint8_t *modulus, uint32_t *modulus_size)
{
	return TlclTpm1ReadPubek(public_exponent, modulus, modulus_size);
}

static inline uint32_t
TlclTakeOwnership(uint8_t enc_owner_auth[TPM_RSA_2048_LEN],
		  uint8_t enc_srk_auth[TPM_RSA_2048_LEN],
		  uint8_t owner_auth[TPM_AUTH_DATA_LEN])
{
	return TlclTpm1TakeOwnership(enc_owner_auth, enc_srk_auth, owner_auth);
}

static inline uint32_t TlclCreateDelegationFamily(uint8_t family_label)
{
	return TlclTpm1CreateDelegationFamily(family_label);
}

static inline uint32_t
TlclReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY *table,
			      uint32_t *table_size)
{
	return TlclTpm1ReadDelegationFamilyTable(table, table_size);
}

#endif /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif /* VBOOT_REFERENCE_TLCL_TPM1_STATIC_H_ */
