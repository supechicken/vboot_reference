/* Copyright 2014 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This utility wraps around "cgpt" execution to work with NAND. If the target
 * device is an MTD device, this utility will read the GPT structures from
 * FMAP, invokes "cgpt" on that, and writes the result back to NOR flash. */

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
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

static char* const CGPT_PATH = "/usr/bin/cgpt.bin";
static char* const FLASHROM_PATH = "/usr/sbin/flashrom";

// Check if cmdline |argv| has "-D". "-D" signifies that GPT structs are stored
// off device, and hence we should not wrap around cgpt.
static bool has_dash_D(int argc, char* argv[]) {
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

  if (major(stat.st_rdev) != 90) {
    return false;
  }

  return true;
}

// Return the element in |argv| that is an MTD device.
static char* find_mtd_device(int argc, char* argv[]) {
  int i;
  for (i = 2; i < argc; ++i) {
    if (is_mtd(argv[i])) {
      return argv[i];
    }
  }
  return NULL;
}

// Obtain the MTD size from its sysfs node.
static int get_mtd_size(char* mtd_device, uint64_t* size) {
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

static int fork_exec(char* cwd, char* argv[]) {
  pid_t pid = fork();
  if (pid == -1) {
    return -1;
  }
  int status = -1;
  if (pid == 0) {
    if (cwd != NULL) {
      chdir(cwd);
    }
    execv(argv[0], argv);
    // If this is reached, execv fails.
    abort();
  } else {
    wait(&status);
  }
  return status;
}

static int run_cmd(char* cwd, char* cmd, ...) {
  int argc;
  va_list ap;
  va_start(ap, cmd);
  for (argc = 1; va_arg(ap, char*) != NULL; ++argc);
  va_end(ap);

  va_start(ap, cmd);
  char** argv = calloc(argc + 1, sizeof(char*));
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

static int wrap_cgpt(int argc, char* argv[], char* mtd_device) {
  // Obtain the MTD size.
  int step = 1;
  uint64_t drive_size = 0;
  if (get_mtd_size(mtd_device, &drive_size) != 0) {
    return step;
  }

  // Create a temp dir to work in.
  step++;
  char temp_dir[] = "/tmp/cgpt_wrapper.XXXXXX";
  if (mkdtemp(temp_dir) == NULL) {
    return step;
  }

  // Read RW_GPT section from NOR flash to "rw_gpt".
  step++;
  if (run_cmd(temp_dir, FLASHROM_PATH, "-i", "RW_GPT:rw_gpt", "-r",
      NULL) != 0) {
    goto errout;
  }

  // Launch cgpt on "rw_gpt" with -D size.
  step++;
  char** my_argv = calloc(argc + 2 + 1, sizeof(char*));
  if (my_argv == NULL) {
    errno = ENOMEM;
    goto errout;
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
  if (fork_exec(temp_dir, my_argv) != 0) {
    goto errout;
  }

  // Write back "rw_gpt" to NOR flash in two chunks.
  // TODO(namnguyen): In __TWO__ chunks please!
  step++;
  if (run_cmd(temp_dir, FLASHROM_PATH, "-i", "RW_GPT:rw_gpt", "-w",
      NULL) != 0) {
    goto errout;
  }

  int ret = 0;
  goto cleanup;

errout:
  ret = step;
cleanup:
  run_cmd(temp_dir, "/bin/ls", "-l", NULL);
  run_cmd(NULL, "/bin/rm", "-rf", temp_dir, NULL);
  return ret;
}

int main(int argc, char* argv[]) {
  if (argc > 2 && !has_dash_D(argc, argv)) {
    char* mtd_device = find_mtd_device(argc, argv);
    if (mtd_device) {
      return wrap_cgpt(argc, argv, mtd_device);
    }
  }

  // Forward to cgpt as-is.
  argv[0] = CGPT_PATH;
  return execv(argv[0], argv);
}
