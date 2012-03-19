#!/bin/bash -eu
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This generates the v3.0 test data used to ensure that modifications to
# VbFirmwarePreambleHeader and VbKernelPreambleHeader will continue to work as
# expected. This assumes that vbutil_firmware and vbutil_kernel emit the
# correct v3.0 format preamble headers by default.

# Load common constants and variables for tests.
. "$(dirname "$0")/common.sh"

# all algs
algs="0 1 2 3 4 5 6 7 8 9 10 11"

# output directories
PREAMBLE_DIR="${SCRIPT_DIR}/preamble_tests"
DATADIR="${PREAMBLE_DIR}/data"
V3DIR="${PREAMBLE_DIR}/preamble_v3x"

[ -d "$V3DIR" ] || mkdir -p "$V3DIR"

# Sign the firmware and kernel data in all the possible ways using the v3.0
# tools.
for d in $algs; do
  echo "alg $d"
  for r in $algs; do
      "${UTIL_DIR}/vbutil_firmware" --vblock "${V3DIR}/fw_${d}_${r}.vblock" \
        --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
        --signprivate "${DATADIR}/data_${d}.vbprivk" \
        --version 1 \
        --kernelkey "${DATADIR}/dummy_0.vbpubk" \
        --fv "${DATADIR}/FWDATA"
     "${UTIL_DIR}/vbutil_kernel" --pack "${V3DIR}/kern_${d}_${r}.vblock" \
       --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
       --signprivate "${DATADIR}/data_${d}.vbprivk" \
       --version 1 \
       --arch arm \
       --vmlinuz "${DATADIR}/KERNDATA" \
       --bootloader "${DATADIR}/dummy_bootloader.bin" \
       --config "${DATADIR}/dummy_config.txt"
  done
done
