// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for CgptManager class.
#include <string>
#include <fstream>
#include <iostream>

#include "../cgpt/CgptManager.h"

// We use some specific GUID constants for some of the tests,
// so pulling in cgpt.h. Make sure this is included after
// CgptManager.h from above to ensure that the actual users
// of CgptManager shouldn't require to include cgpt.h.
extern "C" {
#include "../cgpt/cgpt.h"
}

#include <base/logging.h>
#include <gtest/gtest.h>

using std::string;
using std::ofstream;

// We don't use these parameters for the libcgpt version.
const char* progname = "";
const char* command = "";

static const Guid p2guid = {{{0, 1, 2, 3, 4, {2, 2, 2, 2, 2, 2}}}};
static const Guid p3guid = {{{0, 6, 5, 4, 2, {3, 3, 3, 3, 3, 3}}}};


// Used to expect a call c to succeed as indicated by a zero error code.
#define EXPECT_SUCCESS(c) EXPECT_EQ(0, c)

class CgptManagerUnitTest : public ::testing::Test {
public:
  CgptManagerUnitTest() { }

  void SetUp() {

    const string device_name = "/tmp/DummyFileForCgptManagerTests.bin";

    CreateDummyDevice(device_name);

    LOG(INFO) << "Initializing cgpt with " << device_name;
    EXPECT_SUCCESS(cgpt_manager.Initialize(device_name));

    LOG(INFO) << "Removing all existing partitions";
    EXPECT_SUCCESS(cgpt_manager.ClearAll());

    CheckNumPartitions(0);
  }

  virtual ~CgptManagerUnitTest() { }

protected:
  CgptManager cgpt_manager;

  void CreateDummyDevice(const string& dummy_device) {
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
  }

  void CheckEquality(string field,
                     uint64_t expected,
                     uint64_t actual) {

    LOG(INFO) << field << ":"
              << "Expected = " << expected
              << ";Actual = " << actual;

    EXPECT_EQ(expected, actual);
  }


  void CheckGuidEquality(string field,
                         const Guid& expected_id,
                         const Guid& actual_id) {
    char expected_id_str[GUID_STRLEN];
    GuidToStr(&expected_id, expected_id_str, sizeof expected_id_str);

    char actual_id_str[GUID_STRLEN];
    GuidToStr(&actual_id, actual_id_str, sizeof actual_id_str);

    LOG(INFO) << field << ":"
              << "Expected = " << expected_id_str
              << ";Actual = " << actual_id_str;

    EXPECT_TRUE(GuidEqual(&expected_id, &actual_id));
  }

  void CheckNumPartitions(uint8 expected_num_partitions) {
    uint8_t actual_num_partitions;

    EXPECT_EQ(cgpt_manager.GetNumNonEmptyPartitions(&actual_num_partitions),
             kCgptSuccess);

    CheckEquality("NumPartitions",
                  expected_num_partitions,
                  actual_num_partitions);
  }

  void SetAndCheckSuccessfulBit(uint32_t partition_number,
                                bool expected_is_successful) {
    EXPECT_SUCCESS(cgpt_manager.SetSuccessful(partition_number,
                                              expected_is_successful));

    bool actual_is_successful;
    EXPECT_SUCCESS(cgpt_manager.GetSuccessful(partition_number,
                                              &actual_is_successful));
    EXPECT_EQ(expected_is_successful, actual_is_successful);
  }


  void SetAndCheckNumTriesLeft(uint32_t partition_number,
                               int expected_num_tries) {
    EXPECT_SUCCESS(cgpt_manager.SetNumTriesLeft(partition_number,
                                                expected_num_tries));

    int actual_num_tries;
    EXPECT_SUCCESS(cgpt_manager.GetNumTriesLeft(partition_number,
                                                &actual_num_tries));
    CheckEquality("NumTries", expected_num_tries, actual_num_tries);
  }

  void SetAndCheckPriority(uint32_t partition_number,
                           uint8_t expected_priority) {
    EXPECT_SUCCESS(cgpt_manager.SetPriority(partition_number,
                                            expected_priority));

    uint8_t actual_priority;
    EXPECT_SUCCESS(cgpt_manager.GetPriority(partition_number,
                                            &actual_priority));
    CheckEquality("Priority", expected_priority, actual_priority);
  }

  void CheckPriority(uint32_t partition_number,
                     uint8_t expected_priority) {
    uint8_t actual_priority;
    EXPECT_SUCCESS(cgpt_manager.GetPriority(partition_number,
                                            &actual_priority));
    CheckEquality("Priority", expected_priority, actual_priority);
  }


  void CheckBeginningOffset(uint32_t partition_number,
                            uint64_t expected_offset) {
    uint64_t actual_offset;
    EXPECT_SUCCESS(cgpt_manager.GetBeginningOffset(partition_number,
                                                   &actual_offset));
    CheckEquality("Beginning Offset", expected_offset, actual_offset);
  }


  void CheckNumSectors(uint32_t partition_number,
                       uint64_t expected_num_sectors) {
    uint64_t actual_num_sectors;
    EXPECT_SUCCESS(cgpt_manager.GetNumSectors(partition_number,
                                              &actual_num_sectors));
    CheckEquality("Num Sectors", expected_num_sectors, actual_num_sectors);
  }


  void CheckPartitionTypeId(int partition_number,
                            const Guid& expected_partition_type_id) {
    // Get the partition type id and check.
    Guid actual_partition_type_id;
    EXPECT_SUCCESS(cgpt_manager.GetPartitionTypeId(partition_number,
                                                   &actual_partition_type_id));

    CheckGuidEquality("PartitionTypeId",
                      expected_partition_type_id,
                      actual_partition_type_id);
  }

  void CheckPartitionUniqueId(int partition_number,
                            const Guid& expected_partition_unique_id) {
    // Get the partition unique id and check.
    Guid actual_partition_unique_id;
    EXPECT_SUCCESS(cgpt_manager.GetPartitionUniqueId(
                                    partition_number,
                                    &actual_partition_unique_id));

    CheckGuidEquality("PartitionTypeId",
                      expected_partition_unique_id,
                      actual_partition_unique_id);
  }

  void CheckPartitionNumberByUniqueId(const Guid& unique_id,
                                      uint32_t expected_partition_number) {
    // Get the partition number for the unique id and check.
    uint32_t actual_partition_number;
    EXPECT_SUCCESS(cgpt_manager.GetPartitionNumberByUniqueId(
                                    unique_id,
                                    &actual_partition_number));

    CheckEquality("PartitionNumberForUniqueId",
                  expected_partition_number,
                  actual_partition_number);
  }


  int CreateBootFile(const string& bootFileName) {
    ofstream boot_stream(bootFileName.c_str());

    const int kNumSectors = 1;
    const int kSectorSize = 512;
    const char kFillChar = '8';

    for(int i = 0; i < kNumSectors * kSectorSize; i++) {
     // Fill the file with some character.
     boot_stream << kFillChar;
    }

    boot_stream.flush();
    boot_stream.close();

    return 0;
  }

private:
  DISALLOW_COPY_AND_ASSIGN(CgptManagerUnitTest);
};

TEST_F(CgptManagerUnitTest, AutoPrioritizationTest) {
  // Verify that we're able to initialize CgptManager.
  const string device_name = "/tmp/DummyFileForCgptManagerTests.bin";

  LOG(INFO) << "Initializing " << device_name << " as a dummy device";
  EXPECT_SUCCESS(cgpt_manager.Initialize(device_name));

  LOG(INFO) << "Removing all existing partitions";
  EXPECT_SUCCESS(cgpt_manager.ClearAll());

  CheckNumPartitions(0);

  LOG(INFO) << "Adding kernel1 partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("k1",
                                           guid_chromeos_kernel,
                                           guid_unused,
                                           100,
                                           10));
  CheckNumPartitions(1);

  LOG(INFO) << "Adding kernel2 partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("k2",
                                           guid_chromeos_kernel,
                                           p2guid,
                                           200,
                                           20));
  CheckNumPartitions(2);

  LOG(INFO) << "Adding kernel3 partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("k3",
                                           guid_chromeos_kernel,
                                           p3guid,
                                           300,
                                           30));
  CheckNumPartitions(3);

  uint8_t expectedk1Priority = 1;
  uint8_t expectedk2Priority = 2;
  uint8_t expectedk3Priority = 0;

  LOG(INFO) << "Setting initial priorities ";

  // Calling SetAndCheckPriority will do a set and get of the above priorities.
  SetAndCheckPriority(1, expectedk1Priority);
  SetAndCheckPriority(2, expectedk2Priority);
  SetAndCheckPriority(3, expectedk3Priority);

  LOG(INFO) << "Changing priorities by setting k1 to highest priority.";
  EXPECT_SUCCESS(cgpt_manager.SetHighestPriority(1));

  expectedk1Priority = 2; // change from 1 to 2
  expectedk2Priority = 1; // change from 2 to 1
  expectedk3Priority = 0; // remains unchanged.

  CheckPriority(1, expectedk1Priority);
  CheckPriority(2, expectedk2Priority);
  CheckPriority(3, expectedk3Priority);
}


TEST_F(CgptManagerUnitTest, AddPartitionTest) {
  // Verify that we're able to initialize CgptManager.
  const string device_name = "/tmp/DummyFileForCgptManagerTests.bin";

  LOG(INFO) << "Initializing " << device_name << " as a dummy device";
  EXPECT_SUCCESS(cgpt_manager.Initialize(device_name));

  LOG(INFO) << "Removing all existing partitions";
  EXPECT_SUCCESS(cgpt_manager.ClearAll());

  CheckNumPartitions(0);

  LOG(INFO) << "Successfully cleared all existing partitions.";

  LOG(INFO) << "Adding data partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("data stuff",
                                           guid_linux_data,
                                           guid_unused,
                                           100,
                                           10));

  CheckNumPartitions(1);

  LOG(INFO) << "Adding kernel partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("kernel stuff",
                                           guid_chromeos_kernel,
                                           p2guid,
                                           200,
                                           20));

  CheckNumPartitions(2);


  LOG(INFO) << "Adding rootfs partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("rootfs stuff",
                                           guid_chromeos_rootfs,
                                           p3guid,
                                           300,
                                           30));

  CheckNumPartitions(3);


  uint32_t pmbr_boot_partition_number = 4;
  LOG(INFO) << "Adding ESP partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("ESP stuff",
                                           guid_efi,
                                           guid_unused,
                                           400,
                                           40));

  CheckNumPartitions(4);


  LOG(INFO) << "Adding Future partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("fture stuff",
                                           guid_chromeos_reserved,
                                           guid_unused,
                                           500,
                                           50));

  CheckNumPartitions(5);

  Guid guid_random =
  {{{0x2364a860,0xbf63,0x42fb,0xa8,0x3d,{0x9a,0xd3,0xe0,0x57,0xfc,0xf5}}}};

  LOG(INFO) << "Adding random partition";
  EXPECT_SUCCESS(cgpt_manager.AddPartition("random stuff",
                                           guid_random,
                                           guid_unused,
                                           600,
                                           60));

  CheckNumPartitions(6);

  string boot_file_name = "/tmp/BootFileForCgptManagerTests.bin";

  LOG(INFO) << "Adding EFI partition to PMBR with bootfile: "
            << boot_file_name;

  EXPECT_SUCCESS(CreateBootFile(boot_file_name));
  EXPECT_SUCCESS(cgpt_manager.SetPmbr(pmbr_boot_partition_number,
                                      boot_file_name,
                                      true));

  LOG(INFO) << "Verifying PMBR. ";

  // Get the pmbr boot partition number and check.
  uint32_t actual_boot_partition_number;
  EXPECT_SUCCESS(cgpt_manager.GetPmbrBootPartitionNumber(
                                       &actual_boot_partition_number));
  EXPECT_EQ(pmbr_boot_partition_number, actual_boot_partition_number);

  LOG(INFO) << "Setting Successful attribute";
  SetAndCheckSuccessfulBit(2, true);

  LOG(INFO) << "Changing Successful attribute";
  SetAndCheckSuccessfulBit(2, false);

  LOG(INFO) << "Setting NumTriesLeft attribute";
  SetAndCheckNumTriesLeft(2, 6);

  LOG(INFO) << "Changing NumTriesLeft attribute";
  SetAndCheckNumTriesLeft(2, 5);

  LOG(INFO) << "Setting Priority attribute";
  SetAndCheckPriority(2, 2);

  LOG(INFO) << "Changing Priority attribute";
  SetAndCheckPriority(2, 0);

  LOG(INFO) << "Checking Beginning offset";
  CheckBeginningOffset(2, 200);

  LOG(INFO) << "Checking Number of sectors";
  CheckNumSectors(2, 20);

  LOG(INFO) << "Checking Partition type ID";
  CheckPartitionTypeId(2, guid_chromeos_kernel);

  LOG(INFO) << "Checking Partition unique ID";
  CheckPartitionUniqueId(2, p2guid);

  LOG(INFO) << "Checking Partition number for given unique ID";
  CheckPartitionNumberByUniqueId(p3guid, 3);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
