#!/bin/bash
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

load_shflags || exit 1

FLAGS_HELP="Usage: ${PROG} [options] <manifest>

Validates that cr50 signing manifest includes restrictions.
"

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error and uninitialized variables.
set -e
set -u

# Verify that the manifest file includes either a board or device restriction.
verify_restrictions() {
  [[ $# -eq 1 ]] || die "Usage: verify_restrictions <manifest>"
  local manifest_file="$1"

  local restrictions=($(awk '
    BEGIN {
      board = 0;
      device = 0;
    };
    /"board_id": (0x[0-9a-f]+|[0-9]+),/ && strtonum($2) != 0 {
      board += 1
    };
    /"board_id_mask": (0x[0-9a-f]+|[0-9]+),/ && strtonum($2) == 0 {
      board += 1
    };
    /"board_id_flags": (0x[0-9a-f]+|[0-9]+),/ && strtonum($2) == 0x10 {
      board += 1
    };
    /"DEV_ID[01]": [-0-9a-f]+,/ && strtonum($2) != 0 { device += 1 };
    END { print board, device };
  ' "${manifest_file}"))

  if [[ ${#restrictions[@]} != 2 ]]; then
    die "Invalid number of restrictions counted."
  fi

  local board="${restrictions[0]}"
  local device="${restrictions[1]}"

  if [[ "${board}" != "3" || "${device}" != "0" ]]; then
    die "Manifest lacks board (board=${board} != 3) or has" \
      "device (device=${device} != 0) restrictions."
  fi
}

main() {
  if [[ $# -ne 1 ]]; then
    flags_help
    exit 1
  fi

  local manifest_file="$1"

  if [[ ! -e "${manifest_file}" ]]; then
    die "Missing manifest file: ${manifest_file}"
  fi

  verify_restrictions "${manifest_file}"
}
main "$@"
