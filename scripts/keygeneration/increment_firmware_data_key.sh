#!/bin/bash
# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to increment kernel subkey and datakey for firmware updates.
# Used when revving versions for a firmware update.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

# Abort on errors.
set -e

# File to read current versions from.
VERSION_FILE="key.versions"

main() {
  current_fkey_version=$(get_version "firmware_key_version" $VERSION_FILE)
  # Firmware version is the kernel subkey version.
  current_ksubkey_version=$(get_version "firmware_version" $VERSION_FILE)
  # Kernel data key version is the kernel key version.
  current_kdatakey_version=$(get_version "kernel_key_version" $VERSION_FILE)
  current_kernel_version=$(get_version "kernel_version" $VERSION_FILE)

  cat <<EOF
Current Firmware key version: ${current_fkey_version}
Current Firmware version: ${current_ksubkey_version}
Current Kernel key version: ${current_kdatakey_version}
Current Kernel version: ${current_kernel_version}
EOF

  new_fkey_version=$(( current_fkey_version + 1 ))

  if [ $new_fkey_version -gt 65535 ];
  then
    echo "Version overflow!"
    exit 1
  fi

  cat <<EOF
Generating new kernel subkey, data keys and new kernel keyblock.

New Firmware key version (due to firmware key change): ${new_fkey_version}.
EOF
  make_pair firmware_data_key $FIRMWARE_DATAKEY_ALGOID $new_fkey_version

  write_updated_version_file $new_fkey_version $current_ksubkey_version \
    $current_kdatakey_version $current_kernel_version $VERSION_FILE
}

main $@
