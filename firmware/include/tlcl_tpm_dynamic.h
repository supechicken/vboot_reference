/* Copyright 2021 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM Lightweight Command Library.
 *
 * A low-level library for interfacing to TPM hardware or an emulator.
 */

#ifndef VBOOT_REFERENCE_TLCL_TPM_DYNAMIC_H_
#define VBOOT_REFERENCE_TLCL_TPM_DYNAMIC_H_

#include <stddef.h>
#include <stdint.h>

#include "tss_constants.h"
#include "tlcl_tpm1.h"
#include "tlcl_tpm2.h"

#if !TPM_DYNAMIC
_Static_assert(false, "TPM_DYNAMIC should be true");
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum TlclTpmVersion {
	TLCL_TPM_VERSION_UNKNOWN,
	TLCL_TPM_VERSION_1_2,
	TLCL_TPM_VERSION_2_0,
};

static inline enum TlclTpmVersion
TlclDynamicTpmVersion(enum TlclTpmVersion version)
{
	static enum TlclTpmVersion static_tpm_version =
		TLCL_TPM_VERSION_2_0; // Default version.
	if (version == TLCL_TPM_VERSION_UNKNOWN)
		return static_tpm_version;

	static_tpm_version = version;
	return version;
}

static inline uint32_t TlclLibInit(void)
{

	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1LibInit();
	return TlclTpm2LibInit();
}

static inline uint32_t TlclLibClose(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1LibClose();
	return TlclTpm2LibClose();
}

static inline uint32_t TlclSendReceive(const uint8_t *request,
				       uint8_t *response, int max_length)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SendReceive(request, response, max_length);
	return TlclTpm2SendReceive(request, response, max_length);
}

static inline int TlclPacketSize(const uint8_t *packet)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1PacketSize(packet);
	return TlclTpm2PacketSize(packet);
}

static inline uint32_t TlclStartup(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1Startup();
	return TlclTpm2Startup();
}

static inline uint32_t TlclSaveState(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SaveState();
	return TlclTpm2SaveState();
}

static inline uint32_t TlclResume(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1Resume();
	return TlclTpm2Resume();
}

static inline uint32_t TlclSelfTestFull(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SelfTestFull();
	return TlclTpm2SelfTestFull();
}

static inline uint32_t TlclContinueSelfTest(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1ContinueSelfTest();
	return TlclTpm2ContinueSelfTest();
}

static inline uint32_t TlclDefineSpace(uint32_t index, uint32_t perm,
				       uint32_t size)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1DefineSpace(index, perm, size);
	return TlclTpm2DefineSpace(index, perm, size);
}

static inline uint32_t TlclDefineSpaceEx(const uint8_t *owner_auth,
					 uint32_t owner_auth_size,
					 uint32_t index, uint32_t perm,
					 uint32_t size, const void *auth_policy,
					 uint32_t auth_policy_size)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1DefineSpaceEx(owner_auth, owner_auth_size, index,
					     perm, size, auth_policy,
					     auth_policy_size);
	return TlclTpm2DefineSpaceEx(owner_auth, owner_auth_size, index, perm,
				     size, auth_policy, auth_policy_size);
}

static inline uint32_t TlclWrite(uint32_t index, const void *data,
				 uint32_t length)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1Write(index, data, length);
	return TlclTpm2Write(index, data, length);
}

static inline uint32_t TlclRead(uint32_t index, void *data, uint32_t length)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1Read(index, data, length);
	return TlclTpm2Read(index, data, length);
}

static inline uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1PCRRead(index, data, length);
	return TlclTpm2PCRRead(index, data, length);
}

static inline uint32_t TlclWriteLock(uint32_t index)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1WriteLock(index);
	return TlclTpm2WriteLock(index);
}

static inline uint32_t TlclReadLock(uint32_t index)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1ReadLock(index);
	return TlclTpm2ReadLock(index);
}

static inline uint32_t TlclAssertPhysicalPresence(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1AssertPhysicalPresence();
	return TlclTpm2AssertPhysicalPresence();
}

static inline uint32_t TlclPhysicalPresenceCMDEnable(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1PhysicalPresenceCMDEnable();
	return TlclTpm2PhysicalPresenceCMDEnable();
}

static inline uint32_t TlclFinalizePhysicalPresence(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1FinalizePhysicalPresence();
	return TlclTpm2FinalizePhysicalPresence();
}

static inline uint32_t TlclAssertPhysicalPresenceResult(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1AssertPhysicalPresenceResult();
	return TlclTpm2AssertPhysicalPresenceResult();
}

static inline uint32_t TlclLockPhysicalPresence(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1LockPhysicalPresence();
	return TlclTpm2LockPhysicalPresence();
}

static inline uint32_t TlclSetNvLocked(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SetNvLocked();
	return TlclTpm2SetNvLocked();
}

static inline int TlclIsOwned(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1IsOwned();
	return TlclTpm2IsOwned();
}

static inline uint32_t TlclForceClear(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1ForceClear();
	return TlclTpm2ForceClear();
}

static inline uint32_t TlclSetEnable(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SetEnable();
	return TlclTpm2SetEnable();
}

static inline uint32_t TlclClearEnable(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1ClearEnable();
	return TlclTpm2ClearEnable();
}

static inline uint32_t TlclSetDeactivated(uint8_t flag)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SetDeactivated(flag);
	return TlclTpm2SetDeactivated(flag);
}

static inline uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
				    uint8_t *nvlocked)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetFlags(disable, deactivated, nvlocked);
	return TlclTpm2GetFlags(disable, deactivated, nvlocked);
}

static inline uint32_t TlclSetGlobalLock(void)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1SetGlobalLock();
	return TlclTpm2SetGlobalLock();
}

static inline uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest,
				  uint8_t *out_digest)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1Extend(pcr_num, in_digest, out_digest);
	return TlclTpm2Extend(pcr_num, in_digest, out_digest);
}

static inline uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetPermissions(index, permissions);
	return TlclTpm2GetPermissions(index, permissions);
}

static inline uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes,
					uint32_t *size, void *auth_policy,
					uint32_t *auth_policy_size)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetSpaceInfo(index, attributes, size,
					    auth_policy, auth_policy_size);
	return TlclTpm2GetSpaceInfo(index, attributes, size, auth_policy,
				    auth_policy_size);
}

static inline uint32_t TlclGetOwnership(uint8_t *owned)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetOwnership(owned);
	return TlclTpm2GetOwnership(owned);
}

static inline uint32_t TlclGetRandom(uint8_t *data, uint32_t length,
				     uint32_t *size)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetRandom(data, length, size);
	return TlclTpm2GetRandom(data, length, size);
}

static inline uint32_t TlclGetVersion(uint32_t *vendor,
				      uint64_t *firmware_version,
				      uint8_t *vendor_specific_buf,
				      size_t *vendor_specific_buf_size)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1GetVersion(vendor, firmware_version,
					  vendor_specific_buf,
					  vendor_specific_buf_size);
	return TlclTpm2GetVersion(vendor, firmware_version, vendor_specific_buf,
				  vendor_specific_buf_size);
}

#ifdef CHROMEOS_ENVIRONMENT

static inline uint32_t TlclUndefineSpace(uint32_t index)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1UndefineSpace(index);
	return TlclTpm2UndefineSpace(index);
}

static inline uint32_t TlclUndefineSpaceEx(const uint8_t *owner_auth,
					   uint32_t owner_auth_size,
					   uint32_t index)
{
	if (TlclDynamicTpmVersion(TLCL_TPM_VERSION_UNKNOWN) ==
	    TLCL_TPM_VERSION_1_2)
		return TlclTpm1UndefineSpaceEx(owner_auth, owner_auth_size,
					       index);
	return TlclTpm2UndefineSpaceEx(owner_auth, owner_auth_size, index);
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

#endif /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif /* VBOOT_REFERENCE_TLCL_TPM_DYNAMIC_H_ */
