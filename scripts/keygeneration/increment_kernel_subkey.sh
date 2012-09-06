#!/bin/bash
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to increment kernel subkey for firmware updates.
# Used when revving versions for a firmware update.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

# Abort on errors.
set -e

if [ $# -ne 1 ]; then
  cat <<EOF
  Usage: $0 <keyset directory>

  Increments the kernel subkey in the specified keyset.
EOF
  exit 1
fi

KEY_DIR=$1

main() {
  local key_dir=$1
  cd "${key_dir}"
  load_current_versions

  backup_existing_kernel_keys $CURRENT_KSUBKEY_VERSION $CURRENT_KDATAKEY_VERSION

  new_ksubkey_version=$(( CURRENT_KSUBKEY_VERSION + 1 ))

  if [ $new_ksubkey_version -gt 65535 ];
  then
    echo "Kernel key version overflow!"
    exit 1
  fi

  cat <<EOF
Generating new kernel subkey, data keys and new kernel keyblock.

New Firmware version (due to kernel subkey change): ${new_ksubkey_version}.
EOF
  make_pair kernel_subkey $KERNEL_SUBKEY_ALGOID $new_ksubkey_version
  make_pair kernel_data_key $KERNEL_DATAKEY_ALGOID $CURRENT_KDATAKEY_VERSION
  make_keyblock kernel $KERNEL_KEYBLOCK_MODE kernel_data_key kernel_subkey

  write_updated_version_file $CURRENT_FKEY_VERSION $new_ksubkey_version \
    $CURRENT_KDATAKEY_VERSION $CURRENT_KERNEL_VERSION
}

main $@
