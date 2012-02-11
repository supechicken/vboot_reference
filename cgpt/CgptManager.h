// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VBOOT_REFERENCE_CGPT_CGPTMANAGER_H_
#define VBOOT_REFERENCE_CGPT_CGPTMANAGER_H_

extern "C" {
#include "cgpt_params.h"
}

#include <string>

// This file defines a simple C++ wrapper class interface for the cgpt methods

// These are the possible error codes that can be returned by the CgptManager
typedef enum CgptErrorCode
{
  kCgptSuccess = 0,
  kCgptNotInitialized = 1,
  kCgptUnknownError = 2,
} CgptErrorCode;


// CgptManager exposes methods to manipulate the Guid Partition Table as needed for
// ChromeOS scenarios.
class CgptManager {
  public:
    // Default constructor. The Initialize method must be called before
    // any other method can be called on this class.
    CgptManager();

    // Destructor. Automatically closes any opened device.
    ~CgptManager();


    // Opens the given deviceName (e.g. "/dev/sdc") and initializes this object
    // with the Guid Partition Table of that device. This is the first method
    // that should be called on this class.  Otherwise those methods will return
    // kCgptNotInitialized.
    // Returns kCgptSuccess or an appropriate error code.
    // This device is automatically closed when this object is destructed.
    int Initialize(const std::string& deviceName);

    // Clears all the existing contents of the GPT and PMBR on the current
    // device.
    int ClearAll();

    // Adds a new partition at the end of the existing partitions
    // with the new label, type, unique Id, offset and size.
    // Returns kCgptSuccess or an appropriate error code.
    int AddPartition(
      const std::string& label,
      const Guid& partitionTypeGuid,
      const Guid& uniqueId,
      uint64_t beginningOffset,
      uint64_t numSectors
      );


    // Populates numPartitions parameter with the number of partitions
    // that are currently on this device and not empty.
    // Returns kCgptSuccess or an appropriate error code.
    int GetNumNonEmptyPartitions(uint8_t& numPartitions) const;


    // Sets the Protective Master Boot Record on this device with the given
    // bootPartition number after populating the MBR with the contents of the
    // given bootFileName. It also creates a legacy partition if
    // shouldCreateLegacyPartition is true.
    // Note: Strictly speaking, the PMBR is not part of the GPT, but it is
    // included here for ease of use.
    int SetPmbr(
      uint32_t bootPartitionNumber,
      const std::string& bootFileName,
      bool shouldCreateLegacyPartition
      );

    // Populates bootPartition with the partition number that's set to
    // boot in the PMBR.
    // Returns kCgptSuccess or an appropriate error code.
    int GetPmbrBootPartitionNumber(uint32_t& bootPartition) const;


    // Sets the "successful" attribute of the given kernelPartition to 0 or 1
    // based on the value of isSuccessful being true (1) or false(0)
    // Returns kCgptSuccess or an appropriate error code.
    int SetSuccessful(uint32_t partitionNumber, bool isSuccessful);

    // Populates isSuccessful to true if the successful attribute in the
    // given kernelPartition is non-zero, or to false if it's zero.
    // Returns kCgptSuccess or an appropriate error code.
    int GetSuccessful(uint32_t partitionNumber, bool& isSuccessful) const;

    // Sets the "NumTriesLeft" attribute of the given kernelPartition to
    // the given numTriesLeft value.
    // Returns kCgptSuccess or an appropriate error code.
    int SetNumTriesLeft(uint32_t partitionNumber, int numTriesLeft);

    // Populates the numTriesLeft parameter with the value of the NumTriesLeft
    // attribute of the given kernelPartition.
    // Returns kCgptSuccess or an appropriate error code.
    int GetNumTriesLeft(uint32_t partitionNumber, int& numTriesLeft) const;

    // Sets the "Priority" attribute of the given kernelPartition to
    // the given priority value.
    // Returns kCgptSuccess or an appropriate error code.
    int SetPriority(uint32_t partitionNumber, uint8_t priority);

    // Populates the priority parameter with the value of the Priority
    // attribute of the given kernelPartition.
    // Returns kCgptSuccess or an appropriate error code.
    int GetPriority(uint32_t partitionNumber, uint8_t& priority) const;

    // Populates the offset parameter with the beginning offset of the
    // given partition.
    // Returns kCgptSuccess or an appropriate error code.
    int GetBeginningOffset(uint32_t partitionNumber, uint64_t& offset) const;

    // Populates the number of sectors in the given partition.
    // Returns kCgptSuccess or an appropriate error code.
    int GetNumSectors(uint32_t partitionNumber, uint64_t& numSectors) const;

    // Populates the typeId parameter with the partition type id
    // (these are the standard ids for kernel, rootfs, etc.)
    // of the partition corresponding to the given partitionNumber.
    // Returns kCgptSuccess or an appropriate error code.
    int GetPartitionTypeId(uint32_t partitionNumber, Guid& typeId) const;

    // Populates the uniqueId parameter with the Guid that uniquely identifies
    // the given partitionNumber.
    // Returns kCgptSuccess or an appropriate error code.
    int GetPartitionUniqueId(uint32_t partitionNumber, Guid& uniqueId) const;

    // Populates the partitionNumber parameter with the partition number of the
    // partition which is uniquely identified by the given uniqueId.
    // Returns kCgptSuccess or an appropriate error code.
    int GetPartitionNumberByUniqueId(const Guid& uniqueId,
                                     uint32_t& partitionNumber) const;

    // Sets the "Priority" attribute of given kernelPartition to the value
    // specified in higestPriority parameter. In addition, also reduces the
    // priorities of all the other kernel partitions, if necessary, to ensure
    // no other partition has a higher priority. It does preserve the relative
    // ordering among the remaining partitions and doesn't touch the partitions
    // whose priorities are zero.
    // Returns kCgptSuccess or an appropriate error code.
    int SetHighestPriority(uint32_t partitionNumber, uint8_t highestPriority);


    // Runs the sanity checks on the CGPT and MBR and
    // Returns kCgptSuccess if everything is valid or an appropriate error code
    // if there's anything invalid or if there's any error encountered during
    // the validation.
    int Validate();

  private:
    // todo: commenting out due to this error. need to figure out why
    // the compiler is not happy with this macro:
    // CgptManager.h:158:41: error: ISO C++ forbids declaration of
    // 'DISALLOW_COPY_AND_ASSIGN' with no type [-fpermissive]
    // DISALLOW_COPY_AND_ASSIGN(CgptManager);

    std::string deviceName;
    bool isInitialized;
};

#endif  // VBOOT_REFERENCE_CGPT_CGPTMANAGER_H_

