/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>
#include <stdio.h>
#include <fstream>

using namespace std;

#include "../cgpt/CgptManager.h"

// we don't use these parameters for the libcgpt version.
const char* progname = "";
const char* command = "";

static const Guid p2guid = {{{0, 1, 2, 3, 4, {2, 2, 2, 2, 2, 2}}}};
static const Guid p3guid = {{{0, 6, 5, 4, 2, {3, 3, 3, 3, 3, 3}}}};

class CgptManagerTests
{
public:
  int Run() {
    CgptManager cgptManager;

    int retVal = InitCgptManager(cgptManager);
    if (retVal)
      return retVal;

    // do this test first, as this test erases the partitions
    // created by other tests and it'd be good to have the
    // dummy device file contain the results of all other
    // tests below after the unit test is over (for manual
    // verification if needed).
    retVal = PrioritizeCgptTest(cgptManager);
    if (retVal)
      return retVal;

    // all the following tests build up on the partitions
    // created below.
    retVal = CreateCgptTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = AddCgptTest(cgptManager);
    if (retVal)
      return retVal;

    // all the tests below reuse the partitions added in AddCgptTest.
    retVal = SetPmbrTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = SetSuccessfulAttributeTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = SetNumTriesLeftTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = SetPriorityTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = GetBeginningOffsetTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = GetNumSectorsTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = GetPartitionTypeIdTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = GetPartitionUniqueIdTest(cgptManager);
    if (retVal)
      return retVal;

    retVal = GetPartitionNumberByUniqueIdTest(cgptManager);
    if (retVal)
      return retVal;

    return 0;
  }


  int InitCgptManager(CgptManager& cgptManager) {
    string dummyDevice = "/tmp/DummyFileForCgptManagerTests";
    printf("Initializing CgptManager with %s\n", dummyDevice.c_str());

    int result = InitDummyDevice(dummyDevice);
    if (result != 0) {
      printf("Unable to initialize a dummy device: %s [FAIL]\n", dummyDevice.c_str());
      return result;
    }

    result = cgptManager.Initialize(dummyDevice);
    if (result != kCgptSuccess) {
      printf("Failed to initialize %s [FAIL]\n", dummyDevice.c_str());
      return result;
    }

    return kCgptSuccess;
  }


  int CreateCgptTest(CgptManager& cgptManager) {
    printf("CgptManager->ClearAll \n");
    int result = cgptManager.ClearAll();
    if (result != kCgptSuccess) {
      printf("Failed to clear %s [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 0);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after clearing. [FAIL]\n");
      return result;
    }

    printf("Successfully cleared %s [PASS]\n");
    return 0;
  }

  int AddCgptTest(CgptManager& cgptManager) {
    printf("CgptManager->AddPartition for data partition \n");
    int result = cgptManager.AddPartition(
                                 "data stuff",
                                 guid_linux_data,
                                 guid_unused,
                                 100,
                                 20);
    if (result != kCgptSuccess) {
      printf("Failed to add data partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 1);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for kernel partition \n");
    result = cgptManager.AddPartition(
                                 "kernel stuff",
                                 guid_chromeos_kernel,
                                 p2guid,
                                 200,
                                 30);
    if (result != kCgptSuccess) {
      printf("Failed to add kernel partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 2);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for rootfs partition \n");
    result = cgptManager.AddPartition(
                                 "rootfs stuff",
                                 guid_chromeos_rootfs,
                                 p3guid,
                                 300,
                                 40);
    if (result != kCgptSuccess) {
      printf("Failed to add rootfs partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 3);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for ESP partition \n");
    result = cgptManager.AddPartition(
                                 "ESP stuff",
                                 guid_efi,
                                 guid_unused,
                                 400,
                                 50);
    if (result != kCgptSuccess) {
      printf("Failed to add ESP partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 4);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for Future partition \n");
    result = cgptManager.AddPartition(
                                 "future stuff",
                                 guid_chromeos_reserved,
                                 guid_unused,
                                 500,
                                 60);
    if (result != kCgptSuccess) {
      printf("Failed to add future partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 5);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    Guid guid_random =
    {{{0x2364a860,0xbf63,0x42fb,0xa8,0x3d,{0x9a,0xd3,0xe0,0x57,0xfc,0xf5}}}};

    printf("CgptManager->AddPartition for random partition \n");
    result = cgptManager.AddPartition(
                                 "random stuff",
                                 guid_random,
                                 guid_unused,
                                 600,
                                 70);

    if (result != kCgptSuccess) {
      printf("Failed to add random partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgptManager, 6);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("AddCgpt test successful. [PASS]\n");

    return 0;
  }


  int SetPmbrTest(CgptManager& cgptManager) {
    printf("CgptManager::SetPmbr\n");

    string bootFileName = "/tmp/BootFileForCgptManagerTests";
    printf("Creating bootfile %s\n", bootFileName.c_str());

    int result = CreateBootFile(bootFileName);
    if (result != 0) {
      printf("Unable to create boot file: %s [FAIL]\n", bootFileName.c_str());
      return result;
    }

    uint32_t expectedBootPartitionNumber = 2;
    result = cgptManager.SetPmbr(expectedBootPartitionNumber,
                                     bootFileName,
                                     true);
    if (result != kCgptSuccess) {
      printf("Failed to set pmbr [FAIL]\n");
      return result;
    }

    printf("Successfully set pmbr. [PASS]\n");

    // get the pmbr boot partition number and check.
    uint32_t actualBootPartitionNumber;
    result = cgptManager.GetPmbrBootPartitionNumber(actualBootPartitionNumber);
    if (result != kCgptSuccess) {
      printf("Failed to get pmbr partition number. [FAIL]\n");
      return result;
    }

    printf("Boot Partition: Expected = %d, Actual = %d\n",
           expectedBootPartitionNumber,
           actualBootPartitionNumber);

    if (actualBootPartitionNumber != expectedBootPartitionNumber) {
      printf("Boot partition number not set as expected. [FAIL]\n");
      return result;
    }

    printf("Pmbr test successful. [PASS]\n");

    return 0;
  }

  int SetSuccessfulAttributeTest(CgptManager& cgptManager) {
    printf("CgptManager::SetSuccessfulAttributeTest\n");
    bool isSuccessful = true;
    int partitionNumber = 2;
    int result = cgptManager.SetSuccessful(partitionNumber, isSuccessful);
    if (result != kCgptSuccess) {
      printf("Failed to Set Successful attribute. [FAIL]\n");
      return result;
    }

    printf("Successfully set Successful attribute once. [PASS]\n");

    result = CheckSuccessfulAttribute(cgptManager,
                                      partitionNumber,
                                      isSuccessful);
    if (result != kCgptSuccess)
      return result;

    isSuccessful = false;
    partitionNumber = 2;
    result = cgptManager.SetSuccessful(partitionNumber, isSuccessful);
    if (result != kCgptSuccess) {
      printf("Failed to Set Successful attribute. [FAIL]\n");
      return result;
    }

    printf("Successfully set Successful attribute again. [PASS]\n");

    result = CheckSuccessfulAttribute(cgptManager,
                                      partitionNumber,
                                      isSuccessful);

    if (result != kCgptSuccess)
      return result;

    printf("Successful attribute test successful. [PASS]\n");

    return 0;
  }

  int SetNumTriesLeftTest(CgptManager& cgptManager) {
    printf("CgptManager::SetNumTriesTest\n");
    int numTriesLeft = 6;
    int partitionNumber = 2;
    int result = cgptManager.SetNumTriesLeft(partitionNumber, numTriesLeft);
    if (result != kCgptSuccess) {
      printf("Failed to Set NumTries. [FAIL]\n");
      return result;
    }

    printf("NumTries set. [PASS]\n");

    result = CheckNumTriesLeft(cgptManager,
                           partitionNumber,
                           numTriesLeft);
    if (result != kCgptSuccess)
      return result;

    // change it and try again if the change is reflected.

    numTriesLeft = 5;
    partitionNumber = 2;
    result = cgptManager.SetNumTriesLeft(partitionNumber, numTriesLeft);
    if (result != kCgptSuccess) {
      printf("Failed to Set NumTries. [FAIL]\n");
      return result;
    }

    printf("NumTries set again. [PASS]\n");

    result = CheckNumTriesLeft(cgptManager,
                           partitionNumber,
                           numTriesLeft);

    if (result != kCgptSuccess)
      return result;

    printf("NumTries test successful. [PASS]\n");

    return 0;
  }


  int SetPriorityTest(CgptManager& cgptManager) {
    printf("CgptManager::SetPriorityTest\n");
    uint8_t priority = 8;
    int partitionNumber = 2;
    int result = cgptManager.SetPriority(partitionNumber, priority);
    if (result != kCgptSuccess) {
      printf("Failed to Set Priority. [FAIL]\n");
      return result;
    }

    printf("Priority set once. [PASS]\n");

    result = CheckPriority(cgptManager,
                           partitionNumber,
                           priority);
    if (result != kCgptSuccess)
      return result;

    // change it and try again if the change is reflected.

    priority = 4;
    partitionNumber = 2;
    result = cgptManager.SetPriority(partitionNumber, priority);
    if (result != kCgptSuccess) {
      printf("Failed to Set Priority. [FAIL]\n");
      return result;
    }

    printf("Priority set again. [PASS]\n");

    result = CheckPriority(cgptManager,
                           partitionNumber,
                           priority);

    if (result != kCgptSuccess)
      return result;

    printf("Priority test successful. [PASS]\n");
    return 0;
  }

  int GetBeginningOffsetTest(CgptManager& cgptManager) {
    printf("CgptManager::GetBeginningOffsetTest\n");
    int partitionNumber = 2;
    uint64_t expectedOffset = 200; // value comes from AddCgptTest.
    int result = CheckOffset(cgptManager,
                             partitionNumber,
                             expectedOffset);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetBeginningOffset test successful. [PASS]\n");
    return 0;
  }

  int GetNumSectorsTest(CgptManager& cgptManager) {
    printf("CgptManager::GetNumSectorsTest\n");
    int partitionNumber = 2;
    uint64_t expectedNumSectors = 30; // value comes from AddCgptTest.
    int result = CheckNumSectors(cgptManager,
                                 partitionNumber,
                                 expectedNumSectors);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetNumSectors test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionTypeIdTest(CgptManager& cgptManager) {
    printf("CgptManager::GetPartitionTypeIdTest\n");
    int partitionNumber = 2;
    Guid expectedTypeId = guid_chromeos_kernel;
    int result = CheckPartitionTypeId(cgptManager,
                                      partitionNumber,
                                      expectedTypeId);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionTypeId test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionUniqueIdTest(CgptManager& cgptManager) {
    printf("CgptManager::GetPartitionTypeIdTest\n");
    int partitionNumber = 2;
    Guid expectedUniqueId = p2guid;
    int result = CheckPartitionUniqueId(cgptManager,
                                        partitionNumber,
                                        expectedUniqueId);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionUniqueId test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionNumberByUniqueIdTest(CgptManager& cgptManager) {
    printf("CgptManager::GetPartitionNumberByUniqueIdTest\n");
    Guid uniqueId = p3guid;
    int expectedPartitionNumber = 3;
    int result = CheckPartitionNumberByUniqueId(cgptManager,
                                                uniqueId,
                                                expectedPartitionNumber);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionNumberByUniqueId test successful. [PASS]\n");
    return 0;
  }


  int PrioritizeCgptTest(CgptManager& cgptManager) {
    printf("CgptManager::PrioritizeCgpt\n");

    int result = cgptManager.ClearAll();
    if (result != kCgptSuccess) {
      return result;
    }

    printf("CgptManager->AddPartition for kernel 1 partition \n");
    int k1PartitionNumber = 1;
    result = cgptManager.AddPartition(
                                 "k1",
                                 guid_chromeos_kernel,
                                 guid_unused,
                                 100,
                                 10);
    if (result != kCgptSuccess) {
      printf("Failed to add k1 partition  [FAIL]\n");
      return result;
    }

    int k2PartitionNumber = 2;
    printf("CgptManager->AddPartition for kernel 2 partition \n");
    result = cgptManager.AddPartition(
                                 "k2",
                                 guid_chromeos_kernel,
                                 p2guid,
                                 200,
                                 20);
    if (result != kCgptSuccess) {
      printf("Failed to add k2partition  [FAIL]\n");
      return result;
    }


    int k3PartitionNumber = 3;
    printf("CgptManager->AddPartition for kernel 3 partition \n");
    result = cgptManager.AddPartition(
                                 "k3",
                                 guid_chromeos_kernel,
                                 p3guid,
                                 300,
                                 30);

    if (result != kCgptSuccess) {
      printf("Failed to add k3 partition  [FAIL]\n");
      return result;
    }

    uint8_t k1Priority = 2;
    uint8_t k2Priority = 0;
    uint8_t k3Priority = 0;
    result = cgptManager.SetHighestPriority(k1PartitionNumber,
                                            k1Priority);
    if (result != kCgptSuccess) {
      printf("Failed to SetHighestPriority [FAIL]\n");
      return result;
    }

    printf("Successfully set SetHighestPriority. [PASS]\n");

    result = CheckPriority(cgptManager, k1PartitionNumber, k1Priority);
    if (result != kCgptSuccess)
      return result;

    result = CheckPriority(cgptManager, k2PartitionNumber, k2Priority);
    if (result != kCgptSuccess)
      return result;

    result = CheckPriority(cgptManager, k3PartitionNumber, k3Priority);
    if (result != kCgptSuccess)
      return result;

    printf("SetHighestPriority test successful. [PASS]\n");
    return 0;
  }

  int InitDummyDevice(const string& dummyDevice) {
    ofstream ddStream(dummyDevice.c_str());

    const int kNumSectors = 1000;
    const int kSectorSize = 512;
    const char fillChar = '7';
    for(int i = 0; i < kNumSectors * kSectorSize; i++) {
      // fill the file with some character.
      ddStream << fillChar;
    }

    ddStream.flush();
    ddStream.close();

    return 0;
  }

  int CreateBootFile(const string& bootFileName) {
    ofstream bootStream(bootFileName.c_str());

    const int kNumSectors = 1;
    const int kSectorSize = 512;
    const char fillChar = '8';
    for(int i = 0; i < kNumSectors * kSectorSize; i++) {
      // fill the file with some character.
      bootStream << fillChar;
    }

    bootStream.flush();
    bootStream.close();

    return 0;
  }

  int CheckNumPartitions(const CgptManager& cgptManager,
                               uint8_t expectedNumPartitions) {
    uint8_t actualNumPartitions;
    int result = cgptManager.GetNumNonEmptyPartitions(actualNumPartitions);
    if (result != kCgptSuccess) {
      printf("Failed to get partition size. result = %d [FAIL]\n", result);
      return -1;
    }

    printf("NumPartitions: Expected = %d, Actual = %d\n",
           expectedNumPartitions,
           actualNumPartitions);

    if (expectedNumPartitions != actualNumPartitions) {
      printf("Actual number of partitions doesn't match expected number."
             "[FAIL]\n");
      return -1;
    }

    return kCgptSuccess;
  }



  int CheckSuccessfulAttribute(const CgptManager& cgptManager,
                               int partitionNumber,
                               bool expectedIsSuccessful) {
    // get the Successful attribute and check.
    bool isSuccessful;
    int result = cgptManager.GetSuccessful(partitionNumber, isSuccessful);
    if (result != kCgptSuccess) {
      printf("Failed to get Successful attr for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    printf("Successful attr for partition: %d: Expected = %d, Actual = %d\n",
           partitionNumber,
           expectedIsSuccessful,
           isSuccessful);

    if (isSuccessful != expectedIsSuccessful) {
      printf("Successful attr for partition %d not set as expected. [FAIL]\n",
             partitionNumber);
      return -1;
    }

    return kCgptSuccess;
  }



  int CheckNumTriesLeft(const CgptManager& cgptManager,
                          int partitionNumber,
                          int expectedNumTriesLeft) {
    // get the numTries attribute and check.
    int numTriesLeft;
    int result = cgptManager.GetNumTriesLeft(partitionNumber, numTriesLeft);
    if (result != kCgptSuccess) {
      printf("Failed to get numTries for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    printf("numTries for partition: %d: Expected = %d, Actual = %d\n",
           partitionNumber,
           expectedNumTriesLeft,
           numTriesLeft);

    if (numTriesLeft != expectedNumTriesLeft) {
      printf("numTries for partition %d not set as expected. [FAIL]\n",
             partitionNumber);
      return -1;
    }

    return kCgptSuccess;
  }

  int CheckPriority(CgptManager& cgptManager,
                       int partitionNumber,
                       uint8_t expectedPriority) {
    // get the priority and check.
    uint8_t actualPriority;
    int result = cgptManager.GetPriority(partitionNumber, actualPriority);
    if (result != kCgptSuccess) {
      printf("Failed to get priority for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    printf("Priority for partition: %d: Expected = %d, Actual = %d\n",
           partitionNumber,
           expectedPriority,
           actualPriority);

    if (actualPriority != expectedPriority) {
      printf("Priority for partition %d not set as expected. [FAIL]\n",
             partitionNumber);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckOffset(CgptManager& cgptManager,
                  int partitionNumber,
                  uint64_t expectedOffset) {
    // get the offset and check.
    uint64_t actualOffset;
    int result = cgptManager.GetBeginningOffset(partitionNumber, actualOffset);
    if (result != kCgptSuccess) {
      printf("Failed to get offset for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    printf("Offset for partition: %d: Expected = %d, Actual = %d\n",
           partitionNumber,
           expectedOffset,
           actualOffset);

    if (actualOffset != expectedOffset) {
      printf("Offset for partition %d not set as expected. [FAIL]\n",
             partitionNumber);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckNumSectors(CgptManager& cgptManager,
                      int partitionNumber,
                      uint64_t expectedNumSectors) {
    // get the numSectors and check.
    uint64_t actualNumSectors;
    int result = cgptManager.GetNumSectors(partitionNumber, actualNumSectors);
    if (result != kCgptSuccess) {
      printf("Failed to get numSectors for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    printf("NumSectors for partition: %d: Expected = %d, Actual = %d\n",
           partitionNumber,
           expectedNumSectors,
           actualNumSectors);

    if (actualNumSectors != expectedNumSectors) {
      printf("NumSectors for partition %d not set as expected. [FAIL]\n",
             partitionNumber);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckPartitionTypeId(CgptManager& cgptManager,
                           int partitionNumber,
                           const Guid& expectedPartitionTypeId) {
    // get the partition type id and check.
    Guid actualPartitionTypeId;
    int result = cgptManager.GetPartitionTypeId(partitionNumber,
                                                actualPartitionTypeId);
    if (result != kCgptSuccess) {
      printf("Failed to get partition type id for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    result = CheckGuidMatch("PartitionTypeId",
                            expectedPartitionTypeId,
                            actualPartitionTypeId);
    if (result != kCgptSuccess) {
      return result;
    }

    return kCgptSuccess;
  }


  int CheckPartitionUniqueId(CgptManager& cgptManager,
                             int partitionNumber,
                             const Guid& expectedPartitionUniqueId) {
    // get the partition unique id and check.
    Guid actualPartitionUniqueId;
    int result = cgptManager.GetPartitionUniqueId(partitionNumber,
                                                  actualPartitionUniqueId);
    if (result != kCgptSuccess) {
      printf("Failed to get partition unique id for partition: %d. [FAIL]\n",
              partitionNumber);
      return result;
    }

    result = CheckGuidMatch("PartitionUniqueId",
                            expectedPartitionUniqueId,
                            actualPartitionUniqueId);
    if (result != kCgptSuccess) {
      return result;
    }

    return kCgptSuccess;
  }

  int CheckPartitionNumberByUniqueId(CgptManager& cgptManager,
                                     const Guid& uniqueId,
                                     uint32_t expectedPartitionNumber) {
    // get the partition unique id and check.
    uint32_t actualPartitionNumber;
    int result = cgptManager.GetPartitionNumberByUniqueId(uniqueId,
                                                actualPartitionNumber);
    if (result != kCgptSuccess) {
      printf("Failed to get partition number. [FAIL]\n");
      return result;
    }

    printf("PartitionNumber: Expected = %d, Actual = %d\n",
           expectedPartitionNumber,
           actualPartitionNumber);

    if (actualPartitionNumber != expectedPartitionNumber) {
      printf("PartitionNumber not same as expected. [FAIL]\n");
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckGuidMatch(string message,
                     const Guid& expectedId,
                     const Guid& actualId) {
    char expectedIdStr[GUID_STRLEN];
    GuidToStr(&expectedId, expectedIdStr, sizeof expectedIdStr);

    char actualIdStr[GUID_STRLEN];
    GuidToStr(&actualId, actualIdStr, sizeof actualIdStr);

    printf("%s: Expected = %s, Actual = %s\n",
           message.c_str(),
           expectedIdStr,
           actualIdStr);

    if (!GuidEqual(&expectedId,& actualId)) {
      printf("%s: Guids do not match as expected. [FAIL]\n",
             message.c_str());
      return -1;
    }

    return kCgptSuccess;
  }

};

int main(int argc, char* argv[]) {
    CgptManagerTests cgptManagerTests;

    printf("Running CgptManagerTests...\n");

    int retVal = cgptManagerTests.Run();

    printf("Finished CgptManagerTests with exit code = %d\n", retVal);
    return retVal;
}

#if 0


  int InitializeGpt(CgptManager& cgpt) {

    int result = cgpt.Initialize(device);
    if (result != kCgptSuccess) {
      return result;
    }

    // todo: similar error checking needs to be added to all the code below.

    // todo: need to define all the *BeginOffset and *Size constants below
    // based on the actual values in the .sh files.

    cgpt.SetPartition(kState, "STATE",  kDataPartition,   StateBeginOffset, StateSize);
    cgpt.SetPartition(kKernA, "KERN-A", kKernelPartition, KernABeginOffset, KernASize);
    cgpt.SetPartition(kRootA, "ROOT-A", kRootFsPartition, RootABeginOffset, RootASize);
    cgpt.SetPartition(kKernB, "KERN-B", kKernelPartition, KernBBeginOffset, KernBSize);
    cgpt.SetPartition(kRootB, "ROOT-B", kRootFsPartition, RootBBeginOffset, RootBSize);
    cgpt.SetPartition(kKernC, "KERN-C", kKernelPartition, KernCBeginOffset, KernCSize);
    cgpt.SetPartition(kRootC, "ROOT-C", kRootFsPartition, RootCBeginOffset, RootCSize);
    cgpt.SetPartition(kOem, "OEM",    kDataPartition, OemBeginOffset,   OemSize);

    cgpt.SetPartition(kReserved9,  "Reserved", kReservedPartition,
                      Reserved9BeginOffset,  Reserved9Size);
    cgpt.SetPartition(kReserved10, "Reserved", kReservedPartition,
                      Reserved10BeginOffset, Reserved10Size);

    cgpt.SetPartition(kFirmware, "RWFW", kFirmwarePartition, FirmwareBeginOffset, FirmwareSize);
    cgpt.SetPartition(kEfi, "EFI-SYSTEM", kEfiPartition, EfiBeginOffset, EfiSize);

    cgpt.SetPmbr(kEfi, "/tmp/pmbr.bin", true);

    cgpt.SetSuccessful(kKernA, false);
    cgpt.SetNumTriesLeft(kKernA, 15);
    cgpt.SetPriority(kKernA, 2);

    cgpt.SetSuccessful(kKernB, false);
    cgpt.SetNumTriesLeft(kKernB, 15);
    cgpt.SetPriority(kKernB, 1);

    cgpt.SetSuccessful(kKernC, false);
    cgpt.SetNumTriesLeft(kKernC, 0);
    cgpt.SetPriority(kKernC, 0);

    cgpt.SetKernelAttributes(kKernB, 0, 15, 1);
    cgpt.SetKernelAttributes(kKernC, 0, 0,  0);

    return result;
  }

  // This method shows how the code will look like when we replace
  // the cgpt usage in all the other installer scripts.
  void OtherUses(ChromeOsGuidPartitionTable& cgpt) {
    // set kernA as the kernel to boot from when it's successful.
    cgpt.SetSuccessful(kKernA);
    cgpt.SetHighestPriority(kKernA);

    // get the successful attribute value
    bool isKernASuccessful = cgpt.GetSuccessful(kKernA);

    // get the Number of Tries remaining attribute value
    int remainingTries = cgpt.GetNumTriesLeft(kKernB);

    // get the successful attribute value
    int kcPriority = cgpt.GetPriority(kKernC);

    // set highest priority with a guid instead of a partition number
    Guid KernBGuid;
    cgpt.SetHighestPriority(KernBGuid);

    // get the attributes of a partition
    int rootAOffset = cgpt.GetBeginningOffset(kRootA);
    int kernBSize   = cgpt.GetNumSectors(kKernB);
    PartitionNumber efiPartitionNumber = cgpt.GetPartitionByLabel("EFI-SYSTEM");

    Guid KernAGuid;
    int kernAPartitionNumber = cgpt.GetRootFsPartitionForKernelGuid(KernAGuid);

    if (cgpt.Validate() != kCgptSuccess)  {
      // invalid.
    }
  }

#endif

