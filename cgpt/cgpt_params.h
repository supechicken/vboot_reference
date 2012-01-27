// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VBOOT_REFERENCE_CGPT_CGPT_PARAMS_H_
#define VBOOT_REFERENCE_CGPT_CGPT_PARAMS_H_

#include "cgpt.h"

// this file defines the internal methods that use the user-mode cgpt programatically.
// this is the interface for the callers such as the cgpt tool or the C++ post installer executable.

typedef struct cgpt_create_params
{
  char *driveName;
  int zap;
} cgpt_create_params;


typedef struct cgpt_add_params
{
  char *driveName;
  uint32_t partition;
  uint64_t begin;
  uint64_t size;
  Guid type_guid;
  Guid unique_guid;
  char *label;
  int successful;
  int tries;
  int priority;
  uint16_t raw_value;
  int set_begin;
  int set_size;
  int set_type;
  int set_unique;
  int set_successful;
  int set_tries;
  int set_priority;
  int set_raw;
} cgpt_add_params;


typedef struct cgpt_show_params
{
  char *driveName;
  int numeric;
  int verbose;
  int quick;
  uint32_t partition;
  int single_item;
} cgpt_show_params;

typedef struct cgpt_repair_params
{
  char *driveName;
  int verbose;
} cgpt_repair_params;


typedef struct cgpt_boot_params
{
  char *driveName;
  uint32_t partition;
  char *bootfile;
  int create_pmbr;
} cgpt_boot_params;

typedef struct cgpt_prioritize_params
{
  char *driveName;

  uint32_t set_partition;
  int set_friends;
  int max_priority;
  int orig_priority;
} cgpt_prioritize_params;


typedef struct cgpt_find_params
{
  char *driveName;

  int verbose;
  int set_unique;
  int set_type;
  int set_label;
  int oneonly;
  int numeric;
  uint8_t *matchbuf;
  uint64_t matchlen;
  uint64_t matchoffset;
  uint8_t *comparebuf;
  Guid unique_guid;
  Guid type_guid;
  char *label;
  int hits;
  int match_partnum;           // 0 for no match, 1-N for match
} cgpt_find_params;


int cgpt_create(cgpt_create_params *params);
int cgpt_add(cgpt_add_params *params);
int cgpt_boot(cgpt_boot_params *params);
int cgpt_show(cgpt_show_params *params);
int cgpt_repair(cgpt_repair_params *params);
int cgpt_prioritize(cgpt_prioritize_params *params);
void cgpt_find(cgpt_find_params *params);



#endif  // VBOOT_REFERENCE_CGPT_CGPT_PARAMS_H_
