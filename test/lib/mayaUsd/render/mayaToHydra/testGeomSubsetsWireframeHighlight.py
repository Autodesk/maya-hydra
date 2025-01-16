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
import ufe
import usdUtils

from testUtils import PluginLoaded

class TestGeomSubsetsWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _stageUfePathSegment = "|GeomSubsetsWireframeHighlightTestScene|GeomSubsetsWireframeHighlightTestSceneShape"

    _cubeMeshUfePathSegment = "/Root/CubeMeshXform/CubeMesh"
    _sphereMeshUfePathSegment = "/Root/SphereMeshXform/SphereMesh"

    _cubeUpperHalfName = "CubeUpperHalf"
    _sphereUpperHalfName = "SphereUpperHalf"

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testGeomSubsetsWireframeHighlight', 'GeomSubsetsWireframeHighlightTestScene.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestGeomSubsetsWireframeHighlight, self).setUp()
        self.loadUsdScene()
        cmds.select(clear=True)
        cmds.optionVar(
                sv=('mayaHydra_GeomSubsetsPickMode', 'Faces'))
        self.setBasicCam(5)
        cmds.refresh()

    def test_SimpleGeomSubsetHighlight(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        cubeGeomSubsetPath = self._stageUfePathSegment + "," + self._cubeMeshUfePathSegment + "/" + self._cubeUpperHalfName
        cubeGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubeGeomSubsetPath))

        sn.clear()
        sn.append(cubeGeomSubsetItem)
        self.assertSnapshotClose("simpleGeomSubsetHighlight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_InstancedGeomSubsetHighlight(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        sphereGeomSubsetPath = self._stageUfePathSegment + "," + self._sphereMeshUfePathSegment + "/" + self._sphereUpperHalfName
        sphereGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(sphereGeomSubsetPath))

        sn.clear()
        sn.append(sphereGeomSubsetItem)
        self.assertSnapshotClose("instancedGeomSubsetHighlight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
