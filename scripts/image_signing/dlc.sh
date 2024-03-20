#!/bin/bash

# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

get_unsigned_dlc_id() {
  # Unsigned firmware DLC should always use this DLC_ID.
  echo "dev-signed-cros-fw"
}

has_fw_dlc() {
  local rootfs_dir="$1"
  # Check the existence of firmware DLC metadata.
  [[ -d "$(get_dlc_metadata_dir "${rootfs_dir}" "$(get_unsigned_dlc_id)")" ]]
}

rootfs_fw_is_empty_archive() {
  local src="$1"
  grep -q '^##CUTHERE##' "${src}" && \
    sed -n '/^##CUTHERE##/{n;p;}' "${src}" | grep -q '.'
}

has_fw_in_rootfs() {
  local input="$1"
  has_firmware_update "${input}" && ! rootfs_fw_is_empty_archive "${input}"
}

get_dlc_metadata_dir() {
  local rootfs_dir="$1"
  local dlc_id="$2"
  echo "${rootfs_dir}/opt/google/dlc/${dlc_id}/package/"
}

factory_install_dlc_img_for() {
  local stateful_dir="$1"
  local dlc_id="$2"
  echo "${stateful_dir}/unencrypted/dlc-factory-images/${dlc_id}/package/dlc.img"
}

# Extract firmware bundle. Copy a new shellball.
new_extract_firmware_bundle() {
  local rootfs_dir="$1"
  local stateful_dir="$2"
  local new_shellball="$3"

  if has_fw_dlc "${rootfs_dir}"; then
    # Find the shellball from the DLC.

    local unsigned_dlc_img
    local dlc_dev
    local dlc_dir
    unsigned_dlc_img="$(factory_install_dlc_img_for "${stateful_dir}" "$(get_unsigned_dlc_id)")"
    dlc_dev="$(sudo losetup --show -f "${unsigned_dlc_img}")"
    dlc_dir="$(make_temp_dir)"
    sudo mount -o ro "${dlc_dev}" "${dlc_dir}"

    local firmware_bundle_in_dlc
    firmware_bundle_in_dlc="root/$(get_fw_bundle_rel_path)"

    # extract firmware archive to shellball_dir
    "${dlc_dir}/${firmware_bundle_in_dlc}" --unpack "${shellball_dir}" ||
      die "Extracting firmware autoupdate (--unpack) failed from DLC."

    cp -f "${dlc_dir}/${firmware_bundle_in_dlc}" "${new_shellball}"

    sudo umount "${dlc_dir}"
    sudo losetup --detach "${dlc_dev}"
  elif extract_firmware_bundle "${firmware_bundle_rootfs}" "${shellball_dir}"; then
    cp -f "${firmware_bundle_rootfs}" "${new_shellball}"
  else
    return 1
  fi
}

# Write a python wrapper and call it instead of call raw python script
#
generate_dlc() {
  # parse needed metadata from imageloader.json to avoid hard coded
  #
  local dlc_id="$1"
  local src_dir="$2"
  local tar_dir="$3"

  python -c "
from chromite.lib import dlc_lib

params = dlc_lib.EbuildParams(
    dlc_id='dev-signed-cros-fw',
    dlc_package='package',
    fs_type=dlc_lib.SQUASHFS_TYPE,
    pre_allocated_blocks=335544320,
    version='9999',
    name='unsigned-chromeos-firmware-trogdor',
    description='',
    preload=False,
    mount_file_required=False,
    factory_install=True,
    fullnamerev='',
)
params.VerifyDlcParameters()

generator = dlc_lib.DlcGenerator(
    #src_dir='${new_dlc_dir}',
    src_dir='/build/trogdor/build/rootfs/dlc/unsigned-chromeos-firmware/package/root/',
    sysroot='',
    board=dlc_lib.MAGIC_BOARD,
    ebuild_params=params,
    license_file='../platform/borealis/tools/kabuto/dlc_assets/DLC_LICENSE'
)
artifacts = generator.ExternalGenerateDLC(
    #'${dlc_temp_dir}'
    'tmp_dlc'
)
print(artifacts.StringJSON())
"
}
