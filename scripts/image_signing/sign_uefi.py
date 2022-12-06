#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""
Sign the UEFI binaries in the target directory.
The target directory can be either the root of ESP or /boot of root filesystem.
"""

import argparse
from pathlib import Path
import shutil
import sys
import subprocess
from tempfile import TemporaryDirectory

CROS_LOG_PREFIX = "sign_uefi.py: "
# ANSI color codes used when displaying messages.
V_BOLD_GREEN = "\033[1;32m"
V_BOLD_YELLOW = "\033[1;33m"
V_VIDOFF = "\033[0m"


def info(message):
    """Print an INFO message."""
    print(
        f"{V_BOLD_GREEN}{CROS_LOG_PREFIX}INFO   : {message}{V_VIDOFF}", file=sys.stderr
    )


def warn(message):
    """Print a WARNING message."""
    print(
        f"{V_BOLD_GREEN}{CROS_LOG_PREFIX}WARNING   : {message}{V_VIDOFF}",
        file=sys.stderr,
    )


def ensure_executable_available(name):
    """Exit non-zero if the given executable isn't in $PATH."""
    if not shutil.which(name):
        sys.exit(f"Cannot sign UEFI binaries ({name} not found)")


def ensure_file_exists(path, message):
    """Exit non-zero if the given file doesn't exist."""
    if not path.is_file():
        sys.exit(f"{message}: {path}")


class Signer:
    """EFI file signer."""

    def __init__(self, temp_dir, priv_key, sign_cert, verify_cert):
        self.temp_dir = temp_dir
        self.priv_key = priv_key
        self.sign_cert = sign_cert
        self.verify_cert = verify_cert

    def sign_efi_file(self, target):
        """Sign an EFI binary file, if possible."""
        info(f"Signing efi file {target}")

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
            warn(f"Cannot sign {target}")
            return

        subprocess.run(["sudo", "cp", "--force", signed_file, target], check=True)
        try:
            subprocess.run(["sbverify", "--cert", self.verify_cert, target], check=True)
        except subprocess.CalledProcessError:
            sys.exit("Verification failed")


def sign_target_dir(target_dir, key_dir, efi_glob):
    """Sign various EFI files under `target_dir`."""
    bootloader_dir = target_dir / "efi/boot"
    syslinux_dir = target_dir / "syslinux"
    kernel_dir = target_dir

    verify_cert = key_dir / "db/db.pem"
    ensure_file_exists(verify_cert, "No verification cert")

    sign_cert = key_dir / "db/db.children/db_child.pem"
    ensure_file_exists(sign_cert, "No signing cert")

    sign_key = key_dir / "db/db.children/db_child.rsa"
    ensure_file_exists(sign_key, "No signing key")

    with TemporaryDirectory() as working_dir:
        signer = Signer(Path(working_dir), sign_key, sign_cert, verify_cert)

        for efi_file in sorted(bootloader_dir.glob(efi_glob)):
            if efi_file.is_file():
                signer.sign_efi_file(efi_file)

        for syslinux_kernel_file in sorted(syslinux_dir.glob("vmlinuz.?")):
            if syslinux_kernel_file.is_file():
                signer.sign_efi_file(syslinux_kernel_file)

        kernel_file = (kernel_dir / "vmlinuz").resolve()
        if kernel_file.is_file():
            signer.sign_efi_file(kernel_file)


def main():
    """Sign UEFI binaries."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("target_dir", type=Path)
    parser.add_argument("key_dir", type=Path)
    parser.add_argument("efi_glob")
    args = parser.parse_args()

    ensure_executable_available("sbattach")
    ensure_executable_available("sbsign")
    ensure_executable_available("sbverify")

    sign_target_dir(args.target_dir, args.key_dir, args.efi_glob)


if __name__ == "__main__":
    main()
