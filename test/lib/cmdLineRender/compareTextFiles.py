# Copyright 2026 Autodesk, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

"""Compare two text files, normalizing line endings.

Exit 0 if the files match, 1 if they differ.  On mismatch a unified diff
is printed to stdout so the CTest log contains actionable diagnostics.

With --lines, only that many leading lines of each file are compared.

Usage:
    python compareTextFiles.py <actual_file> <expected_file> [--lines N]
"""

import argparse
import difflib
import re
import sys

_ADDR_RE = re.compile(r'_[0-9A-Fa-f]{8,}( --- )')


def _mask_addresses(line):
    """Replace runtime hex addresses in Hydra prim lines with a fixed token."""
    return _ADDR_RE.sub(r'_ADDR\1', line)


def main():
    parser = argparse.ArgumentParser(
        description="Compare two text files, normalizing line endings.")
    parser.add_argument("actual_file")
    parser.add_argument("expected_file")
    parser.add_argument("--lines", type=int, default=0, metavar="N",
                        help="Compare only the first N lines of each file.  "
                             "The default, 0, compares the whole file.")
    args = parser.parse_args()

    actual_path = args.actual_file
    expected_path = args.expected_file
    num_lines = args.lines

    try:
        with open(actual_path, "r", newline="") as f:
            actual_lines = f.read().splitlines(keepends=True)
    except FileNotFoundError:
        print(f"ERROR: actual file not found: {actual_path}", file=sys.stderr)
        return 1

    try:
        with open(expected_path, "r", newline="") as f:
            expected_lines = f.read().splitlines(keepends=True)
    except FileNotFoundError:
        print(f"ERROR: expected file not found: {expected_path}",
              file=sys.stderr)
        return 1

    # Normalize line endings and mask runtime hex addresses in prim lines.
    actual_normalized = [_mask_addresses(line.rstrip("\r\n") + "\n")
                         for line in actual_lines]
    expected_normalized = [_mask_addresses(line.rstrip("\r\n") + "\n")
                           for line in expected_lines]

    # Comparing a file shorter than the requested number of lines would
    # silently compare fewer lines than asked for, making the test vacuous.
    scope = ""
    if num_lines > 0:
        if len(expected_normalized) < num_lines:
            print(f"ERROR: expected file has fewer than {num_lines} lines: "
                  f"{expected_path}", file=sys.stderr)
            return 2
        if len(actual_normalized) < num_lines:
            print(f"ERROR: actual file has fewer than {num_lines} lines: "
                  f"{actual_path}", file=sys.stderr)
            return 1
        actual_normalized = actual_normalized[:num_lines]
        expected_normalized = expected_normalized[:num_lines]
        scope = f" (first {num_lines} lines)"

    if actual_normalized == expected_normalized:
        print("Files match.")
        return 0

    diff = difflib.unified_diff(
        expected_normalized,
        actual_normalized,
        fromfile=expected_path + scope,
        tofile=actual_path + scope,
    )
    sys.stdout.writelines(diff)
    print(f"\nERROR: files differ{scope}: {actual_path} vs {expected_path}",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
