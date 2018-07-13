#!/bin/bash
# Copyright 2017 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

. "$(dirname "$0")/common.sh"

load_shflags || exit 1

DEFINE_string output "" \
  "Where to write signed output to (default: sign in-place)"

FLAGS_HELP="Usage: ${PROG} [options] <input_image> <key_dir> <image_type>

Signs <input_image> with keys in <key_dir>. Should have an imageloader.json
file which imageloader can understand and will use to mount the squashfs
image contained within the input image.

<image_type> is the type of the image that is being signed:
  * oci-container - The squashfs image that that provides an OCI container's
    rootfs and OCI configuration.
  * demo-mode-resources - The squashfs image that provides pre-installed
    resources to be used in demo mode sessions.

Input can be an unpacked imageloader image, a CRX/ZIP file, or a tar.gz file.
"

# Parse command line.
FLAGS "$@" || exit 1
eval set -- "${FLAGS_ARGV}"

# Abort on error.
set -e

# Sign the directory holding OCI container(s).  We look for an imageloader.json
# file.
sign_imageloader_image() {
  [[ $# -eq 4 ]] || \
      die "Usage: sign_imageloader_image <input> <key> <suffix> <output>"
  local input="${1%/}"
  local key_file="$2"
  local sig_suffix="$3"
  local output="$4"

  if [[ "${input}" != "${output}" ]]; then
    rsync -a "${input}/" "${output}/"
  fi

  local manifest out_manifest
  while read -d $'\0' -r manifest; do
    out_manifest="${output}/${manifest%.json}.sig.${sig_suffix}"
    manifest="${input}/${manifest}"
    info "Signing: ${manifest}"
    if ! openssl dgst -sha256 -sign "${key_file}" \
                      -out "${out_manifest}" "${manifest}"; then
      die "Failed to sign"
    fi
  done < <(find "${input}/" -name imageloader.json -printf '%P\0')
}

# Sign the crx/zip holding an imageloader image.  We look for an
# imageloader.json file.
sign_imageloader_image_zip() {
  [[ $# -eq 4 ]] || \
      die "Usage: sign_imageloader_image <input> <key> <suffix> <output>"
  local input="$1"
  local key_file="$2"
  local sig_suffix="$3"
  local output="$4"
  local tempdir=$(make_temp_dir)

  info "Unpacking archive: ${input}"
  unzip -q "${input}" -d "${tempdir}"

  sign_imageloader_image "${tempdir}" "${key_file}" "${sig_suffix}" "${tempdir}"

  rm -f "${output}"
  info "Packing archive: ${output}"
  (
    cd "${tempdir}"
    zip -q -r - ./
  ) >"${output}"
}

# Sign the tar holding an imageloader image. We look for imageloader.json file.
sign_oci_container_tar() {
  [[ $# -eq 4 ]] || \
      die "Usage: sign_imageloader_image <input> <key> <suffix> <output>"
  local input="$1"
  local key_file="$2"
  local sig_suffix="$3"
  local output="$4"
  local tempdir=$(make_temp_dir)

  info "Unpacking tar archive: ${input}"
  tar -xzf "${input}" -C "${tempdir}"

  sign_imageloader_image "${tempdir}" "${key_file}" "${sig_suffix}" "${tempdir}"

  rm -f "${output}"
  info "Packing tar archive:  ${output}"
  tar -czf "${output}" -C "${tempdir}" .
}

main() {
  if [[ $# -ne 3 ]]; then
    flags_help
    exit 1
  fi

  local input="${1%/}"
  local key_dir="$2"
  local image_type="$3"

  local key_file sig_suffix
  if [[ "${image_type}" == "oci-container"  ]]; then
    key_file="${key_dir}/cros-oci-container.pem"
    sig_suffix="2"
  elif [[ "${image_type}" == "demo-mode-resources" ]]; then
    key_file="${key_dir}/demo-mode-resources.pem"
    sig_suffix="3"
  else
    flags_help
    exit 1
  fi

  if [[ ! -e "${key_file}" ]]; then
    die "Missing key file: ${key_file}"
  fi

  : "${FLAGS_output:=${input}}"

  local sign_function
  if [[ -f "${input}" ]]; then
    filetype=$(file -b "${input}" | cut -d " " -f 1)
    if [[ "${filetype}" == "gzip" ]]; then
      sign_function=sign_imageloader_image_tar
    else
      sign_function=sign_imageloader_image_zip
    fi
  else
    sign_function=sign_imageloader_image
  fi

  "${sign_function}" "${input}" "${key_file}" "${sig_suffix}" \
          "${FLAGS_output}"
}
main "$@"
