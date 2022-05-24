#!/bin/bash

# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# abort on error
set -eu

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

declare -A required_bit_masks=(
  [guybrush]="$((1 << 58))" # PSP_S0I3_RESUME_VERSTAGE bit
  [zork]="0x0"
)

declare -A forbidden_bit_masks=(
  [guybrush]="0x0"
  [zork]="0x0"
)

# Grunt uses an old firmware format that amdfwread cannot read
# See b/233787191 for skyrim
board_ignore_list=(grunt skyrim)

main() {
if [[ $# -ne 2 ]]; then
    echo "Usage $0 <board> <image>"
    exit 1
  fi

  local board="$1"
  local image="$2"

  # Check the ignore list
  local ignored
  for ignored in "${board_ignore_list[@]}"
  do
    if [ "${board}" == "${ignored}" ]; then
      exit 0
    fi
  done

  # Mount the image
  local loopdev rootfs
  if [[ -d "${image}" ]]; then
    rootfs="${image}"
  else
    rootfs="$(make_temp_dir)"
    loopdev="$(loopback_partscan "${image}")"
    mount_loop_image_partition_ro "${loopdev}" 3 "${rootfs}"
  fi

  local firmware_bundle shellball_dir
  firmware_bundle="${rootfs}/usr/sbin/chromeos-firmwareupdate"
  shellball_dir="$(make_temp_dir)"

  # Get the board specific bit masks
  local required_bit_mask forbidden_bit_mask

  if [[ -z "${required_bit_masks[${board}]+x}" ]]; then
    die "Missing PSP required bit mask set for ${board}"
  fi

  if [[ -z "${forbidden_bit_masks[${board}]+x}" ]]; then
    die "Missing PSP forbidden bit mask set for ${board}"
  fi

  required_bit_mask="${required_bit_masks[${board}]}"
  forbidden_bit_mask="${forbidden_bit_masks[${board}]}"

  # Extract our firmware
  if ! extract_firmware_bundle "${firmware_bundle}" "${shellball_dir}"; then
    die "Failed to extract firmware bundle"
  fi

  # Find our images and check the soft-fuse bits in each
  declare -a images
  readarray -t images < <(find "${shellball_dir}" -iname 'bios-*')

  local image
  for image in "${images[@]}"
  do
    # Output is Soft-fuse:value
    local soft_fuse forbidden_set missing_set
    soft_fuse="$(amdfwread --soft-fuse "${image}" | \
      sed -E -n 's/Soft-fuse:(0[xX][0-9a-fA-F]+)/\1/p')"

    if [[ -z "${soft_fuse}" ]]; then
      die "Failed to get soft-fuse bits from firmware"
    fi

    forbidden_set="$((soft_fuse & forbidden_bit_mask))"
    if [[ "${forbidden_set}" != 0 ]]; then
      die "$(printf "%s: Forbidden AMD PSP soft fuse bits set: 0x%x" \
        "${image}" "${forbidden_set}")"
    fi

    missing_set="$((~soft_fuse & required_bit_mask))"
    if [[ "${missing_set}" != 0 ]]; then
      die "$(printf "%s: Required AMD PSP soft fuse bits not set: 0x%x" \
        "${image}" "${missing_set}")"
    fi
  done
}
main "$@"
