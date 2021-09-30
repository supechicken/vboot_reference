/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
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

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
/* Functions implemented in tlcl.c */

enum TlclTpmVersion{
  TLCL_TPM_VERSION_UNKNOWN,
  TLCL_TPM_VERSION_1_2,
  TLCL_TPM_VERSION_2_0,
};

static inline void TlclDynamicTpmVersion(int version = TLCL_TPM_VERSION_UNKNOWN)
{
	static enum TlclTpmVersion static_tpm_version = TLCL_TPM_VERSION_2_0; // Default version.
	if (version == TLCL_TPM_VERSION_UNKNOWN){
		return static_tpm_version;
	}else {
		static_tpm_version = version;
		return version;
	}
}

/**
 * Call this first.  Returns 0 if success, nonzero if error.
 */
static inline uint32_t TlclLibInit(void)
{

if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1LibInit();
}
return TlclTpm2LibInit();

}

/**
 * Call this on shutdown.  Returns 0 if success, nonzero if error.
 */
static inline uint32_t TlclLibClose(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1LibClose();
}
return TlclTpm2LibClose();
}

/* Low-level operations */

/**
 * Perform a raw TPM request/response transaction.
 */
static inline uint32_t TlclSendReceive(const uint8_t *request, uint8_t *response,
			 int max_length)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SendReceive(request, response, max_length);
}
return TlclTpm2SendReceive(request, response, max_length);
}

/**
 * Return the size of a TPM request or response packet.
 */
static inline int TlclPacketSize(const uint8_t *packet)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1PacketSize(packet);
}
return TlclTpm2PacketSize(packet);
}

/* Commands */

/**
 * Send a TPM_Startup(ST_CLEAR).  The TPM error code is returned (0 for
 * success).
 */
static inline uint32_t TlclStartup(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1Startup();
}
return TlclTpm2Startup();
}

/**
 * Save the TPM state.  Normally done by the kernel before a suspend, included
 * here for tests.  The TPM error code is returned (0 for success).
 */
static inline uint32_t TlclSaveState(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SaveState();
}
return TlclTpm2SaveState();
}

/**
 * Resume by sending a TPM_Startup(ST_STATE).  The TPM error code is returned
 * (0 for success).
 */
static inline uint32_t TlclResume(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1Resume();
}
return TlclTpm2Resume();
}

/**
 * Run the self test.
 *
 * Note---this is synchronous.  To run this in parallel with other firmware,
 * use ContinueSelfTest().  The TPM error code is returned.
 */
static inline uint32_t TlclSelfTestFull(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SelfTestFull();
}
return TlclTpm2SelfTestFull();
}

/**
 * Run the self test in the background.
 */
static inline uint32_t TlclContinueSelfTest(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ContinueSelfTest();
}
return TlclTpm2ContinueSelfTest();
}

/**
 * Define a space with permission [perm].  [index] is the index for the space,
 * [size] the usable data size.  The TPM error code is returned.
 */
static inline uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1DefineSpace(index, perm, size);
}
return TlclTpm2DefineSpace(index, perm, size);
}

/**
 * Define a space using owner authorization secret [owner_auth]. The space is
 * set up to have permission [perm].  [index] is the index for the space, [size]
 * the usable data size. Optional auth policy (such as PCR selections) can be
 * passed via [auth_policy]. The TPM error code is returned.
 */
static inline uint32_t TlclDefineSpaceEx(const uint8_t* owner_auth, uint32_t owner_auth_size,
			   uint32_t index, uint32_t perm, uint32_t size,
			   const void* auth_policy, uint32_t auth_policy_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1DefineSpaceEx(owner_auth, owner_auth_size, index, perm, size, auth_policy, auth_policy_size);
}
return TlclTpm2DefineSpaceEx(owner_auth, owner_auth_size, index, perm, size, auth_policy, auth_policy_size);
}

/**
 * Initializes [auth_policy] to require PCR binding of the given
 * [pcr_selection_bitmap]. The PCR values are passed in the [pcr_values]
 * parameter with each entry corresponding to the sequence of indexes that
 * corresponds to the bits that are set in [pcr_selection_bitmap]. Returns
 * TPM_SUCCESS if successful, TPM_E_BUFFER_SIZE if the provided buffer is too
 * short. The actual size of the policy will be set in [auth_policy_size] upon
 * return, also for the case of insufficient buffer size.
 */
static inline uint32_t TlclInitNvAuthPolicy(uint32_t pcr_selection_bitmap,
			      const uint8_t pcr_values[][TPM_PCR_DIGEST],
			      void* auth_policy, uint32_t* auth_policy_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1InitNvAuthPolicy(pcr_selection_bitmap, pcr_values, auth_policy, auth_policy_size);
}
return TlclTpm2InitNvAuthPolicy(pcr_selection_bitmap, pcr_values, auth_policy, auth_policy_size);
}

/**
 * Write [length] bytes of [data] to space at [index].  The TPM error code is
 * returned.
 */
static inline uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1Write(index, data, length);
}
return TlclTpm2Write(index, data, length);
}

/**
 * Read [length] bytes from space at [index] into [data].  The TPM error code
 * is returned.
 */
static inline uint32_t TlclRead(uint32_t index, void *data, uint32_t length)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1Read(index, data, length);
}
return TlclTpm2Read(index, data, length);
}

/**
 * Read PCR at [index] into [data].  [length] must be TPM_PCR_DIGEST or
 * larger. The TPM error code is returned.
 */
static inline uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1PCRRead(index, data, length);
}
return TlclTpm2PCRRead(index, data, length);
}

/**
 * Write-lock space at [index].  The TPM error code is returned.
 */
static inline uint32_t TlclWriteLock(uint32_t index)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1WriteLock(index);
}
return TlclTpm2WriteLock(index);
}

/**
 * Read-lock space at [index].  The TPM error code is returned.
 */
static inline uint32_t TlclReadLock(uint32_t index)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ReadLock(index);
}
return TlclTpm2ReadLock(index);
}

/**
 * Assert physical presence in software.  The TPM error code is returned.
 */
static inline uint32_t TlclAssertPhysicalPresence(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1AssertPhysicalPresence();
}
return TlclTpm2AssertPhysicalPresence();
}

/**
 * Enable the physical presence command.  The TPM error code is returned.
 */
static inline uint32_t TlclPhysicalPresenceCMDEnable(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1PhysicalPresenceCMDEnable();
}
return TlclTpm2PhysicalPresenceCMDEnable();
}

/**
 * Finalize the physical presence settings: sofware PP is enabled, hardware PP
 * is disabled, and the lifetime lock is set.  The TPM error code is returned.
 */
static inline uint32_t TlclFinalizePhysicalPresence(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1FinalizePhysicalPresence();
}
return TlclTpm2FinalizePhysicalPresence();
}

static inline uint32_t TlclAssertPhysicalPresenceResult(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1AssertPhysicalPresenceResult();
}
return TlclTpm2AssertPhysicalPresenceResult();
}

/**
 * Turn off physical presence and locks it off until next reboot.  The TPM
 * error code is returned.
 */
static inline uint32_t TlclLockPhysicalPresence(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1LockPhysicalPresence();
}
return TlclTpm2LockPhysicalPresence();
}

/**
 * Set the nvLocked bit.  The TPM error code is returned.
 */
static inline uint32_t TlclSetNvLocked(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SetNvLocked();
}
return TlclTpm2SetNvLocked();
}

/**
 * Return 1 if the TPM is owned, 0 otherwise.
 */
static inline int TlclIsOwned(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1IsOwned();
}
return TlclTpm2IsOwned();
}

/**
 * Issue a ForceClear.  The TPM error code is returned.
 */
static inline uint32_t TlclForceClear(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ForceClear();
}
return TlclTpm2ForceClear();
}

/**
 * Issue a PhysicalEnable.  The TPM error code is returned.
 */
static inline uint32_t TlclSetEnable(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SetEnable();
}
return TlclTpm2SetEnable();
}

/**
 * Issue a PhysicalDisable.  The TPM error code is returned.
 */
static inline uint32_t TlclClearEnable(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ClearEnable();
}
return TlclTpm2ClearEnable();
}

/**
 * Issue a SetDeactivated.  Pass 0 to activate.  Returns result code.
 */
static inline uint32_t TlclSetDeactivated(uint8_t flag)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SetDeactivated(flag);
}
return TlclTpm2SetDeactivated(flag);
}

/**
 * Get flags of interest.  Pointers for flags you aren't interested in may
 * be NULL.  The TPM error code is returned.
 */
static inline uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
		      uint8_t *nvlocked)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetFlags(disable, deactivated, nvlocked);
}
return TlclTpm2GetFlags(disable, deactivated, nvlocked);
}

/**
 * Set the bGlobalLock flag, which only a reboot can clear.  The TPM error
 * code is returned.
 */
static inline uint32_t TlclSetGlobalLock(void)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1SetGlobalLock();
}
return TlclTpm2SetGlobalLock();
}

/**
 * Perform a TPM_Extend.
 */
static inline uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest, uint8_t *out_digest)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1Extend(pcr_num, in_digest, out_digest);
}
return TlclTpm2Extend(pcr_num, in_digest, out_digest);
}

/**
 * Get the permission bits for the NVRAM space with |index|.
 */
static inline uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetPermissions(index, permissions);
}
return TlclTpm2GetPermissions(index, permissions);
}

/**
 * Get the public information about the NVRAM space identified by |index|. All
 * other parameters are filled in with the respective information.
 * |auth_policy_size| is both an input an output parameter. It should contain
 * the available buffer size in |auth_policy| and will be updated to indicate
 * the size of the filled in auth policy upon return. If the buffer size is not
 * sufficient, the return value will be TPM_E_BUFFER_SIZE.
 */
static inline uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes, uint32_t *size,
			  void* auth_policy, uint32_t* auth_policy_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetSpaceInfo(index, attributes, size, auth_policy, auth_policy_size);
}
return TlclTpm2GetSpaceInfo(index, attributes, size, auth_policy, auth_policy_size);
}

/**
 * Get the entire set of permanent flags.
 */
static inline uint32_t TlclGetPermanentFlags(TPM_PERMANENT_FLAGS *pflags)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetPermanentFlags(pflags);
}
return TlclTpm2GetPermanentFlags(pflags);
}

/**
 * Get the entire set of volatile (ST_CLEAR) flags.
 */
static inline uint32_t TlclGetSTClearFlags(TPM_STCLEAR_FLAGS *pflags)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetSTClearFlags(pflags);
}
return TlclTpm2GetSTClearFlags(pflags);
}

/**
 * Get the ownership flag. The TPM error code is returned.
 */
static inline uint32_t TlclGetOwnership(uint8_t *owned)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetOwnership(owned);
}
return TlclTpm2GetOwnership(owned);
}

/**
 * Request [length] bytes from TPM RNG to be stored in [data]. Actual number of
 * bytes read is stored in [size]. The TPM error code is returned.
 */
static inline uint32_t TlclGetRandom(uint8_t *data, uint32_t length, uint32_t *size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetRandom(data, length, size);
}
return TlclTpm2GetRandom(data, length, size);
}

/**
 * Requests version information from the TPM.
 * If vendor_specific_buf_size != NULL, requests also the vendor-specific
 * variable-length part of the version:
 *   if vendor_specific_buf == NULL, determines its size and returns in
 *       *vendor_specific_buf_size;
 *   if vendor_specific_buf != NULL, fills the buffer until either the
 *       end of the vendor specific data or the end of the buffer, sets
 *       *vendor_specific_buf_size to the length of the filled data.
 */
static inline uint32_t TlclGetVersion(uint32_t* vendor, uint64_t* firmware_version,
			uint8_t* vendor_specific_buf,
			size_t* vendor_specific_buf_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1GetVersion(vendor, firmware_version, vendor_specific_buf, vendor_specific_buf_size);
}
return TlclTpm2GetVersion(vendor, firmware_version, vendor_specific_buf, vendor_specific_buf_size);
}

/**
 * Issues the IFX specific FieldUpgradeInfoRequest2 TPM_FieldUpgrade subcommand
 * and fills in [info] with results.
 */
static inline uint32_t TlclIFXFieldUpgradeInfo(TPM_IFX_FIELDUPGRADEINFO *info)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1IFXFieldUpgradeInfo(info);
}
return TlclTpm2IFXFieldUpgradeInfo(info);
}

#ifdef CHROMEOS_ENVIRONMENT

/**
 * Undefine the space. [index] is the index for the space. The TPM error code
 * is returned.
 */
static inline uint32_t TlclUndefineSpace(uint32_t index)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1UndefineSpace(index);
}
return TlclTpm2UndefineSpace(index);
}

/**
 * Undefine a space. For TPM 2.0, it will use platform authrorization when the
 * space is created by TPMA_NV_PLATFORMCREATE flag, or use owner authorization
 * secret [owner_auth] otherwise. For TPM 1.2, only avaible when physical
 * presence is set or TPM_PERMANENT_FLAGS->nvLocked is not set.
 * [index] is the index for the space
 * The TPM error code is returned.
 */
static inline uint32_t TlclUndefineSpaceEx(const uint8_t* owner_auth,
			     uint32_t owner_auth_size,
			     uint32_t index)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1UndefineSpaceEx(owner_auth, owner_auth_size, index);
}
return TlclTpm2UndefineSpaceEx(owner_auth, owner_auth_size, index);
}

#ifndef TPM2_MODE

/**
 * Read the public half of the EK.
 */
static inline uint32_t TlclReadPubek(uint32_t* public_exponent,
		       uint8_t* modulus,
		       uint32_t* modulus_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ReadPubek(public_exponent, modulus, modulus_size);
}
return TlclTpm2ReadPubek(public_exponent, modulus, modulus_size);
}

/**
 * Takes ownership of the TPM. [enc_owner_auth] and [enc_srk_auth] are the owner
 * and SRK authorization secrets encrypted under the endorsement key. The clear
 * text [owner_auth] needs to be passed as well for command auth.
 */
static inline uint32_t TlclTakeOwnership(uint8_t enc_owner_auth[TPM_RSA_2048_LEN],
			   uint8_t enc_srk_auth[TPM_RSA_2048_LEN],
			   uint8_t owner_auth[TPM_AUTH_DATA_LEN])
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1TakeOwnership(enc_owner_auth, enc_srk_auth, owner_auth);
}
return TlclTpm2TakeOwnership(enc_owner_auth, enc_srk_auth, owner_auth);
}

/**
 * Create a delegation family with the specified [family_label].
 */
static inline uint32_t TlclCreateDelegationFamily(uint8_t family_label)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1CreateDelegationFamily(family_label);
}
return TlclTpm2CreateDelegationFamily(family_label);
}

/**
 * Read the delegation family table. Entries are stored in [table]. The size of
 * the family table array must be specified in [table_size]. [table_size] gets
 * updated to indicate actual number of table entries available.
 */
static inline uint32_t TlclReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY *table,
				       uint32_t* table_size)
{
if(TlclDynamicTpmVersion()==TLCL_TPM_VERSION_1_2){
return TlclTpm1ReadDelegationFamilyTable(table, table_size);
}
return TlclTpm2ReadDelegationFamilyTable(table, table_size);
}

#endif  /* TPM2_MODE */
#endif  /* CHROMEOS_ENVIRONMENT */

#ifdef __cplusplus
}
#endif

#endif  /* VBOOT_REFERENCE_TLCL_TPM_DYNAMIC_H_ */
