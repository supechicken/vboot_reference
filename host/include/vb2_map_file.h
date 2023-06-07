// Copyright 2023 The ChromiumOS Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VBOOT_REFERENCE_KERNEL_FILE_H_
#define VBOOT_REFERENCE_KERNEL_FILE_H_

#include <stdint.h>

/* Possible file operation errors */
enum file_err {
  FILE_ERR_NONE,
  FILE_ERR_STAT,
  FILE_ERR_SIZE,
  FILE_ERR_MMAP,
  FILE_ERR_MSYNC,
  FILE_ERR_MUNMAP,
  FILE_ERR_OPEN,
  FILE_ERR_CLOSE,
  FILE_ERR_DIR,
  FILE_ERR_CHR,
  FILE_ERR_FIFO,
  FILE_ERR_SOCK,
};

enum file_mode {
  FILE_RO,
  FILE_RW,
};

enum file_err open_file(const char* infile, int* fd, enum file_mode mode);
enum file_err close_file(int fd);

/* Wrapper for mmap/munmap. Skips stupidly large files. */
enum file_err map_file(int fd,
                       enum file_mode mode,
                       uint8_t** buf,
                       uint32_t* len);
enum file_err unmap_file(int fd,
                         enum file_mode mode,
                         uint8_t* buf,
                         uint32_t len);

enum file_err open_and_map_file(const char* infile,
                                int* fd,
                                enum file_mode mode,
                                uint8_t** buf,
                                uint32_t* len);
enum file_err unmap_and_close_file(int fd,
                                   enum file_mode mode,
                                   uint8_t* buf,
                                   uint32_t len);

#endif /* VBOOT_REFERENCE_KERNEL_FILE_H_ */
