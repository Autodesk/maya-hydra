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

    _imageVersionUSD = None

    _baseStageFilename = "GeomSubsetsWireframeHighlightTestScene.usda"
    _baseStageUfePathSegment = "|GeomSubsetsWireframeHighlightTestScene|GeomSubsetsWireframeHighlightTestSceneShape"

    _cubeMeshUfePathSegment = "/Root/CubeMeshXform/CubeMesh"
    _sphereMeshUfePathSegment = "/Root/SphereMeshXform/SphereMesh"

    _cubeUpperHalfName = "CubeUpperHalf"
    _sphereUpperHalfName = "SphereUpperHalf"

    _displacementStageFilename = "GeomSubsetWireframeHighlightDisplacementTestScene.usda"
    _displacementStageUfePathSegment = "|GeomSubsetWireframeHighlightDisplacementTestScene|GeomSubsetWireframeHighlightDisplacementTestSceneShape"
    _displacementGeomSubsetUfePathSegment = "/Torus/GeomSubset"
    
    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    @classmethod
    def setUpClass(cls):
        super(TestGeomSubsetsWireframeHighlight, cls).setUpClass()
        if cls._usdVersion >= (0, 25, 8):
            cls._imageVersionUSD = "usd2508+"

    def setUp(self):
        super(TestGeomSubsetsWireframeHighlight, self).setUp()


    def loadUsdScene(self, stageFilename):
        usdScenePath = testUtils.getTestScene('testGeomSubsetsWireframeHighlight', stageFilename)
        usdUtils.createStageFromFile(usdScenePath)
        cmds.select(clear=True)
        cmds.optionVar(
                sv=('mayaHydra_GeomSubsetsPickMode', 'Faces'))
        self.setBasicCam(5)
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.refresh()

    def test_SimpleGeomSubset(self):
        if self._usdVersion < (0, 24, 3):
            self.skipTest("Skipping test, USD version used does not support Hydra GeomSubset prims")
        
        self.loadUsdScene(self._baseStageFilename)
        
        sn = ufe.GlobalSelection.get()
        sn.clear()

        cubeGeomSubsetPath = self._baseStageUfePathSegment + "," + self._cubeMeshUfePathSegment + "/" + self._cubeUpperHalfName
        cubeGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubeGeomSubsetPath))

        sn.clear()
        sn.append(cubeGeomSubsetItem)
        self.assertSnapshotClose("simpleGeomSubsetHighlight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)

    def test_InstancedGeomSubset(self):
        if self._usdVersion < (0, 24, 3):
            self.skipTest("Skipping test, USD version used does not support Hydra GeomSubset prims")
        
        self.loadUsdScene(self._baseStageFilename)

        sn = ufe.GlobalSelection.get()
        sn.clear()

        sphereGeomSubsetPath = self._baseStageUfePathSegment + "," + self._sphereMeshUfePathSegment + "/" + self._sphereUpperHalfName
        sphereGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(sphereGeomSubsetPath))

        sn.clear()
        sn.append(sphereGeomSubsetItem)
        self.assertSnapshotClose("instancedGeomSubsetHighlight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)
    
    def test_WireframeColorChange(self):
        if self._usdVersion < (0, 24, 3):
            self.skipTest("Skipping test, USD version used does not support Hydra GeomSubset prims")

        self.loadUsdScene(self._baseStageFilename)
        
        sn = ufe.GlobalSelection.get()
        sn.clear()

        cubeGeomSubsetPath = self._baseStageUfePathSegment + "," + self._cubeMeshUfePathSegment + "/" + self._cubeUpperHalfName
        cubeGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubeGeomSubsetPath))

        sphereGeomSubsetPath = self._baseStageUfePathSegment + "," + self._sphereMeshUfePathSegment + "/" + self._sphereUpperHalfName
        sphereGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(sphereGeomSubsetPath))

        sn.clear()
        sn.append(cubeGeomSubsetItem)
        self.assertSnapshotClose("wireframeColorChange_before.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)

        sn.append(sphereGeomSubsetItem)
        self.assertSnapshotClose("wireframeColorChange_after.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)
    
    def test_MeshAndGeomSubsetSelection(self):
        if self._usdVersion < (0, 24, 3):
            self.skipTest("Skipping test, USD version used does not support Hydra GeomSubset prims")
        
        self.loadUsdScene(self._baseStageFilename)
        
        sn = ufe.GlobalSelection.get()
        sn.clear()

        cubeMeshPath = self._baseStageUfePathSegment + "," + self._cubeMeshUfePathSegment
        cubeMeshItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubeMeshPath))

        cubeGeomSubsetPath = self._baseStageUfePathSegment + "," + self._cubeMeshUfePathSegment + "/" + self._cubeUpperHalfName
        cubeGeomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubeGeomSubsetPath))

        sn.clear()
        sn.append(cubeGeomSubsetItem)
        sn.append(cubeMeshItem)
        self.assertSnapshotClose("geomSubsetThenMeshSelection.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)

        # HYDRA-1407 : If a mesh and one of its geomSubsets are both selected, the mesh's wireframe color overpowers the geomSubset's
        #sn.clear()
        #sn.append(cubeMeshItem)
        #sn.append(cubeGeomSubsetItem)
        #self.assertSnapshotClose("meshThenGeomSubsetSelection.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)
    
    def test_Displacement(self):
        if self._usdVersion < (0, 24, 3):
            self.skipTest("Skipping test, USD version used does not support Hydra GeomSubset prims")
        
        self.loadUsdScene(self._displacementStageFilename)
        self.setBasicCam(8)
        
        sn = ufe.GlobalSelection.get()
        sn.clear()

        geomSubsetPath = self._displacementStageUfePathSegment + "," + self._displacementGeomSubsetUfePathSegment
        geomSubsetItem = ufe.Hierarchy.createItem(ufe.PathString.path(geomSubsetPath))
        sn.append(geomSubsetItem)
        self.assertSnapshotClose("displacement.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self._imageVersionUSD)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
