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

"""Compare one rendered image to its baseline via imageDiffUtils.compareImagePair().

Thin argv wrapper for single-image and mayabatch cmdLineRender tests:
replaces the shell's direct `idiff ...` compare leg so failures get the
same Baseline:/Actual:/Diff: report (with Jenkins artifact URLs) that
viewport tests already produce. passReturnCodes=(0,) matches today's shell
`exit $LASTEXITCODE` exactly: only an exact idiff match passes, warning
(rc 1) and error (rc >= 2) both fail.

Usage:
    compareRenderedImage.py <idiff> <BaselineImage> <ActualImage> <Fail> <FailPercent>
"""

import sys

import imageDiffUtils


def main(argv):
    if len(argv) != 6:
        print(
            "Usage: compareRenderedImage.py <idiff> <BaselineImage> "
            "<ActualImage> <Fail> <FailPercent>",
            file=sys.stderr,
        )
        return 2

    idiff = imageDiffUtils.validate_executable("idiff executable", argv[1])
    baseline = imageDiffUtils.validate_existing_file("Baseline image", argv[2])
    actual = imageDiffUtils.validate_existing_file("Actual image", argv[3])
    fail = imageDiffUtils.validate_float("fail threshold", argv[4])
    failpercent = imageDiffUtils.validate_float("failpercent threshold", argv[5])

    result = imageDiffUtils.compareImagePair(
        str(baseline), str(actual), fail, failpercent,
        imageDiffTool=str(idiff), passReturnCodes=(0,), verbose=True,
    )
    if not result.passed:
        print(result.message, file=sys.stderr)
    return 0 if result.passed else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
