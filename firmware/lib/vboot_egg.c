/* Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Display functions used in kernel selection.
 */

#include "bmpblk_font.h"
#include "gbb_header.h"
#include "utility.h"
#include "vboot_api.h"
#include "vboot_common.h"
#include "vboot_display.h"
#include "vboot_nvstorage.h"

void VbEasterEgg(VbCommonParams* cparams, VbNvContext *vncptr) {
  VBDEBUG(("VbEasterEgg() invoked!\n"));
  (void)VbExDisplayScreen(VB_SCREEN_DEVELOPER_EGG);
  VbExBeep(100, 100);
  VbExBeep(100, 200);
  VbExBeep(100, 400);
  VbExBeep(100, 800);
  VbExBeep(100, 1600);
  VbExBeep(100, 3200);
  VBDEBUG(("VbEasterEgg() complete!\n"));
}
