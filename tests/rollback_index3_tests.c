/* Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Tests for do-nothing rollback_index functions with disabled TPM
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "rollback_index.h"
#include "test_common.h"

int main(int argc, char* argv[])
{
	TEST_EQ(RollbackKernelLock(), 0, "RollbackKernelLock()");

	return gTestSuccess ? 0 : 255;
}
