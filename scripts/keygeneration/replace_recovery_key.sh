#!/bin/bash
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to replace the recovery key with a newly generated one. See usage().

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

# Abort on errors.
set -e

usage() {
  cat <<EOF
Usage: $0 <keyset directory>

Creates a new recovery_key (incl. dependent kernel data keys) and renames the
old one to recovery_key_2. This is useful when we want to prevent units
fabricated in the future from booting current recovery or factory shim images,
but still want future recovery and factory shim images to be able to run on
both new units and those that had already been shipped with the old recovery
key.
EOF
}

# Recovery key versions are not meaningful, so always use "1".
VERSION="1"

main() {
  local ext
  local k

  KEY_DIR=$1

  if [ $# -ne 1 ]; then
    usage
    exit 1
  fi

  cd "${KEY_DIR}"

  if [[ -e "recovery_key_2.vbpubk" ]] || [[ -e "recovery_key_2.vbprivk" ]]; then
    die "recovery_key_2 already exists!"
  fi

  info "Moving old recovery key to recovery_key_2."

  for ext in "vbpubk" "vbprivk"; do
    mv "recovery_key.${ext}" "recovery_key_2.${ext}"
  done

  info "Backing up old kernel data keys (no longer needed) as XXX.old_2.YYY."

  for k in "recovery_kernel" "installer_kernel" "minios_kernel"; do
    for ext in "vbpubk" "vbprivk"; do
      mv "${k}_data_key.${ext}" "${k}_data_key.old_2.${ext}"
    done
    mv "${k}.keyblock" "${k}.old_2.keyblock"
  done

  info "Creating new recovery key."

  make_pair recovery_key "${RECOVERY_KEY_ALGOID}" "${VERSION}"

  info "Creating new recovery, minios and installer kernel data keys."

  make_pair recovery_kernel_data_key "${RECOVERY_KERNEL_ALGOID}" "${VERSION}"
  make_pair minios_kernel_data_key "${MINIOS_KERNEL_ALGOID}" "${VERSION}"
  make_pair installer_kernel_data_key "${INSTALLER_KERNEL_ALGOID}" "${VERSION}"

  info "Creating new keyblocks signed with new recovery key."

  make_keyblock recovery_kernel "${RECOVERY_KERNEL_KEYBLOCK_MODE}" recovery_kernel_data_key recovery_key
  make_keyblock minios_kernel "${MINIOS_KERNEL_KEYBLOCK_MODE}" minios_kernel_data_key recovery_key
  make_keyblock installer_kernel "${INSTALLER_KERNEL_KEYBLOCK_MODE}" installer_kernel_data_key recovery_key

  info "Creating secondary XXX_2.keyblocks signing new kernel data keys with old recovery key."

  make_keyblock recovery_kernel_2 "${RECOVERY_KERNEL_KEYBLOCK_MODE}" recovery_kernel_data_key recovery_key_2
  make_keyblock minios_kernel_2 "${MINIOS_KERNEL_KEYBLOCK_MODE}" minios_kernel_data_key recovery_key_2
  make_keyblock installer_kernel_2 "${INSTALLER_KERNEL_KEYBLOCK_MODE}" installer_kernel_data_key recovery_key_2

  info "All done."
}

main "$@"
