#!/bin/sh
#
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# This script can change GBB flags in system live firmware or a given image
# file.

SCRIPT_BASE="$(dirname "$0")"
. "$SCRIPT_BASE/gbb_flags_common.sh"

# DEFINE_string name default_value description flag
DEFINE_string file "" "Path to firmware image. Default to system firmware." "f"
DEFINE_boolean explicit $FLAGS_FALSE "Print list of what flags are set." "e"

set -e

main() {
  if [ $# -ne 0 ]; then
    flags_help
    exit 1
  fi

  local image_file="$FLAGS_file"

  if [ -z "$FLAGS_file" ]; then
    image_file="$(make_temp_file)"
    flashrom $FLASHROM_READ_OPT "$image_file"
  fi

  # Process file
  local
  old_value="$(futility gbb -g --flags "$image_file")"
  local raw_old_value=$(echo $old_value | egrep -o "0x[0-9]+")
  printf "ChromeOS GBB set %s.\n" "$old_value" >&2

  if [ "$FLAGS_explicit" = "$FLAGS_TRUE" ]; then
    local last_name=""
    local is_this_a_name=1
    local flags_in_old_value=""
    # Go over all flags and see if they are defined
    for entry in $GBBFLAGS_LIST;
    do
      if [ $is_this_a_name -eq 1 ]; then
        last_name=$entry
        is_this_a_name=0
      else
        local
        flag_in_gbb=$(($entry & $raw_old_value))
        if [ $flag_in_gbb -ne 0 ]; then
          flags_in_old_value=$flags_in_old_value" "$last_name
        fi
        is_this_a_name=1
      fi
    done
    printf "ChromeOS GBB set flags listed:\n" >&2
    for entry in $flags_in_old_value;
    do
      printf "%s\n" "$entry" >&2
    done
  fi
}

# Parse command line
FLAGS "$@" || exit 1
ORIGINAL_PARAMS="$@"
eval set -- "$FLAGS_ARGV"

main "$@"
