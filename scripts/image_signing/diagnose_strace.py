#!/usr/bin/python3
#
# Copyright 2022 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import re
import typing
import logging


class Line(typing.NamedTuple):
  pid: int
  syscall: typing.Optional[str] # None for exit line
  result: typing.Optional[int] # None for unfinished line
  line: str # Original line


def parse(line: str) -> typing.Optional[Line]:
  """Parse lines from strace log."""

  m = re.search(r'^(\d+)\s+(\w+).+ = (-?\d+)', line)
  if m:
    return Line(int(m[1]), m[2], int(m[3]), line)

  m = re.search(r'^(\d+)\s+(\w+).+<unfinished ...>$', line)
  if m:
    return Line(int(m[1]), m[2], None, line)

  m = re.search(r'^(\d+)\s+<... (\w+) resumed>.+ = (-?\d+)', line)
  if m:
    return Line(int(m[1]), m[2], m[3], line)

  m = re.search(r'^(\d+)\s+\+\+\+', line)
  if m:
    return Line(int(m[1]), None, None, line)

  logging.warning('Unexpected pattern %s', line.rstrip())
  return None


def main():
  parser = argparse.ArgumentParser()
  parser.add_argument('log_file')
  args = parser.parse_args()

  with open(args.log_file) as f:
    lines = list(filter(bool, map(parse, f.readlines())))

  # Print syscalls which meet one of the following criteria.
  # a. Contains 'dalvik-cache' (the directory name is choosed because it's
  #    ofthen removed unintentionally when image corruption happens and it has
  #    unique name)
  # b. Forks of a process chain to the process invoking a).
  # c. Execs for the PIDs invoking a) and b).
  # d. Exit code for the PIDs invoking a) and b).

  marked_pids = set()
  marked_logs = set()
  for l in reversed(lines):
    if 'dalvik-cache' in l.line:
      marked_logs.add(l)
      if l.pid:
        marked_pids.add(l.pid)
    if l.result in marked_pids and l.syscall in ('clone', 'fork', 'vfork'):
      marked_logs.add(l)
      if l.pid:
        marked_pids.add(l.pid)

  for l in lines:
    if l in marked_logs or l.pid in marked_pids and l.syscall in ('execve', 'execveat', None):
      print(l.line, end='')

if __name__ == '__main__':
  main()
