/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_TSS_CONSTANTS_H_
#define VBOOT_REFERENCE_TSS_CONSTANTS_H_

#include "tss_common_constants.h"

#if TPM_DYNAMIC

#include "tpm2_tss_constants.h"
#include "tpm1_tss_constants.h"
#define TPM_MAX_COMMAND_SIZE 4096
#define TPM_PCR_DIGEST 32

#else /* if !TPM_DYNAMIC */
#ifdef TPM2_MODE

#include "tpm2_tss_constants.h"
#define TPM_PERMANENT_FLAGS TPM2_PERMANENT_FLAGS
#define TPM_STCLEAR_FLAGS TPM2_STCLEAR_FLAGS
#define TPM_IFX_FIELDUPGRADEINFO TPM2_IFX_FIELDUPGRADEINFO
#define TPM_MAX_COMMAND_SIZE TPM2_MAX_COMMAND_SIZE
#define TPM_PCR_DIGEST TPM2_PCR_DIGEST

#else /* ifndef TPM2_MODE */

#include "tpm1_tss_constants.h"
#define TPM_PERMANENT_FLAGS TPM1_PERMANENT_FLAGS
#define TPM_STCLEAR_FLAGS TPM1_STCLEAR_FLAGS
#define TPM_IFX_FIELDUPGRADEINFO TPM1_IFX_FIELDUPGRADEINFO
#define TPM_MAX_COMMAND_SIZE TPM1_MAX_COMMAND_SIZE
#define TPM_PCR_DIGEST TPM1_PCR_DIGEST

#endif /* TPM2_MODE */
#endif /* TPM_DYNAMIC */

#endif  /* VBOOT_REFERENCE_TSS_CONSTANTS_H_ */
