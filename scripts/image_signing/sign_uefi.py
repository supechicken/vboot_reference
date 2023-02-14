#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Sign the UEFI binaries in the target directory.

The target directory can be either the root of ESP or /boot of root filesystem.
"""

import argparse
import csv
import logging
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
from typing import List, Optional


def ensure_executable_available(name):
    """Exit non-zero if the given executable isn't in $PATH.

    Args:
        name: An executable's file name.
    """
    if not shutil.which(name):
        sys.exit(f"Cannot sign UEFI binaries ({name} not found)")


def ensure_file_exists(path, message):
    """Exit non-zero if the given file doesn't exist.

    Args:
        path: Path to a file.
        message: Error message that will be printed if the file doesn't exist.
    """
    if not path.is_file():
        sys.exit(f"{message}: {path}")


class Signer:
    """EFI file signer.

    Attributes:
        temp_dir: Path of a temporary directory used as a workspace.
        priv_key: Path of the private key.
        sign_cert: Path of the signing certificate.
        verify_cert: Path of the certificate used to verify the signature.
    """

    def __init__(self, temp_dir, priv_key, sign_cert, verify_cert):
        self.temp_dir = temp_dir
        self.priv_key = priv_key
        self.sign_cert = sign_cert
        self.verify_cert = verify_cert

    def sign_efi_file(self, target):
        """Sign an EFI binary file, if possible.

        Args:
            target: Path of the file to sign.
        """
        logging.info("signing efi file %s", target)

        # Allow this to fail, as there maybe no current signature.
        subprocess.run(["sudo", "sbattach", "--remove", target], check=False)

        signed_file = self.temp_dir / target.name
        try:
            subprocess.run(
                [
                    "sbsign",
                    "--key",
                    self.priv_key,
                    "--cert",
                    self.sign_cert,
                    "--output",
                    signed_file,
                    target,
                ],
                check=True,
            )
        except subprocess.CalledProcessError:
            logging.warning("cannot sign %s", target)
            return

        subprocess.run(
            ["sudo", "cp", "--force", signed_file, target], check=True
        )
        try:
            subprocess.run(
                ["sbverify", "--cert", self.verify_cert, target], check=True
            )
        except subprocess.CalledProcessError:
            sys.exit("Verification failed")


def read_sbat_section(efi_file, temp_dir):
    """Read the SBAT section from a UEFI file into a file.

    If the UEFI file doesn't have a ".sbat" section, an output file will
    be created.

    Args:
        efi_file: Path of a UEFI file.
        temp_dir: Path of a temporary directory.

    Returns:
        Path of a file in |temp_dir| containing the SBAT data.
    """
    section_name = ".sbat"
    sbat_path = temp_dir / "sbat.csv"
    try:
        # Pass in a temporary destination file for objcopy even though
        # we don't use the copy, otherwise objcopy will print
        # permissions warnings.
        temp_efi_file = temp_dir / f"{efi_file.name}.tmp"

        subprocess.run(
            [
                "objcopy",
                f"--dump-section={section_name}={sbat_path}",
                efi_file,
                temp_efi_file,
            ],
            check=True,
        )
    except subprocess.CalledProcessError:
        # Write an empty file on failure.
        sbat_path.touch()
    return sbat_path


def is_crdyboot_file(efi_file, temp_dir):
    """Check if a UEFI file is a build of the crdyboot bootloader.

    The check is done by reading the file's SBAT data and comparing the
    component name against "crdyboot". For more details of the SBAT
    format, see https://github.com/rhboot/shim/blob/main/SBAT.md.

    Args:
        efi_file: Path of a UEFI file.
        temp_dir: Path of a temporary directory.

    Returns:
        True if the file is a crdyboot build, False otherwise.
    """
    sbat_path = read_sbat_section(efi_file, temp_dir)

    with open(sbat_path) as sbat_file:
        rows = list(csv.reader(sbat_file))
        try:
            # First line is the SBAT component, second line is the
            # crdyboot component.
            row = rows[1]
            # First element of each line is the component name.
            component = row[0]
            return component == "crdyboot"
        except IndexError:
            return False


def inject_vbpubk(efi_file, key_dir, temp_dir):
    """Update a UEFI executable's vbpubk section.

    The crdyboot bootloader contains an embedded public key in the
    ".vbpubk" section. This function replaces the data in the existing
    section (normally containing a dev key) with the real key.

    Args:
        efi_file: Path of a UEFI file.
        key_dir: Path of the UEFI key directory.
        temp_dir: Path of a temporary directory.
    """
    section_name = ".vbpubk"

    # Write to a location that doesn't require root permissions.
    temp_efi_file = temp_dir / f"{efi_file.name}.injected"

    # Update the section.
    logging.info("adding new section %s to %s", section_name, efi_file.name)
    section_data_path = key_dir / "../kernel_subkey.vbpubk"
    subprocess.run(
        [
            "objcopy",
            "--update-section",
            f"{section_name}={section_data_path}",
            efi_file,
            temp_efi_file,
        ],
        check=True,
    )

    # Copy the modified file back to the original location.
    subprocess.run(["sudo", "cp", temp_efi_file, efi_file], check=True)


def sign_target_dir(target_dir, key_dir, efi_glob):
    """Sign various EFI files under |target_dir|.

    Args:
        target_dir: Path of a boot directory. This can be either the
            root of the ESP or /boot of the root filesystem.
        key_dir: Path of a directory containing the key and cert files.
        efi_glob: Glob pattern of EFI files to sign, e.g. "*.efi".
    """
    bootloader_dir = target_dir / "efi/boot"
    syslinux_dir = target_dir / "syslinux"
    kernel_dir = target_dir

    verify_cert = key_dir / "db/db.pem"
    ensure_file_exists(verify_cert, "No verification cert")

    sign_cert = key_dir / "db/db.children/db_child.pem"
    ensure_file_exists(sign_cert, "No signing cert")

    sign_key = key_dir / "db/db.children/db_child.rsa"
    ensure_file_exists(sign_key, "No signing key")

    crdyboot_file_names = [
        # Names if built as the first-stage boot loader.
        "bootia32.efi",
        "bootx64.efi",
        # Names if built as the second-stage boot loader.
        "crdybootia32.efi",
        "crdybootx64.efi",
    ]

    with tempfile.TemporaryDirectory() as working_dir:
        working_dir = Path(working_dir)
        signer = Signer(working_dir, sign_key, sign_cert, verify_cert)

        for efi_file in sorted(bootloader_dir.glob(efi_glob)):
            if efi_file.is_file():
                signer.sign_efi_file(efi_file)

        for name in crdyboot_file_names:
            efi_file = bootloader_dir / name
            if is_crdyboot_file(efi_file, working_dir):
                inject_vbpubk(efi_file, key_dir, working_dir)
                signer.sign_efi_file(efi_file)

        for syslinux_kernel_file in sorted(syslinux_dir.glob("vmlinuz.?")):
            if syslinux_kernel_file.is_file():
                signer.sign_efi_file(syslinux_kernel_file)

        kernel_file = (kernel_dir / "vmlinuz").resolve()
        if kernel_file.is_file():
            signer.sign_efi_file(kernel_file)


def get_parser() -> argparse.ArgumentParser:
    """Get CLI parser."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "target_dir",
        type=Path,
        help="Path of a boot directory, either the root of the ESP or "
        "/boot of the root filesystem",
    )
    parser.add_argument(
        "key_dir",
        type=Path,
        help="Path of a directory containing the key and cert files",
    )
    parser.add_argument(
        "efi_glob", help="Glob pattern of EFI files to sign, e.g. '*.efi'"
    )
    return parser


def main(argv: Optional[List[str]] = None) -> Optional[int]:
    """Sign UEFI binaries.

    Args:
        argv: Command-line arguments.
    """
    logging.basicConfig(level=logging.INFO)

    parser = get_parser()
    opts = parser.parse_args(argv)

    for tool in (
        "objcopy",
        "sbattach",
        "sbsign",
        "sbverify",
    ):
        ensure_executable_available(tool)

    sign_target_dir(opts.target_dir, opts.key_dir, opts.efi_glob)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
