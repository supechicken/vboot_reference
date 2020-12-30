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
    echo "Usage $PROG image"
}

main() {
    if [[ $# -ne 1 ]]; then
        usage
        exit 1
    fi

    local image="$1"

    local loopdev=$(loopback_partscan "${image}")
    local rootfs=$(make_temp_dir)
    mount_loop_image_partition_ro "${loopdev}" 3 "${rootfs}"

    # License file was located in a different place, than the licensing docs
    # suggested, so we perform a search to be safe.
    local license=$(find "${rootfs}" -name about_os_credits.html 2>/dev/null)
    if [[ -z "$license" ]]
    then
        echo "License file about_os_credits.html not found!"
        exit 1
    fi

    local html_header=$(sed -n '/<head>/,/<\/head>/p' "$license")

    local tainted_tag="<!-- tainted -->"
    local tainted_status=$(echo "${html_header}" | grep "${tainted_tag}")
    if [[ -z "$tainted_status" ]]
    then
        exit 0
    else
        echo "Image is tainted!"
        exit 1
    fi
}
main "$@"
