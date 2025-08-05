#!/bin/bash -eux
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SRC_RUN="$1"
DATA_PATH="${SRC_RUN}/tests/futility/data_copy"

#### test_updater_utils

mkdir -p "${DATA_PATH}"
cp "${SRC_RUN}/tests/futility/data/image-newer.bin" "${DATA_PATH}"

#	Zip must contain 'tests/futility/data_copy/image-newer.bin'
pushd "${SRC_RUN}"
zip tests/futility/data_copy/images.zip tests/futility/data_copy/image-newer.bin
popd

#	Invalid image
dd if=/dev/random of="${DATA_PATH}/image-bad.bin" bs=1024 count=16

#	Missing FMAP
cp "${DATA_PATH}/image-newer.bin" \
	"${DATA_PATH}/image-missing-fmap.bin"
futility load_fmap "${DATA_PATH}/image-missing-fmap.bin" FMAP:/dev/zero

# 	Missing RO_FRID in FMAP
cp "${DATA_PATH}/image-newer.bin" \
	"${DATA_PATH}/image-missing-ro_frid.bin"
futility dump_fmap -x "${DATA_PATH}/image-missing-ro_frid.bin" FMAP:"${DATA_PATH}/tests_tmp.bin"
sed -i -e 's/RO_FRID/XX_XXXX/g' "${DATA_PATH}/tests_tmp.bin"
futility load_fmap "${DATA_PATH}/image-missing-ro_frid.bin" FMAP:"${DATA_PATH}/tests_tmp.bin"

#	Missing RW_FWID_A in FMAP
cp "${DATA_PATH}/image-newer.bin" \
	"${DATA_PATH}/image-missing-rw_fwid.bin"
futility dump_fmap -x "${DATA_PATH}/image-missing-rw_fwid.bin" FMAP:"${DATA_PATH}/tests_tmp.bin"
sed -i -e 's/RW_FWID_A/XX_XXXX_X/g' "${DATA_PATH}/tests_tmp.bin"
futility load_fmap "${DATA_PATH}/image-missing-rw_fwid.bin" FMAP:"${DATA_PATH}/tests_tmp.bin"
