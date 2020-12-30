#!/bin/bash
#
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script ensures absence of a <!-- tainted --> tag in image's license.

# Abort on error.
set -e

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

usage() {
  echo "Usage ${PROG} image"
}

main() {
  if [[ $# -ne 1 ]]; then
    usage
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

  local license_dir="${rootfs}/opt/google/chrome/"
  if [[ ! -d "${license_dir}" ]]; then
    echo "Directory ${license_dir} does not exist. Skipping the tainted check."
    exit 0
  fi

  local license=$(find "${license_dir}" -name about_os_credits.html 2>/dev/null)
  if [[ -z "${license}" ]]; then
    echo "License file about_os_credits.html not found in ${license_dir}"
    exit 1
  fi

  local tainted_tag="<!-- tainted -->"
  local tainted_status=$(grep "${tainted_tag}" "${license}")
  if [[ -n "${tainted_status}" ]]; then
    echo "License file $license contains packages with LICENSE=TAINTED."
    echo "Remove those packages or modify their license to sign the image."
    # Grab the list of tainted packages and remove html tags from it
    local tainted_pkgs=$(sed -n '/Image is TAINTED/,/<\/ul>/p' "${license}")
    local tainted_pkgs=$(echo "$tainted_pkgs" | sed -e 's/<[^>]*>//g')
    local tainted_pkgs=$(echo "$tainted_pkgs" | grep "\S") # remove empty lines
    echo "${tainted_pkgs}"
    exit 1
  fi
  exit 0
}
main "$@"
