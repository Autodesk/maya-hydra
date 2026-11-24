# Copyright 2026 Autodesk
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

# HYDRA-2726 : the path mapper of the default render description node is shared by every scene
# index registry: one per render override, plus one per batch render. A batch render used to
# remove the render override's mapping when it ended.

import os
import shutil
import tempfile

import maya.cmds as cmds

import fixturesUtils
import mtohUtils
import testUtils

from testUtils import PluginLoaded

class TestRenderDescriptionPathMapper(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def assertMapperRegistered(self):
        cmds.mayaHydraCppTest(f="TestPathMapperRegistry.renderDescriptionMapperRegistered")

    def test_batchRenderKeepsViewportMapping(self):
        if not hasattr(cmds, "hydraRender"):
            self.skipTest("Needs the hydraRender command (Maya 2027 and later).")

        # PluginLoaded's exit does a File New: the whole test runs inside it.
        with PluginLoaded('mayaHydraCppTests'):
            # A scene with a default render description node, its stage stored in the file. The
            # render product is relative to the scene, so open a copy outside the source tree.
            sceneName = 'sphereWithRenderDescription.ma'
            tempDir = tempfile.mkdtemp(prefix='testRenderDescriptionPathMapper_')
            self.addCleanup(shutil.rmtree, tempDir, ignore_errors=True)
            scenePath = os.path.join(tempDir, sceneName)
            shutil.copy(testUtils.getTestScene('testRenderDescriptionPathMapper', sceneName), scenePath)
            cmds.file(scenePath, open=True, force=True)
            self.setTestRenderer()
            cmds.refresh(force=True)
            self.assertMapperRegistered()

            # The batch render's registry shares the registration, and releases it with the
            # batch renderer: at the end of the command, or here in batch render test mode
            # (MAYA_HYDRA_BATCH_RENDER_TEST_MODE), which keeps it alive.
            cmds.hydraRender(renderer="HdStormRendererPlugin")
            cmds.mayaHydraTesting(releaseBatchRenderer=True)
            cmds.refresh(force=True)
            self.assertMapperRegistered()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
