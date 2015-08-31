#!/bin/bash
# Copyright 2015 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Wrapper script for signing firmware image using cbootimage.

# Determine script directory.
SCRIPT_DIR=$(dirname "$0")

# Load common constants and variables.
. "${SCRIPT_DIR}/common_minimal.sh"

# Abort on error.
set -e

usage() {
  cat<<EOF
Usage: $0 <pkc_key> <firmware_image> <soc>

Signs <firmware_image> with <pkc_key> using cbootimage for <soc>.
EOF
  exit 1
}

main() {
  if [[ $# -ne 3 ]]; then
    usage
  fi

  local pkc_key=$1
  local firmware_image=$2
  local soc=$3

  local temp_fw=$(make_temp_file)

  cat >update.cfg <<EOF
PkcKey = $pkc_key, --save;
ReSignBl;
EOF

  # This also generates a file pubkey.sha which contains the hash of public key
  # required by factory to burn into PKC fuses.
  cbootimage -s ${soc} -u update.cfg "${firmware_image}" "${temp_fw}"
  cp "${temp_fw}" "${firmware_image}"
}

main "$@"
