#!/bin/bash
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to increment kernel data key for firmware updates.
# Used when revving versions for a firmware update.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

# Abort on errors.
set -e

if [ $# -ne 1 ]; then
  cat <<EOF
  Usage: $0 <keyset directory>

  Increments the kernel data key in the specified keyset.
EOF
  exit 1
fi

KEY_DIR=$1

main() {
  local key_dir=$1
  cd "${key_dir}"
  load_current_version

  backup_existing_kernel_keys $CURRENT_KSUBKEY_VERSION $CURRENT_KDATAKEY_VERSION

  new_kdatakey_version=$(( CURRENT_KDATAKEY_VERSION + 1 ))

  if [ $new_kdatakey_version -gt 65535 ];
  then
    echo "Version overflow!"
    exit 1
  fi

  cat <<EOF
Generating new kernel data version, and new kernel keyblock.

New Kernel data key version: ${new_kdatakey_version}.
EOF
  # make_pair kernel_subkey $KERNEL_SUBKEY_ALGOID $current_ksubkey_version
  make_pair kernel_data_key $KERNEL_DATAKEY_ALGOID $new_kdatakey_version
  make_keyblock kernel $KERNEL_KEYBLOCK_MODE kernel_data_key kernel_subkey

  write_updated_version_file $CURRENT_FKEY_VERSION $CURRENT_KSUBKEY_VERSION \
    $new_kdatakey_version $CURRENT_KERNEL_VERSION
}

main $@
