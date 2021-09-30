/* Copyright (c) 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef VBOOT_REFERENCE_TLCL_TPM2_STATIC_H_
#define VBOOT_REFERENCE_TLCL_TPM2_STATIC_H_

#include <stddef.h>
#include <stdint.h>

#include "tss_constants.h"
#include "tlcl_tpm2.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline uint32_t TlclLibInit(void) { return TlclTpm2LibInit(); }

static inline uint32_t TlclLibClose(void) { return TlclTpm2LibClose(); }

static inline uint32_t TlclSendReceive(const uint8_t *request,
				       uint8_t *response, int max_length)
{
	return TlclTpm2SendReceive(request, response, max_length);
}

static inline int TlclPacketSize(const uint8_t *packet)
{
	return TlclTpm2PacketSize(packet);
}

static inline uint32_t TlclStartup(void) { return TlclTpm2Startup(); }

static inline uint32_t TlclSaveState(void) { return TlclTpm2SaveState(); }

static inline uint32_t TlclResume(void) { return TlclTpm2Resume(); }

static inline uint32_t TlclSelfTestFull(void) { return TlclTpm2SelfTestFull(); }

static inline uint32_t TlclContinueSelfTest(void)
{
	return TlclTpm2ContinueSelfTest();
}

static inline uint32_t TlclDefineSpace(uint32_t index, uint32_t perm,
				       uint32_t size)
{
	return TlclTpm2DefineSpace(index, perm, size);
}

static inline uint32_t TlclDefineSpaceEx(const uint8_t *owner_auth,
					 uint32_t owner_auth_size,
					 uint32_t index, uint32_t perm,
					 uint32_t size, const void *auth_policy,
					 uint32_t auth_policy_size)
{
	return TlclTpm2DefineSpaceEx(owner_auth, owner_auth_size, index, perm,
				     size, auth_policy, auth_policy_size);
}

static inline uint32_t
TlclInitNvAuthPolicy(uint32_t pcr_selection_bitmap,
		     const uint8_t pcr_values[][TPM_PCR_DIGEST],
		     void *auth_policy, uint32_t *auth_policy_size)
{
	return TlclTpm2InitNvAuthPolicy(pcr_selection_bitmap, pcr_values,
					auth_policy, auth_policy_size);
}

static inline uint32_t TlclWrite(uint32_t index, const void *data,
				 uint32_t length)
{
	return TlclTpm2Write(index, data, length);
}

static inline uint32_t TlclRead(uint32_t index, void *data, uint32_t length)
{
	return TlclTpm2Read(index, data, length);
}

static inline uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length)
{
	return TlclTpm2PCRRead(index, data, length);
}

static inline uint32_t TlclWriteLock(uint32_t index)
{
	return TlclTpm2WriteLock(index);
}

static inline uint32_t TlclReadLock(uint32_t index)
{
	return TlclTpm2ReadLock(index);
}

static inline uint32_t TlclAssertPhysicalPresence(void)
{
	return TlclTpm2AssertPhysicalPresence();
}

static inline uint32_t TlclPhysicalPresenceCMDEnable(void)
{
	return TlclTpm2PhysicalPresenceCMDEnable();
}

static inline uint32_t TlclFinalizePhysicalPresence(void)
{
	return TlclTpm2FinalizePhysicalPresence();
}

static inline uint32_t TlclAssertPhysicalPresenceResult(void)
{
	return TlclTpm2AssertPhysicalPresenceResult();
}

static inline uint32_t TlclLockPhysicalPresence(void)
{
	return TlclTpm2LockPhysicalPresence();
}

static inline uint32_t TlclSetNvLocked(void) { return TlclTpm2SetNvLocked(); }

static inline int TlclIsOwned(void) { return TlclTpm2IsOwned(); }

static inline uint32_t TlclForceClear(void) { return TlclTpm2ForceClear(); }

static inline uint32_t TlclSetEnable(void) { return TlclTpm2SetEnable(); }

static inline uint32_t TlclClearEnable(void) { return TlclTpm2ClearEnable(); }

static inline uint32_t TlclSetDeactivated(uint8_t flag)
{
	return TlclTpm2SetDeactivated(flag);
}

static inline uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
				    uint8_t *nvlocked)
{
	return TlclTpm2GetFlags(disable, deactivated, nvlocked);
}

static inline uint32_t TlclSetGlobalLock(void)
{
	return TlclTpm2SetGlobalLock();
}

static inline uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest,
				  uint8_t *out_digest)
{
	return TlclTpm2Extend(pcr_num, in_digest, out_digest);
}

static inline uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions)
{
	return TlclTpm2GetPermissions(index, permissions);
}

static inline uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes,
					uint32_t *size, void *auth_policy,
					uint32_t *auth_policy_size)
{
	return TlclTpm2GetSpaceInfo(index, attributes, size, auth_policy,
				    auth_policy_size);
}

static inline uint32_t TlclGetPermanentFlags(TPM_PERMANENT_FLAGS *pflags)
{
	return TlclTpm2GetPermanentFlags(pflags);
}

static inline uint32_t TlclGetSTClearFlags(TPM_STCLEAR_FLAGS *pflags)
{
	return TlclTpm2GetSTClearFlags(pflags);
}

static inline uint32_t TlclGetOwnership(uint8_t *owned)
{
	return TlclTpm2GetOwnership(owned);
}

static inline uint32_t TlclGetRandom(uint8_t *data, uint32_t length,
				     uint32_t *size)
{
	return TlclTpm2GetRandom(data, length, size);
}

static inline uint32_t TlclGetVersion(uint32_t *vendor,
				      uint64_t *firmware_version,
				      uint8_t *vendor_specific_buf,
				      size_t *vendor_specific_buf_size)
{
	return TlclTpm2GetVersion(vendor, firmware_version, vendor_specific_buf,
				  vendor_specific_buf_size);
}

static inline uint32_t TlclIFXFieldUpgradeInfo(TPM_IFX_FIELDUPGRADEINFO *info)
{
	return TlclTpm2IFXFieldUpgradeInfo(info);
}

#ifdef CHROMEOS_ENVIRONMENT

static inline uint32_t TlclUndefineSpace(uint32_t index)
{
	return TlclTpm2UndefineSpace(index);
}

static inline uint32_t TlclUndefineSpaceEx(const uint8_t *owner_auth,
					   uint32_t owner_auth_size,
					   uint32_t index)
{
	return TlclTpm2UndefineSpaceEx(owner_auth, owner_auth_size, index);
}

#endif /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif /* VBOOT_REFERENCE_TLCL_TPM2_STATIC_H_ */
