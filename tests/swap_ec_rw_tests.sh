#!/bin/bash
#
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Tests for swap_ec_rw.

# Load common constants and variables.
. "$(dirname "$0")/common.sh"

set -e

ME=${0##*/}
TMP="${ME}.tmp"

SWAP="${VBOOT_BIN_DIR}/swap_ec_rw"

# Intentionally use AP and EC images from different boards
AP_IMAGE="tests/futility/data/bios_geralt_cbfs.bin"
EC_IMAGE="tests/futility/data/ec_krabby.bin"

echo "Testing swap_ec_rw..."

# Missing -e or --ec
cp -f "${AP_IMAGE}" "${TMP}"
if "${SWAP}" -i "${TMP}"; then false; fi

# Good case
cp -f "${AP_IMAGE}" "${TMP}"
"${SWAP}" -i "${TMP}" -e "${EC_IMAGE}"
cmp -s "${AP_IMAGE}" "${TMP}" && error "AP image was not modified"

# Cleanup
rm -f "${TMP}"*
exit 0
