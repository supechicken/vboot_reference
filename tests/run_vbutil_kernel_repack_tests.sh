#!/bin/bash -u
#
# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This tests the --verify and --repack functions of vbutil_kernel, to ensure
# that both v2 and v3 preamble headers are accepted, preserved, or changed
# according to the commandline args. vbutil_firmware doesn't have a --repack
# option, so changing the shellball signature is much more complicated and not
# part of this test.

# Load common constants and variables for tests.
. "$(dirname "$0")/common.sh"

# all algs
algs="0 1 2 3 4 5 6 7 8 9 10 11"

# data directories
PREAMBLE_DIR="${SCRIPT_DIR}/preamble_tests"
DATADIR="${PREAMBLE_DIR}/data"
VBDIR2="${PREAMBLE_DIR}/preamble_v2x"
VBDIR3="${PREAMBLE_DIR}/preamble_v3x"
TMPDIR="${TEST_DIR}/vbutil_kernel_repack_tests_dir"
[ -d "${TMPDIR}" ] || mkdir -p "${TMPDIR}"


tests=0
errs=0


# Make sure we can distinguish the formats if desired.
for d in $algs; do
  for r in $algs; do

    # Accept v3
    : $(( tests++ ))
    echo -n "verify v3 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR3}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept v2
    : $(( tests++ ))
    echo -n "verify v2 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR2}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v3
    : $(( tests++ ))
    echo -n "verify only v3 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR3}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 3 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Reject v2
    : $(( tests++ ))
    echo -n "reject v2 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR2}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 3 >/dev/null 2>&1
    if [ "$?" -eq 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v2
    : $(( tests++ ))
    echo -n "verify only v2 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR2}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 2 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Reject v3
    : $(( tests++ ))
    echo -n "reject v3 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" --verify "${VBDIR3}/kern_${d}_${r}.vblock" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 2 >/dev/null 2>&1
    if [ "$?" -eq 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Repack v2 into v3
    : $(( tests++ ))
    echo -n "repack v2 into v3 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --repack "${TMPDIR}/kern_${d}_${r}_v2_v3.bin" \
      --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
      --signprivate "${DATADIR}/data_${d}.vbprivk" \
      --oldblob "${VBDIR2}/kern_${d}_${r}.vblock" \
      --format 3 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v3
    : $(( tests++ ))
    echo -n "verify only v3 kern_${d}_${r}_v2_v3.bin with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --verify "${TMPDIR}/kern_${d}_${r}_v2_v3.bin" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 3 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi


    # Repack v3 into v2
    : $(( tests++ ))
    echo -n "repack v3 into v2 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --repack "${TMPDIR}/kern_${d}_${r}_v3_v2.bin" \
      --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
      --signprivate "${DATADIR}/data_${d}.vbprivk" \
      --oldblob "${VBDIR3}/kern_${d}_${r}.vblock" \
      --format 2 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v2
    : $(( tests++ ))
    echo -n "verify only v2 kern_${d}_${r}_v3_v2.bin with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --verify "${TMPDIR}/kern_${d}_${r}_v3_v2.bin" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 2 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Repack v2 into v2
    : $(( tests++ ))
    echo -n "repack v2 into v2 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --repack "${TMPDIR}/kern_${d}_${r}_v2_v2.bin" \
      --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
      --signprivate "${DATADIR}/data_${d}.vbprivk" \
      --oldblob "${VBDIR2}/kern_${d}_${r}.vblock" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v2
    : $(( tests++ ))
    echo -n "verify only v2 kern_${d}_${r}_v2_v2.bin with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --verify "${TMPDIR}/kern_${d}_${r}_v2_v2.bin" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 2 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Repack v3 into v3
    : $(( tests++ ))
    echo -n "repack v3 into v3 kern_${d}_${r}.vblock with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --repack "${TMPDIR}/kern_${d}_${r}_v3_v3.bin" \
      --keyblock "${DATADIR}/kb_${d}_${r}.keyblock" \
      --signprivate "${DATADIR}/data_${d}.vbprivk" \
      --oldblob "${VBDIR3}/kern_${d}_${r}.vblock" >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

    # Accept only v3
    : $(( tests++ ))
    echo -n "verify only v3 kern_${d}_${r}_v3_v3.bin with root_${r}.vbpubk ... "
    "${UTIL_DIR}/vbutil_kernel" \
      --verify "${TMPDIR}/kern_${d}_${r}_v3_v3.bin" \
      --signpubkey "${DATADIR}/root_${r}.vbpubk" \
      --format 3 >/dev/null 2>&1
    if [ "$?" -ne 0 ]; then
      echo -e "${COL_RED}FAILED${COL_STOP}"
      : $(( errs++ ))
    else
      echo -e "${COL_GREEN}PASSED${COL_STOP}"
    fi

  done
done


# Summary
ME=$(basename "$0")
if [ "$errs" -ne 0 ]; then
  echo -e "${COL_RED}${ME}: ${errs}/${tests} tests failed${COL_STOP}"
  exit 1
fi
happy "${ME}: All ${tests} tests passed"
exit 0
