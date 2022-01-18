#!/bin/bash

# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Load common constants and functions.
. "$(dirname "$0")/common_leverage_hammer.sh"

usage() {
  cat <<EOF
Usage: ${PROG} <keyname> [options]

Arguments:
  keyname:                   Name of the hammer device (e.g. Staff, Wand).

Options:
  -o, --output_dir <dir>:    Where to write the keys (default is cwd)
EOF

  if [[ $# -ne 0 ]]; then
    die "$*"
  else
    exit 0
  fi
}

main() {
  set -ex

  leverage_hammer_to_create_key "$@"
}

main "$@"
