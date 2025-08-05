#!/bin/bash -eux
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SRC_RUN="$1"
DATA_PATH="${SRC_RUN}/tests/futility/data_copy"

FILE_TMP="${DATA_PATH}/tmptmp"
IMAGE_MAIN="${DATA_PATH}/image.bin"
IMAGE_BAD="${DATA_PATH}/image-bad.bin"
IMAGE_MISSING_FMAP="${DATA_PATH}/image-missing-fmap.bin"
IMAGE_MISSING_FMAP_PATCH="${DATA_PATH}/image-missing-fmap.patch"
IMAGE_MISSING_RO_FRID="${DATA_PATH}/image-missing-ro_frid.bin"
IMAGE_MISSING_RO_FRID_PATCH="${DATA_PATH}/image-missing-ro_frid.patch"
IMAGE_MISSING_RW_FWID="${DATA_PATH}/image-missing-rw_fwid.bin"
IMAGE_MISSING_RW_FWID_PATCH="${DATA_PATH}/image-missing-rw_fwid.patch"
IMAGES_ARCHIVE="${DATA_PATH}/images.zip"
FILE_READ_ONLY="${DATA_PATH}/read-only"

# Apply patch $1 to $2 and write result to $3
apply_patch() {
	local patch="$1"
	local src="$2"
	local dest="$3"

	xxd -p "${src}" "${FILE_TMP}"
	patch "${FILE_TMP}" "${patch}"
	xxd -r -p "${FILE_TMP}" "${dest}"
}

#### test_updater_utils

mkdir -p "${DATA_PATH}"
cp "${SRC_RUN}/tests/futility/data/image-steelix.bin" "${IMAGE_MAIN}"
cp "${SRC_RUN}/tests/futility/data/image-missing-fmap.patch" "${IMAGE_MISSING_FMAP_PATCH}"
cp "${SRC_RUN}/tests/futility/data/image-missing-ro_frid.patch" "${IMAGE_MISSING_RO_FRID_PATCH}"
cp "${SRC_RUN}/tests/futility/data/image-missing-rw_fwid.patch" "${IMAGE_MISSING_RW_FWID_PATCH}"
cp "${SRC_RUN}/tests/futility/data/images.zip" "${IMAGES_ARCHIVE}"

#	Invalid image
dd if=/dev/zero of="${IMAGE_BAD}" bs=1024 count=16

#	Missing FMAP
apply_patch "${IMAGE_MISSING_FMAP_PATCH}" "${IMAGE_MAIN}" "${IMAGE_MISSING_FMAP}"

# 	Missing RO_FRID in FMAP
apply_patch "${IMAGE_MISSING_RO_FRID_PATCH}" "${IMAGE_MAIN}" "${IMAGE_MISSING_RO_FRID}"

#	Missing RW_FWID_A in FMAP
apply_patch "${IMAGE_MISSING_RW_FWID_PATCH}" "${IMAGE_MAIN}" "${IMAGE_MISSING_RW_FWID}"

#	Read-only file
touch "${FILE_READ_ONLY}"
chmod 444 "${FILE_READ_ONLY}"
