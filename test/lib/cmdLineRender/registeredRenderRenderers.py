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

import subprocess
import sys

if __name__ == "__main__":
    # Path to Render executable required.
    if len(sys.argv) == 1:
        print("Missing test arguments.", file=sys.stderr)
        sys.exit(1)

    RenderPath     = sys.argv[1]
    RenderArg      = sys.argv[2]
    RenderExitCode = int(sys.argv[3])

    result = subprocess.run(
        [RenderPath, RenderArg],
        capture_output=True,
        text=True
    )

    if result.returncode != RenderExitCode:
        print("%s %s failed." % (RenderPath, RenderArg), file=sys.stderr)
        sys.exit(1)

    # Render places its output into stderr.
    expected = ["HdArnoldRendererPlugin", "HdStormRendererPlugin"]
    for renderer in expected:
        if renderer not in result.stderr:
            print("Renderer %s not found in\n%s" % \
                  (renderer, result.stderr), file=sys.stderr)
            sys.exit(1)
