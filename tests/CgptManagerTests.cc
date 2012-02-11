/* Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <string>
#include <stdio.h>
#include <fstream>

using std::string;
using std::ofstream;

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
    CgptManager cgpt_manager;

    int retval = InitCgptManager(cgpt_manager);
    if (retval)
      return retval;

    // do this test first, as this test erases the partitions
    // created by other tests and it'd be good to have the
    // dummy device file contain the results of all other
    // tests below after the unit test is over (for manual
    // verification if needed).
    retval = PrioritizeCgptTest(cgpt_manager);
    if (retval)
      return retval;

    // all the following tests build up on the partitions
    // created below.
    retval = CreateCgptTest(cgpt_manager);
    if (retval)
      return retval;

    retval = AddCgptTest(cgpt_manager);
    if (retval)
      return retval;

    // all the tests below reuse the partitions added in AddCgptTest.
    retval = SetPmbrTest(cgpt_manager);
    if (retval)
      return retval;

    retval = SetSuccessfulAttributeTest(cgpt_manager);
    if (retval)
      return retval;

    retval = SetNumTriesLeftTest(cgpt_manager);
    if (retval)
      return retval;

    retval = SetPriorityTest(cgpt_manager);
    if (retval)
      return retval;

    retval = GetBeginningOffsetTest(cgpt_manager);
    if (retval)
      return retval;

    retval = GetNumSectorsTest(cgpt_manager);
    if (retval)
      return retval;

    retval = GetPartitionTypeIdTest(cgpt_manager);
    if (retval)
      return retval;

    retval = GetPartitionUniqueIdTest(cgpt_manager);
    if (retval)
      return retval;

    retval = GetPartitionNumberByUniqueIdTest(cgpt_manager);
    if (retval)
      return retval;

    return 0;
  }


  int InitCgptManager(CgptManager& cgpt_manager) {
    string dummy_device = "/tmp/DummyFileForCgptManagerTests.bin";
    printf("Initializing CgptManager with %s\n", dummy_device.c_str());

    int result = InitDummyDevice(dummy_device);
    if (result != 0) {
      printf("Unable to initialize a dummy device: %s [FAIL]\n",
              dummy_device.c_str());
      return result;
    }

    result = cgpt_manager.Initialize(dummy_device);
    if (result != kCgptSuccess) {
      printf("Failed to initialize %s [FAIL]\n", dummy_device.c_str());
      return result;
    }

    return kCgptSuccess;
  }


  int CreateCgptTest(CgptManager& cgpt_manager) {
    printf("CgptManager->ClearAll \n");
    int result = cgpt_manager.ClearAll();
    if (result != kCgptSuccess) {
      printf("Failed to clear %s [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 0);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after clearing. [FAIL]\n");
      return result;
    }

    printf("Successfully cleared %s [PASS]\n");
    return 0;
  }

  int AddCgptTest(CgptManager& cgpt_manager) {
    printf("CgptManager->AddPartition for data partition \n");
    int result = cgpt_manager.AddPartition(
                                 "data stuff",
                                 guid_linux_data,
                                 guid_unused,
                                 100,
                                 20);
    if (result != kCgptSuccess) {
      printf("Failed to add data partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 1);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for kernel partition \n");
    result = cgpt_manager.AddPartition(
                                 "kernel stuff",
                                 guid_chromeos_kernel,
                                 p2guid,
                                 200,
                                 30);
    if (result != kCgptSuccess) {
      printf("Failed to add kernel partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 2);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for rootfs partition \n");
    result = cgpt_manager.AddPartition(
                                 "rootfs stuff",
                                 guid_chromeos_rootfs,
                                 p3guid,
                                 300,
                                 40);
    if (result != kCgptSuccess) {
      printf("Failed to add rootfs partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 3);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for ESP partition \n");
    result = cgpt_manager.AddPartition(
                                 "ESP stuff",
                                 guid_efi,
                                 guid_unused,
                                 400,
                                 50);
    if (result != kCgptSuccess) {
      printf("Failed to add ESP partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 4);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("CgptManager->AddPartition for Future partition \n");
    result = cgpt_manager.AddPartition(
                                 "future stuff",
                                 guid_chromeos_reserved,
                                 guid_unused,
                                 500,
                                 60);
    if (result != kCgptSuccess) {
      printf("Failed to add future partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 5);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    Guid guid_random =
    {{{0x2364a860,0xbf63,0x42fb,0xa8,0x3d,{0x9a,0xd3,0xe0,0x57,0xfc,0xf5}}}};

    printf("CgptManager->AddPartition for random partition \n");
    result = cgpt_manager.AddPartition(
                                 "random stuff",
                                 guid_random,
                                 guid_unused,
                                 600,
                                 70);

    if (result != kCgptSuccess) {
      printf("Failed to add random partition  [FAIL]\n");
      return result;
    }

    result = CheckNumPartitions(cgpt_manager, 6);
    if (result != kCgptSuccess) {
      printf("CheckNumPartitions failed after adding. [FAIL]\n");
      return result;
    }

    printf("AddCgpt test successful. [PASS]\n");

    return 0;
  }


  int SetPmbrTest(CgptManager& cgpt_manager) {
    printf("CgptManager::SetPmbr\n");

    string boot_file_name = "/tmp/BootFileForCgptManagerTests.bin";
    printf("Creating bootfile %s\n", boot_file_name.c_str());

    int result = CreateBootFile(boot_file_name);
    if (result != 0) {
      printf("Unable to create bootfile: %s [FAIL]\n", boot_file_name.c_str());
      return result;
    }

    uint32_t expected_boot_partition_number = 2;
    result = cgpt_manager.SetPmbr(expected_boot_partition_number,
                                  boot_file_name,
                                  true);
    if (result != kCgptSuccess) {
      printf("Failed to set pmbr [FAIL]\n");
      return result;
    }

    printf("Successfully set pmbr. [PASS]\n");

    // get the pmbr boot partition number and check.
    uint32_t actual_boot_partition_number;
    result = cgpt_manager.GetPmbrBootPartitionNumber(actual_boot_partition_number);
    if (result != kCgptSuccess) {
      printf("Failed to get pmbr partition number. [FAIL]\n");
      return result;
    }

    printf("Boot Partition: Expected = %d, Actual = %d\n",
           expected_boot_partition_number,
           actual_boot_partition_number);

    if (actual_boot_partition_number != expected_boot_partition_number) {
      printf("Boot partition number not set as expected. [FAIL]\n");
      return result;
    }

    printf("Pmbr test successful. [PASS]\n");

    return 0;
  }

  int SetSuccessfulAttributeTest(CgptManager& cgpt_manager) {
    printf("CgptManager::SetSuccessfulAttributeTest\n");
    bool is_successful = true;
    int partition_number = 2;
    int result = cgpt_manager.SetSuccessful(partition_number, is_successful);
    if (result != kCgptSuccess) {
      printf("Failed to Set Successful attribute. [FAIL]\n");
      return result;
    }

    printf("Successfully set Successful attribute once. [PASS]\n");

    result = CheckSuccessfulAttribute(cgpt_manager,
                                      partition_number,
                                      is_successful);
    if (result != kCgptSuccess)
      return result;

    is_successful = false;
    partition_number = 2;
    result = cgpt_manager.SetSuccessful(partition_number, is_successful);
    if (result != kCgptSuccess) {
      printf("Failed to Set Successful attribute. [FAIL]\n");
      return result;
    }

    printf("Successfully set Successful attribute again. [PASS]\n");

    result = CheckSuccessfulAttribute(cgpt_manager,
                                      partition_number,
                                      is_successful);

    if (result != kCgptSuccess)
      return result;

    printf("Successful attribute test successful. [PASS]\n");

    return 0;
  }

  int SetNumTriesLeftTest(CgptManager& cgpt_manager) {
    printf("CgptManager::SetNumTriesTest\n");
    int num_tries_left = 6;
    int partition_number = 2;
    int result = cgpt_manager.SetNumTriesLeft(partition_number, num_tries_left);
    if (result != kCgptSuccess) {
      printf("Failed to Set NumTries. [FAIL]\n");
      return result;
    }

    printf("NumTries set. [PASS]\n");

    result = CheckNumTriesLeft(cgpt_manager,
                               partition_number,
                               num_tries_left);
    if (result != kCgptSuccess)
      return result;

    // change it and try again if the change is reflected.

    num_tries_left = 5;
    partition_number = 2;
    result = cgpt_manager.SetNumTriesLeft(partition_number, num_tries_left);
    if (result != kCgptSuccess) {
      printf("Failed to Set NumTries. [FAIL]\n");
      return result;
    }

    printf("NumTries set again. [PASS]\n");

    result = CheckNumTriesLeft(cgpt_manager,
                               partition_number,
                               num_tries_left);

    if (result != kCgptSuccess)
      return result;

    printf("NumTries test successful. [PASS]\n");

    return 0;
  }


  int SetPriorityTest(CgptManager& cgpt_manager) {
    printf("CgptManager::SetPriorityTest\n");
    uint8_t priority = 8;
    int partition_number = 2;
    int result = cgpt_manager.SetPriority(partition_number, priority);
    if (result != kCgptSuccess) {
      printf("Failed to Set Priority. [FAIL]\n");
      return result;
    }

    printf("Priority set once. [PASS]\n");

    result = CheckPriority(cgpt_manager,
                           partition_number,
                           priority);
    if (result != kCgptSuccess)
      return result;

    // change it and try again if the change is reflected.

    priority = 4;
    partition_number = 2;
    result = cgpt_manager.SetPriority(partition_number, priority);
    if (result != kCgptSuccess) {
      printf("Failed to Set Priority. [FAIL]\n");
      return result;
    }

    printf("Priority set again. [PASS]\n");

    result = CheckPriority(cgpt_manager,
                           partition_number,
                           priority);

    if (result != kCgptSuccess)
      return result;

    printf("Priority test successful. [PASS]\n");
    return 0;
  }

  int GetBeginningOffsetTest(CgptManager& cgpt_manager) {
    printf("CgptManager::GetBeginningOffsetTest\n");
    int partition_number = 2;
    uint64_t expected_offset = 200; // value comes from AddCgptTest.
    int result = CheckOffset(cgpt_manager,
                             partition_number,
                             expected_offset);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetBeginningOffset test successful. [PASS]\n");
    return 0;
  }

  int GetNumSectorsTest(CgptManager& cgpt_manager) {
    printf("CgptManager::GetNumSectorsTest\n");
    int partition_number = 2;
    uint64_t expected_num_sectors = 30; // value comes from AddCgptTest.
    int result = CheckNumSectors(cgpt_manager,
                                 partition_number,
                                 expected_num_sectors);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetNumSectors test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionTypeIdTest(CgptManager& cgpt_manager) {
    printf("CgptManager::GetPartitionTypeIdTest\n");
    int partition_number = 2;
    Guid expected_type_id = guid_chromeos_kernel;
    int result = CheckPartitionTypeId(cgpt_manager,
                                      partition_number,
                                      expected_type_id);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionTypeId test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionUniqueIdTest(CgptManager& cgpt_manager) {
    printf("CgptManager::GetPartitionTypeIdTest\n");
    int partition_number = 2;
    Guid expected_unique_id = p2guid;
    int result = CheckPartitionUniqueId(cgpt_manager,
                                        partition_number,
                                        expected_unique_id);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionUniqueId test successful. [PASS]\n");
    return 0;
  }

  int GetPartitionNumberByUniqueIdTest(CgptManager& cgpt_manager) {
    printf("CgptManager::GetPartitionNumberByUniqueIdTest\n");
    Guid unique_id = p3guid;
    int expected_partition_number = 3;
    int result = CheckPartitionNumberByUniqueId(cgpt_manager,
                                                unique_id,
                                                expected_partition_number);
    if (result != kCgptSuccess) {
      return result;
    }

    printf("GetPartitionNumberByUniqueId test successful. [PASS]\n");
    return 0;
  }


  int PrioritizeCgptTest(CgptManager& cgpt_manager) {
    printf("CgptManager::PrioritizeCgpt\n");

    int result = cgpt_manager.ClearAll();
    if (result != kCgptSuccess) {
      return result;
    }

    printf("CgptManager->AddPartition for kernel 1 partition \n");
    int k1_partition_number = 1;
    result = cgpt_manager.AddPartition("k1",
                                       guid_chromeos_kernel,
                                       guid_unused,
                                       100,
                                       10);
    if (result != kCgptSuccess) {
      printf("Failed to add k1 partition  [FAIL]\n");
      return result;
    }

    int k2_partition_number = 2;
    printf("CgptManager->AddPartition for kernel 2 partition \n");
    result = cgpt_manager.AddPartition("k2",
                                       guid_chromeos_kernel,
                                       p2guid,
                                       200,
                                       20);
    if (result != kCgptSuccess) {
      printf("Failed to add k2partition  [FAIL]\n");
      return result;
    }


    int k3_partition_number = 3;
    printf("CgptManager->AddPartition for kernel 3 partition \n");
    result = cgpt_manager.AddPartition("k3",
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
    result = cgpt_manager.SetHighestPriority(k1_partition_number,
                                             k1Priority);
    if (result != kCgptSuccess) {
      printf("Failed to SetHighestPriority [FAIL]\n");
      return result;
    }

    printf("Successfully set SetHighestPriority. [PASS]\n");

    result = CheckPriority(cgpt_manager, k1_partition_number, k1Priority);
    if (result != kCgptSuccess)
      return result;

    result = CheckPriority(cgpt_manager, k2_partition_number, k2Priority);
    if (result != kCgptSuccess)
      return result;

    result = CheckPriority(cgpt_manager, k3_partition_number, k3Priority);
    if (result != kCgptSuccess)
      return result;

    printf("SetHighestPriority test successful. [PASS]\n");
    return 0;
  }

  int InitDummyDevice(const string& dummy_device) {
    ofstream dd_stream(dummy_device.c_str());

    const int kNumSectors = 1000;
    const int kSectorSize = 512;
    const char kFillChar = '7';
    for(int i = 0; i < kNumSectors * kSectorSize; i++) {
      // fill the file with some character.
      dd_stream << kFillChar;
    }

    dd_stream.flush();
    dd_stream.close();

    return 0;
  }

  int CreateBootFile(const string& bootFileName) {
    ofstream boot_stream(bootFileName.c_str());

    const int kNumSectors = 1;
    const int kSectorSize = 512;
    const char kFillChar = '8';
    for(int i = 0; i < kNumSectors * kSectorSize; i++) {
      // fill the file with some character.
      boot_stream << kFillChar;
    }

    boot_stream.flush();
    boot_stream.close();

    return 0;
  }

  int CheckNumPartitions(const CgptManager& cgpt_manager,
                               uint8_t expected_num_partitions) {
    uint8_t actual_num_partitions;
    int result = cgpt_manager.GetNumNonEmptyPartitions(actual_num_partitions);
    if (result != kCgptSuccess) {
      printf("Failed to get partition size. result = %d [FAIL]\n", result);
      return -1;
    }

    printf("NumPartitions: Expected = %d, Actual = %d\n",
           expected_num_partitions,
           actual_num_partitions);

    if (expected_num_partitions != actual_num_partitions) {
      printf("Actual number of partitions doesn't match expected number."
             "[FAIL]\n");
      return -1;
    }

    return kCgptSuccess;
  }



  int CheckSuccessfulAttribute(const CgptManager& cgpt_manager,
                               int partition_number,
                               bool expected_is_successful) {
    // get the Successful attribute and check.
    bool is_successful;
    int result = cgpt_manager.GetSuccessful(partition_number, is_successful);
    if (result != kCgptSuccess) {
      printf("Failed to get Successful attr for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    printf("Successful attr for partition: %d: Expected = %d, Actual = %d\n",
           partition_number,
           expected_is_successful,
           is_successful);

    if (is_successful != expected_is_successful) {
      printf("Successful attr for partition %d not set as expected. [FAIL]\n",
             partition_number);
      return -1;
    }

    return kCgptSuccess;
  }



  int CheckNumTriesLeft(const CgptManager& cgpt_manager,
                          int partition_number,
                          int expected_num_tries_left) {
    // get the numTries attribute and check.
    int num_tries_left;
    int result = cgpt_manager.GetNumTriesLeft(partition_number,
                                              num_tries_left);
    if (result != kCgptSuccess) {
      printf("Failed to get numTries for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    printf("numTries for partition: %d: Expected = %d, Actual = %d\n",
           partition_number,
           expected_num_tries_left,
           num_tries_left);

    if (num_tries_left != expected_num_tries_left) {
      printf("numTries for partition %d not set as expected. [FAIL]\n",
             partition_number);
      return -1;
    }

    return kCgptSuccess;
  }

  int CheckPriority(CgptManager& cgpt_manager,
                    int partition_number,
                    uint8_t expectedPriority) {
    // get the priority and check.
    uint8_t actual_priority;
    int result = cgpt_manager.GetPriority(partition_number, actual_priority);
    if (result != kCgptSuccess) {
      printf("Failed to get priority for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    printf("Priority for partition: %d: Expected = %d, Actual = %d\n",
           partition_number,
           expectedPriority,
           actual_priority);

    if (actual_priority != expectedPriority) {
      printf("Priority for partition %d not set as expected. [FAIL]\n",
             partition_number);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckOffset(CgptManager& cgpt_manager,
                  int partition_number,
                  uint64_t expected_offset) {
    // get the offset and check.
    uint64_t actual_offset;
    int result = cgpt_manager.GetBeginningOffset(partition_number,
                                                 actual_offset);
    if (result != kCgptSuccess) {
      printf("Failed to get offset for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    printf("Offset for partition: %d: Expected = %d, Actual = %d\n",
           partition_number,
           expected_offset,
           actual_offset);

    if (actual_offset != expected_offset) {
      printf("Offset for partition %d not set as expected. [FAIL]\n",
             partition_number);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckNumSectors(CgptManager& cgpt_manager,
                      int partition_number,
                      uint64_t expectedNumSectors) {
    // get the numSectors and check.
    uint64_t actual_num_sectors;
    int result = cgpt_manager.GetNumSectors(partition_number,
                                            actual_num_sectors);
    if (result != kCgptSuccess) {
      printf("Failed to get numSectors for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    printf("NumSectors for partition: %d: Expected = %d, Actual = %d\n",
           partition_number,
           expectedNumSectors,
           actual_num_sectors);

    if (actual_num_sectors != expectedNumSectors) {
      printf("NumSectors for partition %d not set as expected. [FAIL]\n",
             partition_number);
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckPartitionTypeId(CgptManager& cgpt_manager,
                           int partition_number,
                           const Guid& expected_partition_type_id) {
    // get the partition type id and check.
    Guid actual_partition_type_id;
    int result = cgpt_manager.GetPartitionTypeId(partition_number,
                                                actual_partition_type_id);
    if (result != kCgptSuccess) {
      printf("Failed to get partition type id for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    result = CheckGuidMatch("PartitionTypeId",
                            expected_partition_type_id,
                            actual_partition_type_id);
    if (result != kCgptSuccess) {
      return result;
    }

    return kCgptSuccess;
  }


  int CheckPartitionUniqueId(CgptManager& cgpt_manager,
                             int partition_number,
                             const Guid& expected_partition_unique_id) {
    // get the partition unique id and check.
    Guid actual_partition_unique_id;
    int result = cgpt_manager.GetPartitionUniqueId(
                                partition_number,
                                actual_partition_unique_id);
    if (result != kCgptSuccess) {
      printf("Failed to get partition unique id for partition: %d. [FAIL]\n",
              partition_number);
      return result;
    }

    result = CheckGuidMatch("PartitionUniqueId",
                            expected_partition_unique_id,
                            actual_partition_unique_id);
    if (result != kCgptSuccess) {
      return result;
    }

    return kCgptSuccess;
  }

  int CheckPartitionNumberByUniqueId(CgptManager& cgpt_manager,
                                     const Guid& uniqueId,
                                     uint32_t expected_partition_number) {
    // get the partition unique id and check.
    uint32_t actual_partition_number;
    int result = cgpt_manager.GetPartitionNumberByUniqueId(
                                uniqueId,
                                actual_partition_number);
    if (result != kCgptSuccess) {
      printf("Failed to get partition number. [FAIL]\n");
      return result;
    }

    printf("PartitionNumber: Expected = %d, Actual = %d\n",
           expected_partition_number,
           actual_partition_number);

    if (actual_partition_number != expected_partition_number) {
      printf("PartitionNumber not same as expected. [FAIL]\n");
      return -1;
    }

    return kCgptSuccess;
  }


  int CheckGuidMatch(string message,
                     const Guid& expected_id,
                     const Guid& actual_id) {
    char expected_id_str[GUID_STRLEN];
    GuidToStr(&expected_id, expected_id_str, sizeof expected_id_str);

    char actual_id_str[GUID_STRLEN];
    GuidToStr(&actual_id, actual_id_str, sizeof actual_id_str);

    printf("%s: Expected = %s, Actual = %s\n",
           message.c_str(),
           expected_id_str,
           actual_id_str);

    if (!GuidEqual(&expected_id,& actual_id)) {
      printf("%s: Guids do not match as expected. [FAIL]\n",
             message.c_str());
      return -1;
    }

    return kCgptSuccess;
  }

};

int main(int argc, char* argv[]) {
    CgptManagerTests cgpt_manager_tests;

    printf("Running CgptManagerTests...\n");

    int retval = cgpt_manager_tests.Run();

    printf("Finished CgptManagerTests with exit code = %d\n", retval);
    return retval;
}
