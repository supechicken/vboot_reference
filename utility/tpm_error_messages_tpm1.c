/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * TPM 1.2 specific part of TPM error description code.
 *
 */

#include "tpm_error_messages.h"

tpm_error_info* get_tpm_error_info_specific(uint32_t error_code)
{
  /*
   * All known TPM 1.2 error descriptions are handled in
   * the common code. Nothing to do here.
   */
  return NULL;
}
