#!/bin/bash -eux
# Copyright 2025 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

SRC_RUN="$1"
CWD="$1"

mkdir -p "${SRC_RUN}"/tests/futility/data_copy
cp "${SRC_RUN}"/tests/futility/data/image-newer.bin "${SRC_RUN}"/tests/futility/data_copy/

#	Zip must contain 'tests/futility/data_copy/image-newer.bin'
cd "${SRC_RUN}"
zip tests/futility/data_copy/images.zip tests/futility/data_copy/image-newer.bin
cd "${CWD}"

#	Random noise
dd if=/dev/zero of="${SRC_RUN}"/tests/futility/data_copy/image-bad.bin bs=512 count=65536

#	Missing FMAP
cp "${SRC_RUN}"/tests/futility/data_copy/image-newer.bin \
	"${SRC_RUN}"/tests/futility/data_copy/image-missing-fmap.bin
dd if=/dev/zero of="${SRC_RUN}"/tests/futility/data_copy/image-missing-fmap.bin \
	bs=1 count=2048 seek=25182208 conv=notrunc

# 	Missing RO_FRID in FMAP
cp "${SRC_RUN}"/tests/futility/data_copy/image-newer.bin \
	"${SRC_RUN}"/tests/futility/data_copy/image-missing-ro_frid.bin
dd if=/dev/zero of="${SRC_RUN}"/tests/futility/data_copy/image-missing-ro_frid.bin \
	bs=1 count=7 seek=25183700 conv=notrunc

#	Missing RW_FRID_A in FMAP
cp "${SRC_RUN}"/tests/futility/data_copy/image-newer.bin \
	"${SRC_RUN}"/tests/futility/data_copy/image-missing-rw_fwid.bin
dd if=/dev/zero of="${SRC_RUN}"/tests/futility/data_copy/image-missing-rw_fwid.bin \
	bs=1 count=9 seek=25182734 conv=notrunc
