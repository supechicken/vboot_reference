/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef TPM_LITE_TLCL_VERSIONED_H_
#define TPM_LITE_TLCL_VERSIONED_H_
uint32_t TlclLibInit(void);
uint32_t Tlcl1LibInit(void);
uint32_t Tlcl2LibInit(void);

uint32_t TlclLibClose(void);
uint32_t Tlcl1LibClose(void);
uint32_t Tlcl2LibClose(void);

uint32_t TlclSendReceive(const uint8_t *request, uint8_t *response,
			 int max_length);
uint32_t Tlcl1SendReceive(const uint8_t *request, uint8_t *response,
			 int max_length);
uint32_t Tlcl2SendReceive(const uint8_t *request, uint8_t *response,
			 int max_length);

int TlclPacketSize(const uint8_t *packet);
int Tlcl1PacketSize(const uint8_t *packet);
int Tlcl2PacketSize(const uint8_t *packet);

uint32_t TlclStartup(void);
uint32_t Tlcl1Startup(void);
uint32_t Tlcl2Startup(void);

uint32_t TlclSaveState(void);
uint32_t Tlcl1SaveState(void);
uint32_t Tlcl2SaveState(void);

uint32_t TlclResume(void);
uint32_t Tlcl1Resume(void);
uint32_t Tlcl2Resume(void);

uint32_t TlclSelfTestFull(void);
uint32_t Tlcl1SelfTestFull(void);
uint32_t Tlcl2SelfTestFull(void);

uint32_t TlclContinueSelfTest(void);
uint32_t Tlcl1ContinueSelfTest(void);
uint32_t Tlcl2ContinueSelfTest(void);

uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size);
uint32_t Tlcl1DefineSpace(uint32_t index, uint32_t perm, uint32_t size);
uint32_t Tlcl2DefineSpace(uint32_t index, uint32_t perm, uint32_t size);

uint32_t TlclDefineSpaceEx(const uint8_t* owner_auth, uint32_t owner_auth_size,
			   uint32_t index, uint32_t perm, uint32_t size,
			   const void* auth_policy, uint32_t auth_policy_size);
uint32_t Tlcl1DefineSpaceEx(const uint8_t* owner_auth, uint32_t owner_auth_size,
			   uint32_t index, uint32_t perm, uint32_t size,
			   const void* auth_policy, uint32_t auth_policy_size);
uint32_t Tlcl2DefineSpaceEx(const uint8_t* owner_auth, uint32_t owner_auth_size,
			   uint32_t index, uint32_t perm, uint32_t size,
			   const void* auth_policy, uint32_t auth_policy_size);

uint32_t Tlcl1InitNvAuthPolicy(uint32_t pcr_selection_bitmap,
			      const uint8_t pcr_values[][TPM1_PCR_DIGEST],
			      void* auth_policy, uint32_t* auth_policy_size);
uint32_t Tlcl2InitNvAuthPolicy(uint32_t pcr_selection_bitmap,
			      const uint8_t pcr_values[][TPM2_PCR_DIGEST],
			      void* auth_policy, uint32_t* auth_policy_size);

uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length);
uint32_t Tlcl1Write(uint32_t index, const void *data, uint32_t length);
uint32_t Tlcl2Write(uint32_t index, const void *data, uint32_t length);

uint32_t TlclRead(uint32_t index, void *data, uint32_t length);
uint32_t Tlcl1Read(uint32_t index, void *data, uint32_t length);
uint32_t Tlcl2Read(uint32_t index, void *data, uint32_t length);

uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length);
uint32_t Tlcl1PCRRead(uint32_t index, void *data, uint32_t length);
uint32_t Tlcl2PCRRead(uint32_t index, void *data, uint32_t length);

uint32_t TlclWriteLock(uint32_t index);
uint32_t Tlcl1WriteLock(uint32_t index);
uint32_t Tlcl2WriteLock(uint32_t index);

uint32_t TlclReadLock(uint32_t index);
uint32_t Tlcl1ReadLock(uint32_t index);
uint32_t Tlcl2ReadLock(uint32_t index);

uint32_t Tlcl1AssertPhysicalPresence(void);

uint32_t Tlcl1PhysicalPresenceCMDEnable(void);

uint32_t Tlcl1FinalizePhysicalPresence(void);

uint32_t Tlcl1AssertPhysicalPresenceResult(void);

uint32_t TlclLockPhysicalPresence(void);
uint32_t Tlcl1LockPhysicalPresence(void);
uint32_t Tlcl2LockPhysicalPresence(void);

uint32_t Tlcl1SetNvLocked(void);

int TlclIsOwned(void);
int Tlcl1IsOwned(void);
int Tlcl2IsOwned(void);

uint32_t TlclForceClear(void);
uint32_t Tlcl1ForceClear(void);
uint32_t Tlcl2ForceClear(void);

uint32_t TlclSetEnable(void);
uint32_t Tlcl1SetEnable(void);
uint32_t Tlcl2SetEnable(void);

uint32_t Tlcl1ClearEnable(void);

uint32_t TlclSetDeactivated(uint8_t flag);
uint32_t Tlcl1SetDeactivated(uint8_t flag);
uint32_t Tlcl2SetDeactivated(uint8_t flag);

uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
		      uint8_t *nvlocked);
uint32_t Tlcl1GetFlags(uint8_t *disable, uint8_t *deactivated,
		      uint8_t *nvlocked);
uint32_t Tlcl2GetFlags(uint8_t *disable, uint8_t *deactivated,
		      uint8_t *nvlocked);

uint32_t TlclSetGlobalLock(void);
uint32_t Tlcl1SetGlobalLock(void);
uint32_t Tlcl2SetGlobalLock(void);

uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest,
                    uint8_t *out_digest);
uint32_t Tlcl1Extend(int pcr_num, const uint8_t *in_digest,
                     uint8_t *out_digest);
uint32_t Tlcl2Extend(int pcr_num, const uint8_t *in_digest,
                     uint8_t *out_digest);

uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions);
uint32_t Tlcl1GetPermissions(uint32_t index, uint32_t *permissions);
uint32_t Tlcl2GetPermissions(uint32_t index, uint32_t *permissions);

uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes, uint32_t *size,
			  void* auth_policy, uint32_t* auth_policy_size);
uint32_t Tlcl1GetSpaceInfo(uint32_t index, uint32_t *attributes, uint32_t *size,
			  void* auth_policy, uint32_t* auth_policy_size);
uint32_t Tlcl2GetSpaceInfo(uint32_t index, uint32_t *attributes, uint32_t *size,
			  void* auth_policy, uint32_t* auth_policy_size);

uint32_t Tlcl1GetPermanentFlags(TPM1_PERMANENT_FLAGS *pflags);
uint32_t Tlcl2GetPermanentFlags(TPM2_PERMANENT_FLAGS *pflags);

uint32_t Tlcl1GetSTClearFlags(TPM1_STCLEAR_FLAGS *pflags);
uint32_t Tlcl2GetSTClearFlags(TPM2_STCLEAR_FLAGS *pflags);

uint32_t TlclGetOwnership(uint8_t *owned);
uint32_t Tlcl1GetOwnership(uint8_t *owned);
uint32_t Tlcl2GetOwnership(uint8_t *owned);

uint32_t TlclGetRandom(uint8_t *data, uint32_t length, uint32_t *size);
uint32_t Tlcl1GetRandom(uint8_t *data, uint32_t length, uint32_t *size);
uint32_t Tlcl2GetRandom(uint8_t *data, uint32_t length, uint32_t *size);

uint32_t TlclGetVersion(uint32_t* vendor, uint64_t* firmware_version,
			uint8_t* vendor_specific_buf,
			size_t* vendor_specific_buf_size);
uint32_t Tlcl1GetVersion(uint32_t* vendor, uint64_t* firmware_version,
			uint8_t* vendor_specific_buf,
			size_t* vendor_specific_buf_size);
uint32_t Tlcl2GetVersion(uint32_t* vendor, uint64_t* firmware_version,
			uint8_t* vendor_specific_buf,
			size_t* vendor_specific_buf_size);

uint32_t Tlcl1IFXFieldUpgradeInfo(TPM1_IFX_FIELDUPGRADEINFO *info);
uint32_t Tlcl2IFXFieldUpgradeInfo(TPM2_IFX_FIELDUPGRADEINFO *info);

uint32_t Tlcl1ReadPubek(uint32_t* public_exponent,
		       uint8_t* modulus,
		       uint32_t* modulus_size);

uint32_t Tlcl1TakeOwnership(uint8_t enc_owner_auth[TPM_RSA_2048_LEN],
			   uint8_t enc_srk_auth[TPM_RSA_2048_LEN],
			   uint8_t owner_auth[TPM_AUTH_DATA_LEN]);

uint32_t Tlcl1CreateDelegationFamily(uint8_t family_label);

uint32_t Tlcl1ReadDelegationFamilyTable(TPM_FAMILY_TABLE_ENTRY *table,
				       uint32_t* table_size);

#endif  /* TPM_LITE_TLCL_VERSIONED_H_ */
