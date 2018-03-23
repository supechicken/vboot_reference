/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Interface for getting descriptions for TPM error codes.
 *
 */

#ifndef TPM_ERROR_MESSAGES_H
#define TPM_ERROR_MESSAGES_H

#include <stddef.h>
#include <stdint.h>

typedef struct tpm_error_info {
  const char* name;
  const char* description;
} tpm_error_info;

/* Get error information.
 *
 * @error_code: the code returned from Tlcl function.
 *
 * Returns:
 *  - for known errors: the pointer to error information structure.
 *  - for unknown errors: NULL.
 */
tpm_error_info* get_tpm_error_info(uint32_t error_code);

#endif  /* TPM_ERROR_MESSAGES_H */
