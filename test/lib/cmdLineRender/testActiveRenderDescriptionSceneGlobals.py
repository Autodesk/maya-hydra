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

import os
import shutil

import maya.cmds as cmds
import fixturesUtils
import mtohUtils

from testUtils import PluginLoaded


_SCENES_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "scenes", "renderSettings")


def _scenePath(sceneFileName):
    return os.path.join(_SCENES_DIR, sceneFileName)


class TestActiveRenderDescriptionSceneGlobals(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _initializeStandalone = True
    _setHdStormRenderer = False
    _requiredPlugins = ["mtoa"]

    def _runBranch(self, sceneFileName, testCase):
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
            with PluginLoaded("mayaHydraCppTests"):
                cmds.mayaHydraCppTest(
                    f="TestActiveRenderDescriptionSceneGlobals.%s" % testCase)
        finally:
            cmds.mayaHydraTesting(releaseBatchRenderer=True)

    def test_RenderSettings(self):
        self._runBranch("activeRenderDescriptionRenderSettings.ma", "RenderSettings")

    def test_ValidRenderPass(self):
        self._runBranch("activeRenderDescriptionValidPass.ma", "ValidRenderPass")

    def test_InvalidRenderPassSource(self):
        self._runBranch(
            "activeRenderDescriptionInvalidPassSource.ma", "InvalidRenderPassSource")

    def test_MissingRenderPass(self):
        self._runBranch("activeRenderDescriptionMissingPass.ma", "MissingRenderPass")


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
