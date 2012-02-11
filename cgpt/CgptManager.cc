// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#include "CgptManager.h"

using namespace std;

// This file implements the C++ wrapper methods over the C cgpt methods.

CgptManager::CgptManager() {
  isInitialized = false;
}

CgptManager::~CgptManager() {
}

int CgptManager::Initialize(const string& deviceName) {
  this->deviceName = deviceName;
  isInitialized = true;
  return kCgptSuccess;
}

// Clears all the existing contents of the GPT and PMBR on the current
// device.
int CgptManager::ClearAll() {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptCreateParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.zap = 0;

  int retValue = cgpt_create(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}

int CgptManager::AddPartition(
  const string& label,
  const Guid& partitionTypeGuid,
  const Guid& uniqueId,
  uint64_t beginningOffset,
  uint64_t numSectors
  ) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.label = const_cast<char *>(label.c_str());

  params.type_guid = partitionTypeGuid;
  params.set_type = 1;

  params.begin = beginningOffset;
  params.set_begin = 1;

  params.size = numSectors;
  params.set_size = 1;

  if (!IsZero(&uniqueId)) {
     params.unique_guid = uniqueId;
     params.set_unique = 1;
  }

  int retValue = cgpt_add(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}


int CgptManager::GetNumNonEmptyPartitions(uint8_t& numPartitions) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptShowParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  int retValue = cgpt_get_num_non_empty_partitions(&params);

  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  numPartitions = params.numPartitions;
  return kCgptSuccess;
}

int CgptManager::SetPmbr(
  uint32_t bootPartitionNumber,
  const string& bootFileName,
  bool shouldCreateLegacyPartition
  ) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptBootParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  if (!bootFileName.empty())
    params.bootfile = const_cast<char *>(bootFileName.c_str());

  params.partition = bootPartitionNumber;
  params.create_pmbr = shouldCreateLegacyPartition;

  return cgpt_boot(&params);
}

int CgptManager::GetPmbrBootPartitionNumber(uint32_t& bootPartition) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptBootParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());

  int retValue = cgpt_get_boot_partition_number(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  bootPartition = params.partition;
  return kCgptSuccess;
}

int CgptManager::SetSuccessful(uint32_t partitionNumber,
                               bool isSuccessful) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  params.successful = isSuccessful;
  params.set_successful = true;

  int retValue = cgpt_set_attributes(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}

int CgptManager::GetSuccessful(uint32_t partitionNumber,
                               bool& isSuccessful) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  isSuccessful = params.successful;
  return kCgptSuccess;
}


int CgptManager::SetNumTriesLeft(uint32_t partitionNumber,
                                 int numTries) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  params.tries = numTries;
  params.set_tries = true;

  int retValue = cgpt_set_attributes(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}

int CgptManager::GetNumTriesLeft(uint32_t partitionNumber,
                                 int& numTries) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  numTries = params.tries;
  return kCgptSuccess;
}

int CgptManager::SetPriority(uint32_t partitionNumber,
                             uint8_t priority) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  params.priority = priority;
  params.set_priority = true;

  int retValue = cgpt_set_attributes(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}

int CgptManager::GetPriority(uint32_t partitionNumber,
                             uint8_t& priority) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  priority = params.priority;
  return kCgptSuccess;
}

int CgptManager::GetBeginningOffset(uint32_t partitionNumber,
                                    uint64_t& offset) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  offset = params.begin;
  return kCgptSuccess;
}

int CgptManager::GetNumSectors(uint32_t partitionNumber,
                               uint64_t& numSectors) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  numSectors = params.size;
  return kCgptSuccess;
}

int CgptManager::GetPartitionTypeId(uint32_t partitionNumber,
                                    Guid& typeId) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  typeId = params.type_guid;
  return kCgptSuccess;
}

int CgptManager::GetPartitionUniqueId(uint32_t partitionNumber,
                                      Guid& uniqueId) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.partition = partitionNumber;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  uniqueId = params.unique_guid;
  return kCgptSuccess;

}

int CgptManager::GetPartitionNumberByUniqueId(
                    const Guid& uniqueId,
                    uint32_t& partitionNumber) const {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptAddParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.unique_guid = uniqueId;
  params.set_unique = 1;

  int retValue = cgpt_get_partition_details(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  partitionNumber = params.partition;
  return kCgptSuccess;

}


int CgptManager::SetHighestPriority(uint32_t partitionNumber,
                                    uint8_t highestPriority) {
  if (!isInitialized)
    return kCgptNotInitialized;

  CgptPrioritizeParams params;
  memset(&params, 0, sizeof(params));

  params.driveName = const_cast<char *>(this->deviceName.c_str());
  params.set_partition = partitionNumber;

  params.max_priority = highestPriority;

  int retValue = cgpt_prioritize(&params);
  if (retValue != CGPT_OK)
    return kCgptUnknownError;

  return kCgptSuccess;
}

int CgptManager::Validate() {
  if (!isInitialized)
    return kCgptNotInitialized;

  uint8_t numPartitions;

  // GetNumNonEmptyPartitions does the check for GptSanityCheck.
  // so call it (ignore the numPartitions result) and just return
  // its success/failure result.
  return GetNumNonEmptyPartitions(numPartitions);
}

