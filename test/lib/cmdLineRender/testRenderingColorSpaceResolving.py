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

import os
import shutil

import maya.cmds as cmds
import fixturesUtils
import mtohUtils

from testUtils import PluginLoaded

# The four scenes live alongside the other cmdLineRender renderSettings scenes.
_SCENES_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "scenes", "renderSettings")


def _scenePath(sceneFileName):
    return os.path.join(_SCENES_DIR, sceneFileName)


class TestRenderingColorSpaceResolving(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _initializeStandalone = True
    _setHdStormRenderer = False
    _requiredPlugins = ['mtoa']

    def _runBranch(self, sceneFileName, testCase):
        # Copy the scenes into the build directory test folder.
        tempAppDir = os.environ.get("MAYA_APP_DIR")
        if tempAppDir:
            destDir = os.path.join(tempAppDir, "projects", "default", "scenes")
            if not os.path.isdir(destDir):
                os.makedirs(destDir)
            baseName = os.path.splitext(sceneFileName)[0]
            for ext in (".ma", ".usda"):
                shutil.copy(_scenePath(baseName + ext), os.path.join(destDir, baseName + ext))
            scenePath = os.path.join(destDir, sceneFileName)
        else:
            scenePath = _scenePath(sceneFileName)

        cmds.file(scenePath, open=True, force=True)

        cmds.hydraRender(renderer="HdArnoldRendererPlugin")

        try:
            with PluginLoaded('mayaHydraCppTests'):
                cmds.mayaHydraCppTest(
                    f="TestRenderingColorSpaceResolving.%s" % testCase)
        finally:
            # Release the retained BatchRenderer / Hydra resources kept alive for unit tests.
            cmds.mayaHydraTesting(releaseBatchRenderer=True)

    # renderingColorSpace not authored -> silently fall back to Maya's prefs (ACEScg).
    def test_Unauthored(self):
        self._runBranch("renderingColorSpaceUnauthored.ma", "Unauthored")

    # renderingColorSpace authored and equal to Maya's prefs -> use the authored value, no warning.
    def test_Matching(self):
        self._runBranch("renderingColorSpaceMatching.ma", "Matching")

    # renderingColorSpace authored, known to OCIO, but differs from Maya's prefs -> use the authored value, with warning.
    def test_Differing(self):
        self._runBranch("renderingColorSpaceDiffering.ma", "Differing")

    # renderingColorSpace authored but not known to OCIO -> falls back to Maya's prefs, with warning.
    def test_Unknown(self):
        self._runBranch("renderingColorSpaceUnknown.ma", "Unknown")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
