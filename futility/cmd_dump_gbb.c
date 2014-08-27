/*
 * Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "futility.h"
#include "gbb_header.h"


static int ValidGBB(GoogleBinaryBlockHeader *gbb, size_t maxlen)
{
  int i;

  printf("0x%lx bytes left\n\n", maxlen);

  printf("signature:            0x%04lx  %c%c%c%c\n",
	 offsetof(GoogleBinaryBlockHeader, signature),
	 gbb->signature[0], gbb->signature[1],
	 gbb->signature[2], gbb->signature[3]);
  printf("major_version:        0x%04lx  %d\n",
	 offsetof(GoogleBinaryBlockHeader, major_version),
	 gbb->major_version);
  if (gbb->major_version != GBB_MAJOR_VER)
    goto bad;
  printf("minor_version:        0x%04lx  %d\n",
	 offsetof(GoogleBinaryBlockHeader, minor_version),
	 gbb->minor_version);
  printf("header_size:          0x%04lx  0x%08x (%d)\n",
	 offsetof(GoogleBinaryBlockHeader, header_size),
	 gbb->header_size, gbb->header_size);
  if (gbb->header_size != GBB_HEADER_SIZE ||
      gbb->header_size > maxlen)
    goto bad;
  printf("flags:                0x%04lx  0x%08x\n",
	 offsetof(GoogleBinaryBlockHeader, flags),
	 gbb->flags);
  printf("hwid_offset:          0x%04lx  0x%08x\n",
	 offsetof(GoogleBinaryBlockHeader, hwid_offset),
	 gbb->hwid_offset);
  printf("hwid_size:            0x%04lx  0x%08x (%d)\n",
	 offsetof(GoogleBinaryBlockHeader, hwid_size),
	 gbb->hwid_size, gbb->hwid_size);
  if (gbb->hwid_offset + gbb->hwid_size > maxlen)
    goto bad;
  printf("rootkey_offset:       0x%04lx  0x%08x\n",
	 offsetof(GoogleBinaryBlockHeader, rootkey_offset),
	 gbb->rootkey_offset);
  printf("rootkey_size:         0x%04lx  0x%08x (%d)\n",
	 offsetof(GoogleBinaryBlockHeader, rootkey_size),
	 gbb->rootkey_size, gbb->rootkey_size);
  if (gbb->rootkey_offset + gbb->rootkey_size > maxlen)
    goto bad;
  printf("bmpfv_offset:         0x%04lx  0x%08x\n",
	 offsetof(GoogleBinaryBlockHeader, bmpfv_offset),
	 gbb->bmpfv_offset);
  printf("bmpfv_size:           0x%04lx  0x%08x (%d)\n",
	 offsetof(GoogleBinaryBlockHeader, bmpfv_size),
	 gbb->bmpfv_size, gbb->bmpfv_size);
  if (gbb->bmpfv_offset + gbb->bmpfv_size > maxlen)
    goto bad;
  printf("recovery_key_offset:  0x%04lx  0x%08x\n",
	 offsetof(GoogleBinaryBlockHeader, recovery_key_offset),
	 gbb->recovery_key_offset);
  printf("recovery_key_size:    0x%04lx  0x%08x (%d)\n",
	 offsetof(GoogleBinaryBlockHeader, recovery_key_size),
	 gbb->recovery_key_size, gbb->recovery_key_size);
  if (gbb->recovery_key_offset + gbb->recovery_key_size > maxlen)
    goto bad;
  printf("pad:                  0x%04lx ",
	 offsetof(GoogleBinaryBlockHeader, pad));
  for (i = 0; i < 80;) {
    printf(" %02x", gbb->pad[i++]);
    if (!(i % 16)) {
      printf("\n");
      if (i < 80)
	printf("                             ");
    }
  }
  printf("\n");

  return 1;

bad:
  printf(" -- invalid --\n\n");
  return 0;
}


#define GBB_SEARCH_STRIDE 4
static GoogleBinaryBlockHeader *FindGbbHeader(char* ptr, size_t size)
{
  size_t i;
  GoogleBinaryBlockHeader *tmp, *gbb_header = 0;
  int count = 0;

  for (i = 0;
       i <= size - GBB_SEARCH_STRIDE; /* comment */
       i += GBB_SEARCH_STRIDE) {
    if (0 != strncmp(ptr + i, GBB_SIGNATURE, GBB_SIGNATURE_SIZE))
      continue;

    /* Found something. See if it's any good. */
    printf("hit at 0x%lx\n", i);
    tmp = (GoogleBinaryBlockHeader *)(ptr + i);
    if (ValidGBB(tmp, size - i))
      if (!count++)
	gbb_header = tmp;
  }

  switch (count) {
  case 0:
    return 0;
  case 1:
    return gbb_header;
  default:
    printf("multiple headers found\n");
    return 0;
  }
}

static int do_dump_gbb(int argc, char *argv[])
{
  char *progname;
  int c;
  int errorcnt = 0;
  struct stat sb;
  int fd;
  GoogleBinaryBlockHeader *gbb;
  void *base_ptr;
  int retval = 1;

  progname = strrchr(argv[0], '/');
  if (progname)
    progname++;
  else
    progname = argv[0];

  opterr = 0;                           /* quiet, you */
  while ((c = getopt(argc, argv, "")) != -1) {
    switch (c) {
    case '?':
      fprintf(stderr, "%s: unrecognized switch: -%c\n",
              progname, optopt);
      errorcnt++;
      break;
    case ':':
      fprintf(stderr, "%s: missing argument to -%c\n",
              progname, optopt);
      errorcnt++;
      break;
    default:
      errorcnt++;
      break;
    }
  }

  if (errorcnt || optind >= argc) {
    fprintf(stderr, "\nUsage:  %s NAME\n", progname);
    return 1;
  }

  if (0 != stat(argv[optind], &sb)) {
    fprintf(stderr, "%s: can't stat %s: %s\n",
            progname,
            argv[optind],
            strerror(errno));
    return 1;
  }

  fd = open(argv[optind], O_RDONLY);
  if (fd < 0) {
    fprintf(stderr, "%s: can't open %s: %s\n",
            progname,
            argv[optind],
            strerror(errno));
    return 1;
  }

  base_ptr = mmap(0, sb.st_size, PROT_READ|PROT_WRITE, MAP_PRIVATE, fd, 0);
  if (base_ptr == (char*)-1) {
    fprintf(stderr, "%s: can't mmap %s: %s\n",
            progname,
            argv[optind],
            strerror(errno));
    close(fd);
    return 1;
  }
  close(fd);                            /* done with this now */

  gbb = FindGbbHeader((char*) base_ptr, sb.st_size);
  if (gbb) {
    retval = 0;
  }

  if (0 != munmap(base_ptr, sb.st_size)) {
    fprintf(stderr, "%s: can't munmap %s: %s\n",
            progname,
            argv[optind],
            strerror(errno));
    return 1;
  }

  return retval;
}

DECLARE_FUTIL_COMMAND(dump_gbb, do_dump_gbb, "HEY");
