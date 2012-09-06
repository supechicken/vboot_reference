#!/bin/bash
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
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

  backup_existing_kernel_keys $current_ksubkey_version $current_kdatakey_version

  new_ksubkey_version=$(( current_ksubkey_version + 1 ))

  if [ $new_ksubkey_version -gt 65535 ];
  then
    echo "Version overflow!"
    exit 1
  fi

  cat <<EOF
Generating new kernel subkey, data keys and new kernel keyblock.

New Firmware version (due to kernel subkey change): ${new_ksubkey_version}.
EOF
  make_pair kernel_subkey $KERNEL_SUBKEY_ALGOID $new_ksubkey_version
  make_pair kernel_data_key $KERNEL_DATAKEY_ALGOID $current_kdatakey_version
  make_keyblock kernel $KERNEL_KEYBLOCK_MODE kernel_data_key kernel_subkey

  write_updated_version_file $current_fkey_version $new_ksubkey_version \
    $current_kdatakey_version $current_kernel_version $VERSION_FILE
}

main $@
