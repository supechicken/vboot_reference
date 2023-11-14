#!/usr/bin/env python3
# Copyright 2024 The ChromiumOS Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Tests for generate_android_cert_config.py.

This is run as part of `make runtests`, or `make runtestscripts` if you
want something a little faster.
"""

import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from generate_android_cert_config import _parse_flags
from generate_android_cert_config import _validate
from generate_android_cert_config import KEY_NAMES
from generate_android_cert_config import PKCS11_MODULE_PATH
import pytest
import yaml


PKCS11_TEST_PATH = "test_path"
YAML_FILE_KEY_RING_NAME = "test_key_ring"


class Test(unittest.TestCase):
    """Basic unit test cases for generate_android_cert_config.py"""

    def test_input_args_default(self):
        """Test default input arguments"""
        args = _parse_flags([""])
        self.assertEqual(args.clean, False)
        self.assertEqual(args.key_ring, "")
        self.assertEqual(args.base_config_yaml_file, "")
        self.assertEqual(args.input_dir, os.getcwd())
        self.assertEqual(args.output_dir, os.getcwd())
        self.assertEqual(args.key_names, KEY_NAMES)

    def test_input_args_clean(self):
        """Test clean input arguments to true"""
        args = _parse_flags(["", "--clean"])
        self.assertEqual(args.clean, True)

    def test_validate_missing_pkcs11_module_path(self):
        with pytest.raises(SystemExit) as e:
            input_dir, output_dir, key_ring = _validate(_parse_flags([""]))
            self.assertEqual(key_ring, "test")
        assert e.value.code == 1

    @mock.patch.dict("os.environ", {PKCS11_MODULE_PATH: PKCS11_TEST_PATH})
    def test_validate_missing_key_ring_or_base_config_yaml_file(self):
        with pytest.raises(SystemExit) as e:
            input_dir, output_dir, key_ring = _validate(_parse_flags([""]))
            self.assertEqual(key_ring, "test")
        assert e.value.code == 1

    @mock.patch.dict("os.environ", {PKCS11_MODULE_PATH: PKCS11_TEST_PATH})
    def test_validate_default_with_key_ring(self):
        input_dir, output_dir, key_ring = _validate(
            _parse_flags(["", "-kr=test", "--input_dir=/tmp"])
        )
        self.assertEqual(input_dir, "/tmp/")
        self.assertEqual(input_dir, output_dir)
        self.assertEqual(key_ring, "test")

    @mock.patch.dict("os.environ", {PKCS11_MODULE_PATH: PKCS11_TEST_PATH})
    def test_validate_default_with_base_config_file(self):
        with tempfile.NamedTemporaryFile(mode="w", suffix=".yaml") as temp_file:
            data = {"tokens": [{"key_ring": YAML_FILE_KEY_RING_NAME}]}
            yaml.dump(data, temp_file)
            yaml_file_name = temp_file.name

            input_dir, output_dir, key_ring = _validate(
                _parse_flags(["", "--input_dir=/tmp", f"-b={yaml_file_name}"])
            )
            self.assertEqual(input_dir, "/tmp/")
            self.assertEqual(input_dir, output_dir)
            self.assertEqual(key_ring, YAML_FILE_KEY_RING_NAME)


if __name__ == "__main__":
    unittest.main()
