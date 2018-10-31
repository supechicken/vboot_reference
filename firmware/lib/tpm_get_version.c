/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <unistd.h>

#include "2sysincludes.h"
#include "tlcl.h"

int GetTpmVersion(void) {
  const char tpm0_path[] = "/sys/class/tpm/tpm0";
  const char caps_path[] = "/sys/class/tpm/tpm0/caps";

  /* Both TPM 1.2 and 2.0 have this directory; fail if it's not present */
  if (access(tpm0_path, F_OK) == -1) {
    return 0;
  }

  /* Only TPM 1.2 exposes useful files in sysfs. The caps file is
   * chosen arbitrarily. */
  if (access(caps_path, F_OK) == 0) {
    return 1;
  }

  /* Assume TPM 2.0 otherwise */
  return 2;
}
