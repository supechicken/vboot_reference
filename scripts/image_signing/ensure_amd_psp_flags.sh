#!/bin/bash

# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# abort on error
set -e

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

REQUIRED_BITS=$((1 << 58)) # PSP_S0I3_RESUME_VERSTAGE bit
FORBIDDEN_BITS="0x0"

main() {
if [[ $# -ne 1 ]]; then
    echo "Usage $0 <image>"
    exit 1
  fi

  local image="$1"

  local loopdev rootfs
  if [[ -d "${image}" ]]; then
    rootfs="${image}"
  else
    rootfs=$(make_temp_dir)
    loopdev=$(loopback_partscan "${image}")
    mount_loop_image_partition_ro "${loopdev}" 3 "${rootfs}"
  fi

  # Get the board for this image
  local board firmware_bundle shellball_dir
  board=$(get_board_from_lsb_release "${rootfs}")
  firmware_bundle="${rootfs}/usr/sbin/chromeos-firmwareupdate"
  shellball_dir=$(make_temp_dir)

  if [ "${board}" != "guybrush" ]; then
    sudo umount "${rootfs}"
    exit
  fi

  # Extract our firmware
  if ! extract_firmware_bundle "${firmware_bundle}" "${shellball_dir}"; then
        sudo umount "${rootfs}"
        die "Failed to extract firmware bundle"
  fi

  # Check the soft fuse bits in each FW image
  local images
  images=$(ls "${shellball_dir}"/images/bios-*)
  for image in ${images}
  do
    # Output is Soft-fuse:value
    local soft_fuse forbidden_set
    soft_fuse=$(amdfwread --soft-fuse "${image}" | \
      grep "Soft-fuse" | cut -d ":" -f2)

    forbidden_set=$((soft_fuse & FORBIDDEN_BITS))
    if [ "${forbidden_set}" != 0 ]; then
      die $(printf "%s: Forbidden AMD PSP soft fuse bits set: 0x%x" \
        "${image}" "${forbidden_set}")
    fi

    if [ $((soft_fuse & REQUIRED_BITS)) != $((REQUIRED_BITS)) ]; then
      die $(printf "%s: Required AMD PSP soft fuse bits not set: 0x%x" \
        "${image}" "$((~soft_fuse & REQUIRED_BITS)))"
    fi
  done
}
main "$@"
