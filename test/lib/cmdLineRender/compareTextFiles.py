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

"""Compare two text files, normalizing line endings and sorting lines.

Lines are sorted before comparison so the check is order-independent.
Exit 0 if the files match, 1 if they differ.  On mismatch a unified diff
is printed to stdout so the CTest log contains actionable diagnostics.

Usage:
    python compareTextFiles.py <actual_file> <expected_file>
"""

import difflib
import re
import sys

_ADDR_RE = re.compile(r'_[0-9A-Fa-f]{8,}( --- )')


def _mask_addresses(line):
    """Replace runtime hex addresses in Hydra prim lines with a fixed token."""
    return _ADDR_RE.sub(r'_ADDR\1', line)


def main():
    if len(sys.argv) != 3:
        print("Usage: compareTextFiles.py <actual_file> <expected_file>",
              file=sys.stderr)
        return 2

    actual_path = sys.argv[1]
    expected_path = sys.argv[2]

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

    actual_normalized = sorted(actual_normalized)
    expected_normalized = sorted(expected_normalized)

    if actual_normalized == expected_normalized:
        print("Files match.")
        return 0

    diff = difflib.unified_diff(
        expected_normalized,
        actual_normalized,
        fromfile=expected_path,
        tofile=actual_path,
    )
    sys.stdout.writelines(diff)
    print(f"\nERROR: files differ: {actual_path} vs {expected_path}",
          file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
