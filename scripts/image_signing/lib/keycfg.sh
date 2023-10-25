#!/bin/bash

# Copyright 2023 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Setup the default key configuration by using the local key in `key_dir`.
setup_default_keycfg() {
  local key_dir=$1
  KEYCFG_KERNEL_KEYBLOCK="${key_dir}/kernel.keyblock"
  KEYCFG_KERNEL_VBPRIVK="${key_dir}/kernel_data_key.vbprivk"
  KEYCFG_MINIOS_KERNEL_KEYBLOCK="${key_dir}/minios_kernel.keyblock"
  KEYCFG_MINIOS_KERNEL_V1_KEYBLOCK="${key_dir}/minios_kernel.v1.keyblock"
  KEYCFG_MINIOS_KERNEL_VBPRIVK="${key_dir}/minios_kernel_data_key.vbprivk"
  KEYCFG_RECOVERY_KERNEL_KEYBLOCK="${key_dir}/recovery_kernel.keyblock"
  KEYCFG_RECOVERY_KERNEL_V1_KEYBLOCK="${key_dir}/recovery_kernel.v1.keyblock"
  KEYCFG_RECOVERY_KERNEL_VBPRIVK="${key_dir}/recovery_kernel_data_key.vbprivk"
  KEYCFG_INSTALLER_KERNEL_KEYBLOCK="${key_dir}/installer_kernel.keyblock"
  KEYCFG_INSTALLER_KERNEL_V1_KEYBLOCK="${key_dir}/installer_kernel.v1.keyblock"
  KEYCFG_INSTALLER_KERNEL_VBPRIVK="${key_dir}/installer_kernel_data_key.vbprivk"
  KEYCFG_ARV_PLATFORM_KEYBLOCK="${key_dir}/arv_platform.keyblock"
  KEYCFG_ARV_PLATFORM_VBPRIVK="${key_dir}/arv_platform.vbprivk"
  KEYCFG_KEY_EC_EFS_VBRPIK2="${key_dir}/key_ec_efs.vbprik2"
  KEYCFG_ACCESSORY_RWSIG_VBRPIK2=""
  KEYCFG_FIRMWARE_VBPRIVK="${key_dir}/firmware_data_key.vbprivk"
  declare -a KEYCFG_KEY_FIRMARE_VBPRIVK_LOEM
}

setup_keycfg() {
  local key_dir=$1
  setup_default_keycfg "${key_dir}"
  if [ -f "${key_dir}/key_config.sh" ]; then
    . "${key_dir}/key_config.sh"
  fi
}
