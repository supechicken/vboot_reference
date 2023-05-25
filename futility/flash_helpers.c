/* Copyright 2023 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "flash_helpers.h"
#include "futility.h"
#include "updater.h"

/*
 * Prepare for flashrom interaction. Setup cfg from args and put servo into
 * flash mode if servo is in use. If this succeeds teardown_flash must be
 * called.
 */
int setup_flash(struct updater_config **cfg,
		struct updater_config_arguments *args)
{
#ifdef USE_FLASHROM
	*cfg = updater_new_config();
	if (!*cfg) {
		ERROR("Out of memory\n");
		return 1;
	}
	const char *prepare_ctrl_name = (*cfg)->prepare_ctrl_name;
	if (args->detect_servo) {
		char *servo_programmer = host_detect_servo(&prepare_ctrl_name);
		if (!servo_programmer) {
			ERROR("Problem communicating with servo\n");
			goto errdelete;
		}

		if (!args->programmer)
			args->programmer = servo_programmer;
		else
			free(servo_programmer);
	}

	bool ignored;
	if (updater_setup_config(*cfg, args, &ignored)) {
		ERROR("Bad servo options\n");
		goto errdelete;
	}
	prepare_servo_control(prepare_ctrl_name, true);
	return 0;

errdelete:
	updater_delete_config(*cfg);
	*cfg = NULL;
	return 1;
#else
	return 1;
#endif /* USE_FLASHROM */
}

/* Cleanup objects created in setup_flash and release servo from flash mode. */
void teardown_flash(struct updater_config *cfg)
{
#ifdef USE_FLASHROM
	prepare_servo_control(cfg->prepare_ctrl_name, false);
	updater_delete_config(cfg);
#endif /* USE_FLASHROM */
}
