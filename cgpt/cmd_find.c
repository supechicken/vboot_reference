// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cgpt.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

#include "cgptlib_internal.h"
#include "cgpt_params.h"

static void Usage(void)
{
  printf("\nUsage: %s find [OPTIONS] [DRIVE]\n\n"
         "Find a partition by its UUID or label. With no specified DRIVE\n"
         "it scans all physical drives.\n\n"
         "Options:\n"
         "  -t GUID      Search for Partition Type GUID\n"
         "  -u GUID      Search for Partition Unique ID\n"
         "  -l LABEL     Search for Label\n"
         "  -v           Be verbose in displaying matches (repeatable)\n"
         "  -n           Numeric output only\n"
         "  -1           Fail if more than one match is found\n"
         "  -M FILE"
         "      Matching partition data must also contain FILE content\n"
         "  -O NUM"
         "       Byte offset into partition to match content (default 0)\n"
         "\n", progname);
  PrintTypes();
}

int cmd_find(int argc, char *argv[]) {

  cgpt_find_params params;

  params.verbose = 0;
  params.set_unique = 0;
  params.set_type = 0;
  params.set_label = 0;
  params.oneonly = 0;
  params.numeric = 0;
  params.matchbuf = NULL;
  params.matchlen = 0;
  params.matchoffset = 0;
  params.comparebuf = NULL;

  params.unique_guid;
  params.type_guid;
  params.label;
  params.hits = 0;

  params.match_partnum = 0;

  int i;
  int errorcnt = 0;
  char *e = 0;
  int c;

  opterr = 0;                     // quiet, you
  while ((c=getopt(argc, argv, ":hv1nt:u:l:M:O:")) != -1)
  {
    switch (c)
    {
    case 'v':
      params.verbose++;
      break;
    case 'n':
      params.numeric = 1;
      break;
    case '1':
      params.oneonly = 1;
      break;
    case 'l':
      params.set_label = 1;
      params.label = optarg;
      break;
    case 't':
      params.set_type = 1;
      if (CGPT_OK != SupportedType(optarg, &params.type_guid) &&
          CGPT_OK != StrToGuid(optarg, &params.type_guid)) {
        Error("invalid argument to -%c: %s\n", c, optarg);
        errorcnt++;
      }
      break;
    case 'u':
      params.set_unique = 1;
      if (CGPT_OK != StrToGuid(optarg, &params.unique_guid)) {
        Error("invalid argument to -%c: %s\n", c, optarg);
        errorcnt++;
      }
      break;
    case 'M':
      params.matchbuf = ReadFile(optarg, &params.matchlen);
      if (!params.matchbuf || !params.matchlen) {
        Error("Unable to read from %s\n", optarg);
        errorcnt++;
      }
      // Go ahead and allocate space for the comparison too
      params.comparebuf = (uint8_t *)malloc(params.matchlen);
      if (!params.comparebuf) {
        Error("Unable to allocate %" PRIu64 "bytes for comparison buffer\n",
              params.matchlen);
        errorcnt++;
      }
      break;
    case 'O':
      params.matchoffset = strtoull(optarg, &e, 0);
      if (!*optarg || (e && *e)) {
        Error("invalid argument to -%c: \"%s\"\n", c, optarg);
        errorcnt++;
      }
      break;

    case 'h':
      Usage();
      return CGPT_OK;
    case '?':
      Error("unrecognized option: -%c\n", optopt);
      errorcnt++;
      break;
    case ':':
      Error("missing argument to -%c\n", optopt);
      errorcnt++;
      break;
    default:
      errorcnt++;
      break;
    }
  }
  if (!params.set_unique && !params.set_type && !params.set_label) {
    Error("You must specify at least one of -t, -u, or -l\n");
    errorcnt++;
  }
  if (errorcnt)
  {
    Usage();
    return CGPT_FAILED;
  }

  if (optind < argc) {
    for (i=optind; i<argc; i++) {
      params.driveName = argv[i];
      cgpt_find(&params);
      }
  } else {
      cgpt_find(&params);
  }

  if (params.oneonly && params.hits != 1) {
    return CGPT_FAILED;
  }

  if (params.match_partnum) {
    return CGPT_OK;
  }

  return CGPT_FAILED;
}
