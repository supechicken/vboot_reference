#!/bin/bash
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to increment firmware version key for firmware updates.
# Used when revving versions for a firmware update.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

# Abort on errors.
set -e

if [ $# -ne 1 ]; then
  cat <<EOF
  Usage: $0 <keyset directory>

  Increments the firmware version in the specified keyset.
EOF
  exit 1
fi

KEY_DIR=$1

main() {
  local key_dir=$1
  cd "${key_dir}"
  load_current_versions
  new_fkey_version=$(( CURRENT_FKEY_VERSION + 1 ))

  if [ $new_fkey_version -gt 65535 ];
  then
    echo "Version overflow!"
    exit 1
  fi

  cat <<EOF
Generating new firmware version key.

New Firmware key version (due to firmware key change): ${new_fkey_version}.
EOF
  make_pair firmware_data_key $FIRMWARE_DATAKEY_ALGOID $new_fkey_version
  make_keyblock firmware $FIRMWARE_KEYBLOCK_MODE firmware_data_key root_key

  write_updated_version_file $new_fkey_version $CURRENT_KSUBKEY_VERSION \
    $CURRENT_KDATAKEY_VERSION $CURRENT_KERNEL_VERSION
}

main $@
