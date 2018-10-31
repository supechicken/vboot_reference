/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "2sysincludes.h"
#include "tlcl.h"
uint32_t TlclLibInit(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2LibInit();
  } else {
    return Tlcl1LibInit();
  }
}

uint32_t TlclLibClose(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2LibClose();
  } else {
    return Tlcl1LibClose();
  }
}

uint32_t TlclSendReceive(const uint8_t *request, uint8_t *response,
			 int max_length) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SendReceive(request, response, max_length);
  } else {
    return Tlcl1SendReceive(request, response, max_length);
  }
}

int TlclPacketSize(const uint8_t *packet) {
  if (GetTpmVersion() == 2) {
    return Tlcl2PacketSize(packet);
  } else {
    return Tlcl1PacketSize(packet);
  }
}

uint32_t TlclStartup(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2Startup();
  } else {
    return Tlcl1Startup();
  }
}

uint32_t TlclSaveState(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SaveState();
  } else {
    return Tlcl1SaveState();
  }
}

uint32_t TlclResume(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2Resume();
  } else {
    return Tlcl1Resume();
  }
}

uint32_t TlclSelfTestFull(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SelfTestFull();
  } else {
    return Tlcl1SelfTestFull();
  }
}

uint32_t TlclContinueSelfTest(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2ContinueSelfTest();
  } else {
    return Tlcl1ContinueSelfTest();
  }
}

uint32_t TlclDefineSpace(uint32_t index, uint32_t perm, uint32_t size) {
  if (GetTpmVersion() == 2) {
    return Tlcl2DefineSpace(index, perm, size);
  } else {
    return Tlcl1DefineSpace(index, perm, size);
  }
}

uint32_t TlclDefineSpaceEx(const uint8_t* owner_auth, uint32_t owner_auth_size,
			   uint32_t index, uint32_t perm, uint32_t size,
			   const void* auth_policy, uint32_t auth_policy_size) {
  if (GetTpmVersion() == 2) {
    return Tlcl2DefineSpaceEx(owner_auth, owner_auth_size, index, perm, size,
                              auth_policy, auth_policy_size);
  } else {
    return Tlcl1DefineSpaceEx(owner_auth, owner_auth_size, index, perm, size,
                              auth_policy, auth_policy_size);
  }
}

uint32_t TlclWrite(uint32_t index, const void *data, uint32_t length) {
  if (GetTpmVersion() == 2) {
    return Tlcl2Write(index, data, length);
  } else {
    return Tlcl1Write(index, data, length);
  }
}

uint32_t TlclRead(uint32_t index, void *data, uint32_t length) {
  if (GetTpmVersion() == 2) {
    return Tlcl2Read(index, data, length);
  } else {
    return Tlcl1Read(index, data, length);
  }
}

uint32_t TlclPCRRead(uint32_t index, void *data, uint32_t length) {
  if (GetTpmVersion() == 2) {
    return Tlcl2PCRRead(index, data, length);
  } else {
    return Tlcl1PCRRead(index, data, length);
  }
}

uint32_t TlclWriteLock(uint32_t index) {
  if (GetTpmVersion() == 2) {
    return Tlcl2WriteLock(index);
  } else {
    return Tlcl1WriteLock(index);
  }
}

uint32_t TlclReadLock(uint32_t index) {
  if (GetTpmVersion() == 2) {
    return Tlcl2ReadLock(index);
  } else {
    return Tlcl1ReadLock(index);
  }
}

uint32_t TlclLockPhysicalPresence(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2LockPhysicalPresence();
  } else {
    return Tlcl1LockPhysicalPresence();
  }
}

int TlclIsOwned(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2IsOwned();
  } else {
    return Tlcl1IsOwned();
  }
}

uint32_t TlclForceClear(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2ForceClear();
  } else {
    return Tlcl1ForceClear();
  }
}

uint32_t TlclSetEnable(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SetEnable();
  } else {
    return Tlcl1SetEnable();
  }
}

uint32_t TlclSetDeactivated(uint8_t flag) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SetDeactivated(flag);
  } else {
    return Tlcl1SetDeactivated(flag);
  }
}

uint32_t TlclGetFlags(uint8_t *disable, uint8_t *deactivated,
		      uint8_t *nvlocked) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetFlags(disable, deactivated, nvlocked);
  } else {
    return Tlcl1GetFlags(disable, deactivated, nvlocked);
  }
}

uint32_t TlclSetGlobalLock(void) {
  if (GetTpmVersion() == 2) {
    return Tlcl2SetGlobalLock();
  } else {
    return Tlcl1SetGlobalLock();
  }
}

uint32_t TlclExtend(int pcr_num, const uint8_t *in_digest,
                    uint8_t *out_digest) {
  if (GetTpmVersion() == 2) {
    return Tlcl2Extend(pcr_num, in_digest, out_digest);
  } else {
    return Tlcl1Extend(pcr_num, in_digest, out_digest);
  }
}

uint32_t TlclGetPermissions(uint32_t index, uint32_t *permissions) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetPermissions(index, permissions);
  } else {
    return Tlcl1GetPermissions(index, permissions);
  }
}

uint32_t TlclGetSpaceInfo(uint32_t index, uint32_t *attributes, uint32_t *size,
			  void* auth_policy, uint32_t* auth_policy_size) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetSpaceInfo(index, attributes, size, auth_policy,
                             auth_policy_size);
  } else {
    return Tlcl1GetSpaceInfo(index, attributes, size, auth_policy,
                             auth_policy_size);
  }
}

uint32_t TlclGetOwnership(uint8_t *owned) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetOwnership(owned);
  } else {
    return Tlcl1GetOwnership(owned);
  }
}

uint32_t TlclGetRandom(uint8_t *data, uint32_t length, uint32_t *size) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetRandom(data, length, size);
  } else {
    return Tlcl1GetRandom(data, length, size);
  }
}

uint32_t TlclGetVersion(uint32_t* vendor, uint64_t* firmware_version,
			uint8_t* vendor_specific_buf,
			size_t* vendor_specific_buf_size) {
  if (GetTpmVersion() == 2) {
    return Tlcl2GetVersion(vendor, firmware_version, vendor_specific_buf,
                           vendor_specific_buf_size);
  } else {
    return Tlcl1GetVersion(vendor, firmware_version, vendor_specific_buf,
                           vendor_specific_buf_size);
  }
}
