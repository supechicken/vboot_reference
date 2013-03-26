// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string.h>

#define _STUB_IMPLEMENTATION_

#include "cgpt.h"
#include "cgpt_params.h"
#include "cgptlib_internal.h"
#include "utility.h"
#include "vboot_host.h"

static const char* DumpCgptAddParams(const CgptAddParams *params) {
  static char buf[256];
  char tmp[64];

  buf[0] = 0;
  snprintf(tmp, sizeof(tmp), "-i %d ", params->partition);
  StrnAppend(buf, tmp, sizeof(buf));
  if (params->label) {
    snprintf(tmp, sizeof(tmp), "-l %s ", params->label);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_begin) {
    snprintf(tmp, sizeof(tmp), "-b %llu ", (unsigned long long)params->begin);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_size) {
    snprintf(tmp, sizeof(tmp), "-s %llu ", (unsigned long long)params->size);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_type) {
    GuidToStr(&params->type_guid, tmp, sizeof(tmp));
    StrnAppend(buf, "-t ", sizeof(buf));
    StrnAppend(buf, tmp, sizeof(buf));
    StrnAppend(buf, " ", sizeof(buf));
  }
  if (params->set_unique) {
    GuidToStr(&params->unique_guid, tmp, sizeof(tmp));
    StrnAppend(buf, "-u ", sizeof(buf));
    StrnAppend(buf, tmp, sizeof(buf));
    StrnAppend(buf, " ", sizeof(buf));
  }
  if (params->set_successful) {
    snprintf(tmp, sizeof(tmp), "-S %d ", params->successful);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_tries) {
    snprintf(tmp, sizeof(tmp), "-T %d ", params->tries);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_priority) {
    snprintf(tmp, sizeof(tmp), "-P %d ", params->priority);
    StrnAppend(buf, tmp, sizeof(buf));
  }
  if (params->set_raw) {
    snprintf(tmp, sizeof(tmp), "-A 0x%x ", params->raw_value);
    StrnAppend(buf, tmp, sizeof(buf));
  }

  StrnAppend(buf, "\n", sizeof(buf));
  return buf;
}

// This is the implementation-specific helper function.
static int GptSetEntryAttributes(struct drive *drive,
                                 uint32_t index,
                                 CgptAddParams *params) {
  GptEntry *entry;

  entry = GetEntry(&drive->gpt, PRIMARY, index);
  if (params->set_begin)
    entry->starting_lba = params->begin;
  if (params->set_size)
    entry->ending_lba = entry->starting_lba + params->size - 1;
  if (params->set_unique) {
    memcpy(&entry->unique, &params->unique_guid, sizeof(Guid));
  } else if (GuidIsZero(&entry->type)) {
    if (!uuid_generator) {
      Error("Unable to generate new GUID. uuid_generator not set.\n");
      return -1;
    }
    (*uuid_generator)((uint8_t *)&entry->unique);
  }
  if (params->set_type)
    memcpy(&entry->type, &params->type_guid, sizeof(Guid));
  if (params->label) {
    if (CGPT_OK != UTF8ToUTF16((uint8_t *)params->label, entry->name,
                               sizeof(entry->name) / sizeof(entry->name[0]))) {
      Error("The label cannot be converted to UTF16.\n");
      return -1;
    }
  }
  return 0;
}

// This is an internal helper function which assumes no NULL args are passed.
// It sets the given attribute values for a single entry at the given index.
static int SetEntryAttributes(struct drive *drive,
                              uint32_t index,
                              CgptAddParams *params) {
  if (params->set_raw) {
    SetRaw(drive, PRIMARY, index, params->raw_value);
  } else {
    if (params->set_successful)
      SetSuccessful(drive, PRIMARY, index, params->successful);
    if (params->set_tries)
      SetTries(drive, PRIMARY, index, params->tries);
    if (params->set_priority)
      SetPriority(drive, PRIMARY, index, params->priority);
  }

  // New partitions must specify type, begin, and size.
  if (IsUnused(drive, PRIMARY, index)) {
    if (!params->set_begin || !params->set_size || !params->set_type) {
      Error("-t, -b, and -s options are required for new partitions\n");
      return -1;
    }
    if (GuidIsZero(&params->type_guid)) {
      Error("New partitions must have a type other than \"unused\"\n");
      return -1;
    }
  }

  return 0;
}

static int CgptCheckAddValidity(struct drive *drive) {
  int gpt_retval;
  if (GPT_SUCCESS != (gpt_retval = GptSanityCheck(&drive->gpt))) {
    Error("GptSanityCheck() returned %d: %s\n",
          gpt_retval, GptError(gpt_retval));
    return -1;
  }

  if (((drive->gpt.valid_headers & MASK_BOTH) != MASK_BOTH) ||
      ((drive->gpt.valid_entries & MASK_BOTH) != MASK_BOTH)) {
    Error("one of the GPT header/entries is invalid.\n"
          "please run 'cgpt repair' before adding anything.\n");
    return -1;
  }
  return 0;
}

static int CgptGetUnusedPartition(struct drive *drive, uint32_t *index,
                                  CgptAddParams *params) {
  uint32_t i;
  uint32_t max_part = GetNumberOfEntries(drive);
  if (params->partition) {
    if (params->partition > max_part) {
      Error("invalid partition number: %d\n", params->partition);
      return -1;
    }
    *index = params->partition - 1;
    return 0;
  } else {
    // Find next empty partition.
    for (i = 0; i < max_part; i++) {
      if (IsUnused(drive, PRIMARY, i)) {
        params->partition = i + 1;
        *index = i;
        return 0;
      }
    }
    Error("no unused partitions available\n");
    return -1;
  }
}

int CgptAdd(CgptAddParams *params) {
  struct drive drive;

  GptEntry *entry, backup;
  uint32_t index;
  int rv;

  if (params == NULL)
    return CGPT_FAILED;

  if (CGPT_OK != DriveOpen(params->drive_name, &drive, O_RDWR))
    return CGPT_FAILED;

  if (CgptCheckAddValidity(&drive)) {
    goto bad;
  }

  if (CgptGetUnusedPartition(&drive, &index, params)) {
    goto bad;
  }

  entry = GetEntry(&drive.gpt, PRIMARY, index);
  memcpy(&backup, entry, sizeof(backup));

  if (SetEntryAttributes(&drive, index, params) ||
      GptSetEntryAttributes(&drive, index, params)) {
    memcpy(entry, &backup, sizeof(*entry));
    goto bad;
  }

  UpdateAllEntries(&drive);

  rv = CheckEntries((GptEntry*)drive.gpt.primary_entries,
                    (GptHeader*)drive.gpt.primary_header);

  if (0 != rv) {
    // If the modified entry is illegal, recover it and return error.
    memcpy(entry, &backup, sizeof(*entry));
    Error("%s\n", GptErrorText(rv));
    Error(DumpCgptAddParams(params));
    goto bad;
  }

  // Write it all out.
  return DriveClose(&drive, 1);

bad:
  DriveClose(&drive, 0);
  return CGPT_FAILED;
}
