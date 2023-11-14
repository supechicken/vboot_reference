#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Generates certificate config files to be used by apksigner.

1. Converts x509.pem certificate files to a pkcs#11 config file that can be
passed to apksigner to achieve signing using gcloud CloudHSM. This is needed
to be generated for each key in token ring.
2. Generates base pkcs#11 config file.

Usage: generate_android_cert_config.py [--key_ring <value>
       --base_config_yaml_file <Base config yaml file that contains keyring>
       --input_dir <input directory>
       --output_dir <output directory for config file>
       --key_names <list>
       --clean
       ]
"""

import argparse
import logging
import os
from pathlib import Path
import sys

import yaml


KEY_NAMES = ["media", "networkstack", "platform", "releasekey", "shared"]
CONFIG_FILE_NAME = "pkcs11_java.cfg"
PKCS11_MODULE_PATH = "PKCS11_MODULE_PATH"


def str_presenter(dumper, data):
    if len(data.splitlines()) > 1:  # check for multiline string
        return dumper.represent_scalar("tag:yaml.org,2002:str", data, style="|")
    return dumper.represent_scalar("tag:yaml.org,2002:str", data)


yaml.add_representer(str, str_presenter)

# to use with safe_dump:
yaml.representer.SafeRepresenter.add_representer(str, str_presenter)


def write_yaml_to_file(py_obj, file_name):
    """
    Writes yaml python object to file."""
    with open(f"{file_name}.yaml", "w") as f:
        yaml.dump(py_obj, f, indent=4)


def read_yaml_file(file_name):
    """
    Reads yaml file and returns the python dictionary object."""
    yaml_file = Path(file_name)
    if not yaml_file.is_file():
        logging.error(
            f"Input base config file name {file_name} provided is "
            "not a valid file."
        )
        sys.exit(1)
    with open(file_name, "r") as stream:
        try:
            return yaml.safe_load(stream)
        except yaml.YAMLError as exc:
            logging.error(f"Unable to open yaml file {file_name}.")
            sys.exit(1)


def _parse_flags(argv):
    """The function passed to absl.app.run to parse flags.

    :param argv: A list of input arguments.

    :return parsed input namespace.
    """
    parser = argparse.ArgumentParser(
        description="Generate certificate files to be used for pkcs#11 signing using gcloud."
    )

    parser.add_argument(
        "--key_ring",
        "-kr",
        type=str,
        help="Name of gcloud key-ring",
        default="",
        required=False,
    )
    parser.add_argument(
        "--base_config_yaml_file",
        "-b",
        type=str,
        help="Base pkcs11 config file",
        default="",
        required=False,
    )
    parser.add_argument(
        "--input_dir",
        "-i",
        type=str,
        help="Input directory location to find the certificate "
        "files. This would default to current working "
        "directory if nothing is provided.",
        default=os.getcwd(),
    )
    parser.add_argument(
        "--output_dir",
        "-o",
        type=str,
        help="Output directory location where files will be "
        "generated. This would default to input directory "
        "if nothing is provided.",
        default=os.getcwd(),
    )
    parser.add_argument(
        "--key_names",
        "-kn",
        nargs="+",
        help="Key names we want to run it on, default is a list of "
        "media, networkstack, platform, releasekey, shared",
        default=KEY_NAMES,
    )
    parser.add_argument(
        "--clean",
        "-c",
        action="store_true",
        help="Clean up generated files.",
        # Migrate to below action when we start using Python3.9 in chroot
        # action=argparse.BooleanOptionalAction,
    )
    return parser.parse_args(argv[1:])


def generate_config_file(output_dir: str) -> None:
    """
    Generates a static config file with name, description, library path and
    slotListIndex.
    """
    config_file_name = output_dir + CONFIG_FILE_NAME

    try:
        with open(config_file_name, "w") as file:
            file.write("name = libkmsp11\n")
            file.write("description = Google Cloud KMS PKCS11 Library\n")
            file.write(f"library = {os.getenv('PKCS11_MODULE_PATH')}\n")
            file.write("slotListIndex = 0\n")
    except OSError as ex:
        logging.error("Unable to open create file due to exception: ", ex)
        sys.exit(1)


def generate_yaml_config_file(
    key_names: [str], input_dir: str, output_dir: str, key_ring: str
) -> None:
    """
    Generate YAML config file for each certificate file present.
    """
    logging.debug(f"Iterating over {len(key_names)} keynames")
    for key_name in key_names:
        token = {"key_ring": key_ring}
        with open(input_dir + key_name + ".x509.pem", "r") as f:
            cert = f.read()
            token["certs"] = [cert]
        tokens = {"tokens": [token]}

        write_yaml_to_file(tokens, output_dir + key_name + "_config")


def _validate(args):
    lib_path = os.getenv(PKCS11_MODULE_PATH)
    if not lib_path:
        logging.error("Please set PKCS11_MODULE_PATH before continuing.")
        sys.exit(1)

    if not args.base_config_yaml_file and not args.key_ring:
        logging.error("Either base config yaml file or key ring is required.")
        sys.exit(1)

    key_ring = args.key_ring

    if not args.key_ring:
        # Get the value from base_config_yaml file.
        yaml_config = read_yaml_file(args.base_config_yaml_file)
        key_ring = yaml_config["tokens"][0]["key_ring"]

    input_dir = str(args.input_dir)

    if not input_dir.endswith("/"):
        input_dir = input_dir + "/"

    output_dir = (
        input_dir if args.output_dir == os.getcwd() else args.output_dir
    )

    if not output_dir.endswith("/"):
        output_dir = str(output_dir) + "/"

    if output_dir is input_dir:
        logging.debug(
            "Going to output config files in the same location "
            "as provided input directory."
        )

    return input_dir, output_dir, key_ring


def cleanup(output_dir, key_names: [str]):
    logging.info("Cleaning up generated certificate files.")

    config_file_name = output_dir + CONFIG_FILE_NAME
    if os.path.isfile(config_file_name):
        logging.warning("Removing " + config_file_name)
        os.remove(config_file_name)

    for key_name in key_names:
        yaml_file_name = output_dir + key_name + "_config.yaml"
        if os.path.isfile(yaml_file_name):
            logging.warning("Removing " + yaml_file_name)
            os.remove(yaml_file_name)


def main(argv) -> None:
    args = _parse_flags(argv)
    input_dir, output_dir, key_ring = _validate(args)

    if args.clean:
        cleanup(output_dir, key_names=args.key_names)
        return

    # Generate the pkcs11 config file.
    generate_config_file(output_dir=output_dir)
    generate_yaml_config_file(
        key_names=args.key_names,
        input_dir=input_dir,
        output_dir=output_dir,
        key_ring=key_ring,
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv))
