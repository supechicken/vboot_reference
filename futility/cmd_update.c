/*
 * Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * The command line tool to invoke firmware updater.
 */

#include "futility.h"
#include "updater.h"

int do_update(int argc, char *argv[]);

DECLARE_FUTIL_COMMAND(update, do_update, VBOOT_VERSION_ALL,
		      "Update system firmware");
