/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "cgpt.h"
#include "fmap.h"

// TODO(namnguyen): Replace with actual area name, e.g. RW_GPT.
static const char FMAP_GPT_SECTION[] = "RW_UNUSED";

off_t FileSeek(struct drive* drive, off_t offset, int whence) {
  return lseek(drive->fd, offset, whence);
}

ssize_t FileRead(struct drive* drive, void* buf, size_t count) {
  return read(drive->fd, buf, count);
}

ssize_t FileWrite(struct drive* drive, const void* buf, size_t count) {
  return write(drive->fd, buf, count);
}

int FileSync(struct drive* drive) {
  return fsync(drive->fd);
}

int FileClose(struct drive* drive) {
  return close(drive->fd);
}

#define CreateTempDirOrAbort(varname) \
  char varname[24]; \
  strcpy(varname, "/tmp/cgptXXXXXX"); \
  if (mkdtemp(varname) == NULL) { \
    Error("Cannot create temp directory for flashrom work.\n"); \
    return -1; \
  }

int FlashInit(struct drive* drive) {
  CreateTempDirOrAbort(tempdir);

  char cmd[256];
  int return_code = -1;
  char fmap_name[28];
  sprintf(fmap_name, "%s/fmap", tempdir);
  sprintf(cmd, "/usr/sbin/flashrom -p host -i FMAP:%s -r > /dev/null 2>&1",
          fmap_name);
  if (system(cmd) != 0) {
    Error("Cannot dump FMAP section from flash.\n");
    goto cleanup;
  };

  int fmap_fd = open(fmap_name, O_RDONLY);
  if (fmap_fd < 0) {
    Error("Cannot open %s.\n", fmap_name);
    goto cleanup;
  }
  // Allocate 4096 bytes. ChromeOS FMAP is usually 2048 bytes.
  uint8_t fmap[4096];
  int fmap_size = read(fmap_fd, fmap, sizeof(fmap));
  if (fmap_size < 0) {
    Error("Cannot read from %s.\n", fmap_name);
    goto cleanup2;
  }

  FmapAreaHeader* gpt_area;
  if (fmap_find_by_name(fmap, fmap_size, NULL,
                        FMAP_GPT_SECTION, &gpt_area) == NULL) {
    Error("Cannot find GPT section in the FMAP.\n");
    goto cleanup;
  }

  drive->flash_start = gpt_area->area_offset;
  drive->flash_size = gpt_area->area_size;
  drive->current_position = 0;

  return_code = 0;

cleanup2:
  close(fmap_fd);
cleanup:
  sprintf(cmd, "/bin/rm -rf %s", tempdir);
  system(cmd);
  return return_code;
}

off_t FlashSeek(struct drive* drive, off_t offset, int whence) {
  off_t new_position;
  switch (whence) {
    case SEEK_SET:
      new_position = offset;
      break;
    case SEEK_CUR:
      new_position = drive->current_position + offset;
      break;
    case SEEK_END:
      new_position = drive->size + offset;
      break;
    default:
      errno = EINVAL;
      return -1;
  }
  if (new_position < 0 || new_position > drive->size) {
    errno = EINVAL;
    return -1;
  }
  drive->current_position = new_position;
  return new_position;
}

// Translate |position| to an address in flash.
// The idea is:
//   1. The total flash size is divided into two, one for primary, one for
//      secondary/alternate structures.
//   2. These two halves take up the beginning and the end of the "drive".
// So if the address falls into the beginning of |drive|, it is mapped to the
// beginning of the flash area (flash_start).
// If it falls into the end of |drive|, if must be mapped back to the second
// half of the flash area.
// This function returns 0 for success.
static int TranslateToFlash(struct drive* drive, off_t position, size_t count,
                            off_t* translated) {
  size_t half_size = drive->flash_size / 2;
  if (0 <= position && position < half_size) {
    // The first half.
    if (position + count > half_size || position + count < position) {
      return -1;
    }
    *translated = drive->flash_start + position;
    return 0;
  } else if (position >= drive->size - half_size && position < drive->size) {
    // The second half.
    if (position + count > drive->size || position + count < position) {
      return -1;
    }
    off_t offset_to_end = drive->size - position;
    *translated = (drive->flash_start + drive->flash_size) - offset_to_end;
    return 0;
  }
  return -1;
}

static int CreateLayout(char* file_name, off_t position, size_t count) {
  int fd = mkstemp(file_name);
  if (fd < 0) {
    Error("Cannot create layout file.\n");
    return -1;
  }
  char buf[128];
  sprintf(buf, "%08X:%08X landmark\n", (unsigned int) position,
          (unsigned int) (position + count - 1));
  int layout_len = strlen(buf);
  int nr_written = write(fd, buf, layout_len);
  close(fd);
  if (nr_written != layout_len) {
    Error("Cannot write out layout for flashrom.\n");
    return -1;
  }

  return 0;
}

ssize_t FlashRead(struct drive* drive, void* buf, size_t count) {
  off_t offset;
  if (TranslateToFlash(drive, drive->current_position, count, &offset) != 0) {
    Error("Cannot translate disk address to SPI address.\n");
    errno = EINVAL;
    return -1;
  }

  CreateTempDirOrAbort(tempdir);

  int return_value = -1;
  char layout_file[40];
  sprintf(layout_file, "%s/layoutXXXXXX", tempdir);
  if (CreateLayout(layout_file, offset, count) != 0) {
    Error("Cannot create layout file for flashrom.\n");
    goto cleanup;
  }

  char content_file[40];
  sprintf(content_file, "%s/contentXXXXXX", tempdir);
  int fd = mkstemp(content_file);
  if (fd < 0) {
    goto cleanup;
  }

  char cmd[256];
  sprintf(cmd, "/usr/sbin/flashrom -p host -l %s -i landmark:%s -r "
          "> /dev/null 2>&1", layout_file, content_file);
  if (system(cmd) != 0) {
    Error("Cannot read from SPI flash.\n");
    goto cleanup2;
  }

  return_value = read(fd, buf, count);
  if (return_value != count) {
    Error("Cannot read from retrieved content file.\n");
    return_value = -1;
  } else {
    drive->current_position += return_value;
  }

cleanup2:
  close(fd);
cleanup:
  sprintf(cmd, "/bin/rm -rf %s", tempdir);
  system(cmd);
  return return_value;
}

ssize_t FlashWrite(struct drive* drive, const void* buf, size_t count) {
  off_t offset;
  if (TranslateToFlash(drive, drive->current_position, count, &offset) != 0) {
    Error("Cannot translate disk address to SPI address.\n");
    errno = EINVAL;
    return -1;
  }

  CreateTempDirOrAbort(tempdir);

  int return_value = -1;
  char layout_file[40];
  sprintf(layout_file, "%s/layoutXXXXXX", tempdir);
  if (CreateLayout(layout_file, offset, count) != 0) {
    Error("Cannot create layout file for flashrom.\n");
    goto cleanup;
  }

  char content_file[40];
  sprintf(content_file, "%s/contentXXXXXX", tempdir);
  int fd = mkstemp(content_file);
  if (fd < 0) {
    goto cleanup;
  }

  return_value = write(fd, buf, count);
  close(fd);
  if (return_value != count) {
    Error("Cannot prepare content file for flashrom.\n");
    return_value = -1;
    goto cleanup;
  }

  char cmd[256];
  // TODO(namnguyen): Add --fast-verify after 428475 is fixed
  sprintf(cmd, "/usr/sbin/flashrom -p host -l %s -i landmark:%s "
          "-w > /dev/null 2>&1", layout_file, content_file);
  if (system(cmd) != 0) {
    Error("Cannot write to SPI flash.\n");
    return_value = -1;
  } else {
    drive->current_position += return_value;
  }

cleanup:
  sprintf(cmd, "/bin/rm -rf %s", tempdir);
  system(cmd);
  return return_value;
}

int FlashSync(struct drive* drive) {
  return 0;
}

int FlashClose(struct drive* drive) {
  return 0;
}
