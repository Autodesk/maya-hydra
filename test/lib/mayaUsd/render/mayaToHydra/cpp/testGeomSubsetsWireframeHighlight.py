# Copyright 2024 Autodesk
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
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import testUtils
import usdUtils

from testUtils import PluginLoaded

class TestGeomSubsetsWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testGeomSubsetsPicking', 'GeomSubsetsPickingTestScene.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestGeomSubsetsWireframeHighlight, self).setUp()
        self.loadUsdScene()
        cmds.select(clear=True)
        cmds.optionVar(
                sv=('mayaHydra_GeomSubsetsPickMode', 'Faces'))
        # Used to ensure the fallback picking behavior works when clicking on a non-GeomSubset
        cmds.optionVar(
                sv=('mayaUsd_PointInstancesPickMode', 'Prototypes'))
        cmds.setAttr('persp.translate', 0, 0, 15, type='float3')
        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        cmds.refresh()

    def test_InstancedGeomSubsetHighlight(self):
        self.runCppTest("GeomSubsetsWireframeHighlight.instancedGeomSubsetHighlight")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
