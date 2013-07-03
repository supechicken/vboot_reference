#!/bin/bash

# Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to manipulate the tag files in the output of build_image

# Load common constants.  This should be the first executable line.
# The path to common.sh should be relative to your script's location.
. "$(dirname "$0")/common.sh"

load_shflags

DEFINE_string from "chromiumos_image.bin" \
  "Input file name of Chrome OS image to tag/stamp."

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error.
set -e

if [ -z "${FLAGS_from}" ] || [ ! -s "${FLAGS_from}" ] ; then
  echo "Error: need a valid file by --from"
  exit 1
fi

# Global variable to track if the image is modified.
g_modified=${FLAGS_FALSE}

# Swiped/modifed from $SRC/src/scripts/base_library/base_image_util.sh.
zero_free_space() {
  local rootfs="$1"
  local sudo

  if [ ! -w "${rootfs}" ]; then
    sudo="sudo"
  fi

  echo "Zeroing freespace in ${rootfs}"
  # dd is a silly thing and will produce a "No space left on device" message
  # that cannot be turned off and is confusing to unsuspecting victims.
  info "${rootfs}/filler"
  ( ${sudo} dd if=/dev/zero of="${rootfs}/filler" bs=4096 conv=fdatasync \
      status=noxfer || true ) 2>&1 | grep -v "No space left on device"
  sudo rm "${rootfs}/filler"
}


process_all_clean_ups() {
  local rootfs="$1"
  local do_modifications="$2"
  local boot="${rootfs}/boot"
  local sudo

  if [ ! -w "${boot}" ]; then
    sudo="sudo"
  fi

  if [ -d "${boot}" ]; then
    g_modified=${FLAGS_TRUE}
  fi
  if [ ${do_modifications} = ${FLAGS_TRUE} ]; then
    ${sudo} rm -rf "${boot}" &&
      echo "/boot directory was removed."

    # To prevent the files we just removed from the FS from remaining as non-
    # zero trash blocks that bloat payload sizes, need to zero them. This was
    # done when the image was built, but needs to be repeated now that we've
    # modified it in a non-trivial way.
    zero_free_space "${rootfs}"
  fi
}


IMAGE=$(readlink -f "${FLAGS_from}")
if [[ -z "${IMAGE}" || ! -f "${IMAGE}" ]]; then
  echo "Missing required argument: --from (image to update)"
  usage
  exit 1
fi

# First round, mount as read-only and check if we need any modifications.
rootfs=$(make_temp_dir)
mount_image_partition_ro "${IMAGE}" 3 "${rootfs}"

# we don't have tags in stateful partition yet...
# stateful_dir=$(make_temp_dir)
# mount_image_partition ${IMAGE} 1 ${stateful_dir}

process_all_clean_ups "${rootfs}" ${FLAGS_FALSE}

if [ ${g_modified} = ${FLAGS_TRUE} ]; then
  # remount as RW (we can't use mount -o rw,remount because of loop device)
  sudo umount "${rootfs}"
  mount_image_partition "${IMAGE}" 3 "${rootfs}"

  # second round, apply the modification to image.
  process_all_clean_ups "${rootfs}" ${FLAGS_TRUE}

  # this is supposed to be automatically done in mount_image_partition,
  # but it's no harm to explicitly make it again here.
  tag_as_needs_to_be_resigned "${rootfs}"
  echo "IMAGE IS MODIFIED. PLEASE REMEMBER TO RESIGN YOUR IMAGE."
else
  echo "Image is not modified."
fi
