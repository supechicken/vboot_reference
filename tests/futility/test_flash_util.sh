#!/bin/bash -eux
# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Skip test with USE_FLASHROM=0
"${FUTILITY}" help flash 2>&1 | grep -vq 'built without flashrom support' || exit 0

me=${0##*/}
TMP="${me}.tmp"

# Work in scratch directory
cd "${OUTDIR}"

# 8MB test image
TEST_BIOS="${SCRIPT_DIR}/futility/data/bios_link_mp.bin"
TEST_PROG="dummy:image=${TEST_BIOS},emulate=VARIABLE_SIZE,size=8388608"

# Test flash size
flash_size=$("${FUTILITY}" flash --flash-size -p "${TEST_PROG}")
[ "${flash_size}" = "Flash size: 0x00800000" ]

# Test WP status (VARIABLE_SIZE always has WP disabled)
wp_status=$("${FUTILITY}" flash --wp-status --ignore-hw -p "${TEST_PROG}")
[ "${wp_status}" = "WP status: disabled" ]

# Cleanup
rm -f "${TMP}"*
exit 0
