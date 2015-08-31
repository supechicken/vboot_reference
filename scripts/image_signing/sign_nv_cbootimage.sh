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
Usage: $0 <type> <pkc_key> <firmware_image> <soc>

Signs <firmware_image> of <type> with <pkc_key> using cbootimage for <soc>.
where type is one of
      bootloader = sign bootloader image
EOF
  exit 1
}

main() {
  if [[ $# -ne 4 ]]; then
    usage
  fi

  local type=$1
  local pkc_key=$2
  local firmware_image=$3
  local soc=$4

  local temp_fw=$(make_temp_file)

  if [[ "${type}" == "bootloader" ]]; then

    cat >update.cfg <<EOF
PkcKey = ${pkc_key}, --save;
ReSignBl;
EOF

    # This also generates a file pubkey.sha which contains the hash of public
    # key required by factory to burn into PKC fuses. Move pubkey.sha into
    # ${firmware_image}.pubkey.sha
    cbootimage -s "${soc}" -u update.cfg "${firmware_image}" "${temp_fw}"
    mv "${temp_fw}" "${firmware_image}"
    mv "pubkey.sha" "${firmware_image}.pubkey.sha"

  else
    usage
  fi

}

main "$@"
