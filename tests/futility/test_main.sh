#!/bin/bash -eux
# Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

me=${0##*/}

# Work in scratch directory
cd "$OUTDIR"
TMP="$me.tmp"

# No args returns nonzero exit code
"$FUTILITY" && false

# Make sure all built-in commands are listed and have help
expected=\
'dev_sign_file
dump_fmap
help
vbutil_firmware
vbutil_kernel
vbutil_key
vbutil_keyblock'
got=$("$FUTILITY" help |
  egrep '^[[:space:]]+[^[:space:]]+[[:space:]]+[^[:space:]]+' |
  awk '{print $1}')
[ "$expected" = "$got" ]

# It's weird but okay if the command is a full path.
"$FUTILITY" /fake/path/to/help  > "$TMP"
grep Usage "$TMP"

# Make sure logging does something.
# Note: This will zap any existing log file. Too bad.
LOG="/tmp/futility.log"
rm -f "$LOG"
touch "$LOG"
"$FUTILITY" help
grep "$FUTILITY" "$LOG"
rm "$LOG"

# cleanup
rm -f "$TMP"
exit 0
