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

#
# Plugin renderers are registered to the Maya installation twice.
#
# - For the maya executable, renderers are registered with the
#   renderer command.  This is done programmatically, and requires a
#   MEL or Python interpreter to run.  See test registeredMayaRenderers.py.
# - For the Render command line executable, renderers are registered
#   by a renderer description XML file.  This is what is used by Render
#   to list available renderers and to validate the Render command
#   line -r argument (which specifies the renderer to use).
#
# See
# https://help.autodesk.com/view/MAYAUL/2026/ENU/?guid=GUID-AF8A7EA4-DEEF-49EF-A18C-CDA72B4F9E1E
# for more details.
#
# Here we check Render command line executable renderer registration
# for Hydra Storm and Hydra Arnold.

# subprocess is required to launch Maya's Render executable for this CTest
# unit test. Bandit B404 (PYTH-INJC-30) flags any subprocess import as a
# command-injection candidate, but in this script all argv entries are
# supplied by CTest itself (see test/lib/cmdLineRender/CMakeLists.txt) and
# subprocess.run() is called with a list of arguments and without
# shell=True, so the shell is never invoked and no command-injection
# vector exists.
import os
import subprocess  # nosec B404
import sys
from pathlib import Path

if __name__ == "__main__":
    # Validate argv arity up front so a missing argument gives a clear
    # message instead of a downstream IndexError / ValueError. All three
    # arguments are mandatory and supplied by the CTest invocation.
    if len(sys.argv) < 4:
        print(
            "Usage: registeredRenderRenderers.py <RenderPath> <RenderArg> <ExpectedExitCode>",
            file=sys.stderr,
        )
        sys.exit(1)

    RenderPath = sys.argv[1]
    RenderArg  = sys.argv[2]
    try:
        RenderExitCode = int(sys.argv[3])
    except ValueError:
        print(
            "Expected exit code must be an integer, got: %s" % sys.argv[3],
            file=sys.stderr,
        )
        sys.exit(1)

    # Validate the Render executable path before invoking subprocess
    # (Bandit B603 / PYTH-INJC-30). Requiring an absolute path to an
    # existing, executable regular file means subprocess.run() can never
    # be tricked into searching PATH for a same-named binary or into
    # executing a directory / dangling symlink. Combined with the list
    # argument form (no shell=True) below, this makes command injection
    # structurally impossible.
    render_exe = Path(RenderPath)
    if not render_exe.is_absolute():
        print("Render executable path must be absolute: %s" % RenderPath, file=sys.stderr)
        sys.exit(1)
    try:
        render_exe = render_exe.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("Render executable not found: %s (%s)" % (RenderPath, exc), file=sys.stderr)
        sys.exit(1)
    if not render_exe.is_file():
        print("Render executable is not a regular file: %s" % render_exe, file=sys.stderr)
        sys.exit(1)
    if not os.access(str(render_exe), os.X_OK):
        print("Render executable is not executable: %s" % render_exe, file=sys.stderr)
        sys.exit(1)

    # Safe usage of subprocess.run: argument list form (no shell=True), so
    # arguments are passed verbatim to the child process and shell
    # metacharacters cannot trigger command injection (PYTH-INJC-30). The
    # executable is the validated absolute path computed above.
    result = subprocess.run(  # nosec B603
        [str(render_exe), RenderArg],
        capture_output=True,
        text=True,
    )

    if result.returncode != RenderExitCode:
        print("%s %s failed." % (render_exe, RenderArg), file=sys.stderr)
        sys.exit(1)

    # Render places its output into stderr.
    expected = ["HdArnoldRendererPlugin", "HdStormRendererPlugin"]
    for renderer in expected:
        if renderer not in result.stderr:
            print("Renderer %s not found in\n%s" % \
                  (renderer, result.stderr), file=sys.stderr)
            sys.exit(1)
