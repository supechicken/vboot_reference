#!/bin/bash -eux
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SRC_RUN="$1"
DATA_PATH="${SRC_RUN}/tests/futility/data_copy"

FILE_TMP="${DATA_PATH}/tmptmp"
IMAGE_MAIN="${DATA_PATH}/image-newer.bin"
IMAGE_BAD="${DATA_PATH}/image-bad.bin"
IMAGE_MISSING_FMAP="${DATA_PATH}/image-missing-fmap.bin"
IMAGE_MISSING_RO_FRID="${DATA_PATH}/image-missing-ro_frid.bin"
IMAGE_MISSING_RW_FWID="${DATA_PATH}/image-missing-rw_fwid.bin"
FILE_READ_ONLY="${DATA_PATH}/read-only"

#### test_updater_utils

mkdir -p "${DATA_PATH}"
cp "${SRC_RUN}/tests/futility/data/image-newer.bin" "${DATA_PATH}"

#	Zip must contain 'tests/futility/data_copy/image-newer.bin'
pushd "${SRC_RUN}"
zip tests/futility/data_copy/images.zip tests/futility/data_copy/image-newer.bin
popd

#	Invalid image
dd if=/dev/zero of="${IMAGE_BAD}" bs=1024 count=16

#	Missing FMAP
cp "${IMAGE_MAIN}" "${IMAGE_MISSING_FMAP}"
futility load_fmap "${IMAGE_MISSING_FMAP}" FMAP:/dev/zero

# 	Missing RO_FRID in FMAP
cp "${IMAGE_MAIN}" "${IMAGE_MISSING_RO_FRID}"
futility dump_fmap -x "${IMAGE_MISSING_RO_FRID}" FMAP:"${FILE_TMP}"
sed -i -e 's/RO_FRID/XX_XXXX/g' "${FILE_TMP}"
futility load_fmap "${IMAGE_MISSING_RO_FRID}" FMAP:"${FILE_TMP}"

#	Missing RW_FWID_A in FMAP
cp "${IMAGE_MAIN}" "${IMAGE_MISSING_RW_FWID}"
futility dump_fmap -x "${IMAGE_MISSING_RW_FWID}" FMAP:"${FILE_TMP}"
sed -i -e 's/RW_FWID_A/XX_XXXX_X/g' "${FILE_TMP}"
futility load_fmap "${IMAGE_MISSING_RW_FWID}" FMAP:"${FILE_TMP}"

#	Read-only file
touch "${FILE_READ_ONLY}"
chmod 444 "${FILE_READ_ONLY}"
