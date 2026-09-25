# Copyright 2025 Autodesk
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
import unittest

import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded

class TestMayaHydraSceneIndex(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupScene(self):
        self.setHdStormRenderer()

        cmds.group(empty=True, name="group1")
        cmds.group(empty=True, name="group2", parent="group1")
        cmds.polyCube(name="leafShape", constructionHistory=False)
        cmds.parent("leafShape", "group2")
        cmds.refresh()

    def test_PrimAncestors(self):
        """Test _AddPrimAncestors and _RemoveEmptyAncestors functionality"""
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaHydraSceneIndex.PrimAncestors")

    # Coverage builds deliberately leak the scene index and its filtering chain
    # (CODE_COVERAGE_WORKAROUND), so the old scene index can never expire there.
    @unittest.skipIf(os.environ.get("MAYAHYDRA_CODE_COVERAGE"),
                     "Coverage builds leak the scene index on purpose (CODE_COVERAGE_WORKAROUND)")
    def test_ReleasedOnHydraRebuild(self):
        """The scene index must be destroyed when Hydra resources are rebuilt (HYDRA-2019)"""
        self.setHdStormRenderer()
        cmds.polySphere(name="sphere1", subdivisionsX=8, subdivisionsY=8)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaHydraSceneIndex.ReleasedOnHydraRebuild")

if __name__ == '__main__':
    fixturesUtils.runTests(globals()) 