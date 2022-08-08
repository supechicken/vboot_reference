#!/usr/bin/env python3
# Copyright 2022 The ChromiumOS Authors.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Run security tests on a ChromeOS image"""

import argparse
import path
import os
import subprocess
import sys

def execTest(name, image, args):
  """Runs a given script

  Args:
    name: the name of the script to execute
    image: the input image
    args: list of additional arguments for the script
  """
  # Ensure this script can execute from any directory
  cmd_path = os.path.join(os.path.dirname(__file__), f'{name}.sh')

  cmd = [cmd_path, image] + args
  ret = subprocess.run(cmd)
  if ret.returncode != 0:
    sys.exit(ret.returncode)

def main():
  """Main function, parses arguments and invokes the relevant scripts"""
  parser = argparse.ArgumentParser(description=sys.modules[__name__].__doc__)
  parser.add_argument(
    '--config',
    '-c',
    help='Security test baseline config directory',
    required=True,
    type=path.Path
  )

  parser.add_argument(
    '--image',
    '-i',
    help='ChromeOS image to test',
    required=True,
    type=path.Path,
  )

  parser.add_argument(
    '--keyset-is-mp',
    action='store_true',
    help='Target image is signed with a mass production keyset',
    default=False,
  )

  args = parser.parse_args()

  # Run generic baseline tests.
  baseline_tests = [
    'ensure_sane_lsb-release'
  ]

  if args.keyset_is_mp:
    baseline_tests += [
      'ensure_no_nonrelease_files',
      'ensure_secure_kernelparams'
    ]

  for test in baseline_tests:
    execTest(test, args.image, [os.path.join(args.config, f'{test}.config')])

  # Run generic non-baseline tests.
  tests = []

  if args.keyset_is_mp:
    tests += [
      'ensure_not_ASAN',
      'ensure_not_tainted_license',
      'ensure_update_verification'
    ]

  for test in tests:
    execTest(test, args.image, [])

if __name__ == '__main__':
  main()