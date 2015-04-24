#!/bin/bash

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Abort on error.
set -e

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

usage() {
    echo "Usage $PROG image [config]"
}

main() {
    # We want to catch all the discrepancies, not just the first one.
    # So, any time we find one, we set testfail=1 and continue.
    # When finished we will use testfail to determine our exit value.
    local testfail=0

    if [ $# -ne 1 ] && [ $# -ne 2 ]; then
        usage
        exit 1
    fi

    local image="$1"

    # Default config location: same name/directory as this script,
    # with a .config file extension, ie ensure_no_nonrelease_files.config.
    local configfile="$(dirname "$0")/${0/%.sh/.config}"
    # Or, maybe a config was provided on the command line.
    if [ $# -eq 2 ]; then
        configfile="$2"
    fi
    # Either way, load test-expectations data from config.
    . "$configfile" || return 1

    local board="${SECURITY_TEST_BOARD}"
    local rootfs=$(make_temp_dir)
    mount_image_partition_ro "$image" 3 "$rootfs"

    for file in ${RELEASE_FILE_BLACKLIST[@]}; do
        if [ -e "$rootfs/$file" ]; then
            testfail=1
            ls -al "$rootfs/$file"

            # Check if this file for this board is whitelisted. Reset testfail
            # to zero if it is whitelisted.
            for b in "${!RELEASE_FILE_ALLOWED_PER_BOARD[@]}"; do
                if [ "${board}" == "${b}" ]; then
                    local allowed_files=${RELEASE_FILE_ALLOWED_PER_BOARD["$b"]}
                    for f in ${allowed_files}; do
                        if [ "$f" == "$file" ]; then
                            echo "WARN: $file whitelisted for $board"
                            testfail=0
                            break
                        fi
                    done
                fi
            done

            if [ "$testfail" == "1" ]; then
                echo "FAIL: $file exists in this image!"
            fi
        fi
    done

    # Verify that session_manager isn't configured to pass additional
    # environment variables or command-line arguments to Chrome.
    local config_path="$rootfs/etc/chrome_dev.conf"
    local matches=$(grep -s "^[^#]" "${config_path}")
    if [ -n "$matches" ]; then
        echo "FAIL: Found commands in $config_path:"
        echo "$matches"
        testfail=1
    fi

    exit $testfail
}
main "$@"
