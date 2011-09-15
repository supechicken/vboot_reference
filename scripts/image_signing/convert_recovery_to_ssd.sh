#!/bin/bash 

# Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Script to convert a recovery image into an SSD image. Changes are made in-
# place.

usage() {
  cat <<EOF
Usage: $PROG <image> [--force] [--cgpt /path/to/cgpt]

In-place converts recovery <image> into an SSD image. With --force, does not
ask for confirmation from the user.

EOF
}

if [ $# -gt 0 ]; then
  IMAGE=$1
  shift
else
  usage
  exit 1
fi

if [ $# -gt 3 ]; then
  usage
  exit 1
fi

while (( "$#" )); do
  echo "working on: $1"
  case "$1" in
  --force)
    IS_FORCE=$1
    shift
    ;;
  --cgpt)
    shift
    CGPT=$1
    shift
    if [[ -z $CGPT ]]; then
      usage
      exit 1
    fi
    ;;
  *)
    usage
    exit 1
    ;;
  esac
done

: ${GPT:=$CGPT}

if [[ -z $GPT ]]; then
  GPT=cgpt
fi

type -P $GPT &>/dev/null ||
  { echo "cgpt tool must be in the path"; exit 1; }

# Load common constants (after we have set GPT) and variables.
. "$(dirname "$0")/common_minimal.sh"

# Abort on errors.
set -e

if [ "${IS_FORCE}" != "--force" ]; then
  echo "This will modify ${IMAGE} in-place and convert it into an SSD image."
  read -p "Are you sure you want to continue (y/N)?" SURE
  SURE="${SURE:0:1}"
  [ "${SURE}" != "y" ] && exit 1
fi

kerna_offset=$(partoffset ${IMAGE} 2)
kernb_offset=$(partoffset ${IMAGE} 4)
# Kernel partition sizes should be the same.
kern_size=$(partsize ${IMAGE} 2)

# Move Kernel B to Kernel A.
kernb=$(make_temp_file)
echo "Replacing Kernel partition A with Kernel partition B"
extract_image_partition ${IMAGE} 4 ${kernb}
replace_image_partition ${IMAGE} 2 ${kernb}

# Overwrite the vblock.
stateful_dir=$(make_temp_dir)
tmp_vblock=$(make_temp_file)
mount_image_partition_ro ${IMAGE} 1 ${stateful_dir}
sudo cp ${stateful_dir}/vmlinuz_hd.vblock ${tmp_vblock}
# Unmount before overwriting image to avoid sync issues.
sudo umount -d ${stateful_dir}
echo "Overwriting kernel partition A vblock with SSD vblock"
sudo dd if=${tmp_vblock} of=${IMAGE} seek=${kerna_offset} bs=512 conv=notrunc

# Zero out Kernel B partition.
echo "Zeroing out Kernel partition B"
sudo dd if=/dev/zero of=${IMAGE} seek=${kernb_offset} bs=512 count=${kern_size} conv=notrunc
echo "${IMAGE} was converted to an SSD image."
