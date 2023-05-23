/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef VBOOT_REFERENCE_FLASH_HELPERS_H_
#define VBOOT_REFERENCE_FLASH_HELPERS_H_

#include "futility.h"
#include "updater.h"

int setup_flash(struct updater_config **cfg,
		struct updater_config_arguments *args,
		const char **prepare_ctrl_name);

void teardown_flash(struct updater_config *cfg,
		   const char *prepare_ctrl_name,
		   char *servo_programmer);

#endif /* VBOOT_REFERENCE_FLASH_HELPERS_H_ */
