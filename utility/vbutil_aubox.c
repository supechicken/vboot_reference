/*
 * Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * A Busybox-like bundle of Auto-Update friendly utilities.
 * */

#include <stdio.h>
#include <string.h>

#define DECLARE_APPLET(x) int x##_main(int argc, char *argv[])
#define TRY_INVOKE_APPLET(x) \
    if (strcmp(name, #x) == 0) { *ret = x##_main(argc, argv); }

DECLARE_APPLET(crossystem);
DECLARE_APPLET(dump_fmap);
DECLARE_APPLET(gbb_utility);

int run_applet(const char *name, int *ret, int argc, char *argv[]) {
  TRY_INVOKE_APPLET(crossystem) else
  TRY_INVOKE_APPLET(dump_fmap) else
  TRY_INVOKE_APPLET(gbb_utility) else {
    return 0;
  }

  return 1;
}

int main(int argc, char *argv[]) {
  int ret = 0;
  const char *applet = strchr(argv[0], '/');

  if (applet)
    applet++;
  else
    applet = argv[0];

  /* Allow executing vbutil_aubox as symlink. */
  if (run_applet(applet, &ret, argc, argv))
    return ret;

  if (argc > 1) {
    /* Allow execution in command line: vbutil_aubox COMMAND PARAMS */
    applet = argv[1];
    if (run_applet(applet, &ret, argc - 1, argv + 1))
      return ret;
  }

  fprintf(stderr, "Unknown applet: %s.\n", applet);
  return -1;
}
