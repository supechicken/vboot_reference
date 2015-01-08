/* Copyright 2015 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This utility wraps around "cgpt" execution to work with NAND. If the target
 * device is an MTD device, this utility will read the GPT structures from
 * FMAP, invokes "cgpt" on that, and writes the result back to NOR flash. */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/major.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "cgpt.h"

static const char CGPT_PATH[] = "/usr/bin/cgpt.bin";
static const char FLASHROM_PATH[] = "/usr/sbin/flashrom";
static const char RM_PATH[] = "/bin/rm";

// Check if cmdline |argv| has "-D". "-D" signifies that GPT structs are stored
// off device, and hence we should not wrap around cgpt.
static bool has_dash_D(int argc, const char* const argv[]) {
  int i;
  // We go from 2, because the second arg is a cgpt command such as "create".
  for (i = 2; i < argc; ++i) {
    if (strcmp("-D", argv[i]) == 0) {
      return true;
    }
  }
  return false;
}

// Check if |device_path| is an MTD device based on its major number being 90.
static bool is_mtd(const char* device_path) {
  struct stat stat;
  if (lstat(device_path, &stat) != 0) {
    return false;
  }

  if (major(stat.st_rdev) != MTD_CHAR_MAJOR) {
    return false;
  }

  return true;
}

// Return the element in |argv| that is an MTD device.
static const char* find_mtd_device(int argc, const char* const argv[]) {
  int i;
  for (i = 2; i < argc; ++i) {
    if (is_mtd(argv[i])) {
      return argv[i];
    }
  }
  return NULL;
}

// Obtain the MTD size from its sysfs node.
static int get_mtd_size(const char* mtd_device, uint64_t* size) {
  mtd_device = strrchr(mtd_device, '/');
  if (mtd_device == NULL) {
    errno = EINVAL;
    return 1;
  }
  char* sysfs_name;
  if (asprintf(&sysfs_name, "/sys/class/mtd%s/size", mtd_device) == -1) {
    return 1;
  }
  FILE* fp = fopen(sysfs_name, "r");
  free(sysfs_name);
  if (fp == NULL) {
    return 1;
  }
  int ret = (fscanf(fp, "%" PRIu64 "\n", size) != 1);
  fclose(fp);
  return ret;
}

static int fork_exec(const char* cwd, const char* const argv[]) {
  pid_t pid = fork();
  if (pid == -1) {
    return -1;
  }
  int status = -1;
  if (pid == 0) {
    if (cwd != NULL && chdir(cwd) != 0) {
      return -1;
    }
    execv(argv[0], (char* const*)argv);
    // If this is reached, execv fails.
    err(-1, "Cannot exec %s in %s.", argv[0], cwd);
  } else {
    if (wait(&status) == -1) {
      return -1;
    }
  }
  return status;
}

static int run_cmd(const char* cwd, const char* cmd, ...) {
  int argc;
  va_list ap;
  va_start(ap, cmd);
  for (argc = 1; va_arg(ap, char*) != NULL; ++argc);
  va_end(ap);

  va_start(ap, cmd);
  const char** argv = calloc(argc + 1, sizeof(char*));
  if (argv == NULL) {
    errno = ENOMEM;
    return -1;
  }
  argv[0] = cmd;
  int i;
  for (i = 1; i < argc; ++i) {
    argv[i] = va_arg(ap, char*);
  }
  va_end(ap);

  int ret = fork_exec(cwd, argv);
  free(argv);
  return ret;
}

static int read_write(int source_fd,
                      uint64_t size,
                      const char* src_name,
                      int idx) {
  int ret = 1;
  char* buf = malloc(size);
  if (buf == NULL) {
    goto clean_exit;
  }

  ret++;
  char* dest;
  if (asprintf(&dest, "%s_%d", src_name, idx) == -1) {
    goto free_buf;
  }

  ret++;
  int dest_fd = open(dest, O_WRONLY | O_CLOEXEC);
  if (dest_fd < 0) {
    goto free_dest;
  }

  ret++;
  if (read(source_fd, buf, size) != size) {
    goto close_dest_fd;
  }
  if (write(dest_fd, buf, size) != size) {
    goto close_dest_fd;
  }

  ret = 0;

close_dest_fd:
  close(dest_fd);
free_dest:
  free(dest);
free_buf:
  free(buf);
clean_exit:
  return ret;
}

static int split_gpt(const char* dir_name, const char* file_name) {
  int ret = 1;
  char* source;
  if (asprintf(&source, "%s/%s", dir_name, file_name) == -1) {
    goto clean_exit;
  }

  ret++;
  int fd = open(source, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    goto free_source;
  }

  ret++;
  struct stat stat;
  if (fstat(fd, &stat) != 0 || (stat.st_size & 1) != 0) {
    goto close_fd;
  }
  uint64_t half_size = stat.st_size / 2;

  ret++;
  if (read_write(fd, half_size, source, 1) != 0 ||
      read_write(fd, half_size, source, 2) != 0) {
    goto close_fd;
  }

  ret = 0;
close_fd:
  close(fd);
free_source:
  free(source);
clean_exit:
  return ret;
}

static int wrap_cgpt(int argc,
                     const char* const argv[],
                     const char* mtd_device) {
  // Obtain the MTD size.
  int ret = 1;
  uint64_t drive_size = 0;
  if (get_mtd_size(mtd_device, &drive_size) != 0) {
    Error("Cannot get the size of %s.\n", mtd_device);
    return ret;
  }

  // Create a temp dir to work in.
  ret++;
  char temp_dir[] = "/tmp/cgpt_wrapper.XXXXXX";
  if (mkdtemp(temp_dir) == NULL) {
    Error("Cannot create a temporary directory.\n");
    return ret;
  }

  // Read RW_GPT section from NOR flash to "rw_gpt".
  ret++;
  if (run_cmd(temp_dir, FLASHROM_PATH, "-i", "RW_GPT:rw_gpt", "-r",
      NULL) != 0) {
    Error("Cannot exec flashrom to read from RW_GPT section.\n");
    goto cleanup;
  }

  // Launch cgpt on "rw_gpt" with -D size.
  ret++;
  const char** my_argv = calloc(argc + 2 + 1, sizeof(char*));
  if (my_argv == NULL) {
    errno = ENOMEM;
    goto cleanup;
  }
  memcpy(my_argv, argv, sizeof(char*) * argc);
  my_argv[0] = CGPT_PATH;
  int i;
  for (i = 2; i < argc; ++i) {
    if (strcmp(my_argv[i], mtd_device) == 0) {
      my_argv[i] = "rw_gpt";
    }
  }
  my_argv[argc] = "-D";
  char size[32];
  snprintf(size, sizeof(size), "%" PRIu64, drive_size);
  my_argv[argc + 1] = size;
  i = fork_exec(temp_dir, my_argv);
  free(my_argv);
  if (i != 0) {
    Error("Cannot exec cgpt to modify rw_gpt.\n");
    goto cleanup;
  }

  // Write back "rw_gpt" to NOR flash in two chunks.
  ret++;
  if (split_gpt(temp_dir, "rw_gpt") != 0) {
    Error("Cannot split rw_gpt in two.\n");
    goto cleanup;
  }
  if (run_cmd(temp_dir, FLASHROM_PATH, "-i", "RW_GPT_PRIMARY:rw_gpt_1",
              "-w", NULL) == 0 ||
      run_cmd(temp_dir, FLASHROM_PATH, "-i", "RW_GPT_SECONDARY:rw_gpt_2",
	      "-w", NULL) == 0) {
    // Succeed if either part is written.
    ret = 0;
  } else {
    Error("Cannot write rw_gpt back with flashrom.\n");
  }

cleanup:
  run_cmd(NULL, RM_PATH, "-rf", temp_dir, NULL);
  return ret;
}

int main(int argc, const char* argv[]) {
  if (argc > 2 && !has_dash_D(argc, argv)) {
    const char* mtd_device = find_mtd_device(argc, argv);
    if (mtd_device) {
      return wrap_cgpt(argc, argv, mtd_device);
    }
  }

  // Forward to cgpt as-is.
  argv[0] = CGPT_PATH;
  return execv(argv[0], (char* const*)argv);
}
