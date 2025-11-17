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
from testUtils import PluginLoaded

class TestPointInstancingWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _stagePathSegment = "|NestedAndComposedPointInstancers|NestedAndComposedPointInstancersShape"

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1
    imageVersion=None

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testPointInstancingWireframeHighlight', 'NestedAndComposedPointInstancers.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestPointInstancingWireframeHighlight, self).setUp()
        self.loadUsdScene()
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.refresh()
        # Compute imageVersion once during setup
        frame_passes_count = self.framePassesCount
        if frame_passes_count == 2:
            self.imageVersion = "two_passes"

    def test_PointInstancerSelection(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 10, 10, 10, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        topInstancerPath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer"
        secondInstancerPath = self._stagePathSegment + "," + "/Root/SecondInstancer"
        thirdInstancerPath = self._stagePathSegment + "," + "/Root/ThirdInstancer"
        fourthInstancerPath = self._stagePathSegment + "," + "/Root/FourthInstancer"

        topInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerPath))
        secondInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(secondInstancerPath))
        thirdInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(thirdInstancerPath))
        fourthInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(fourthInstancerPath))

        sn.clear()
        sn.append(topInstancerItem)
        self.assertSnapshotClose("directSelection_topInstancer.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(secondInstancerItem)
        self.assertSnapshotClose("directSelection_secondInstancer.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(thirdInstancerItem)
        self.assertSnapshotClose("directSelection_thirdInstancer.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(fourthInstancerItem)
        self.assertSnapshotClose("directSelection_fourthInstancer.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        topInstancerParentPath = self._stagePathSegment + "," + "/Root/TopInstancerXform"
        topInstancerParentItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerParentPath))
        sn.clear()
        sn.append(topInstancerParentItem)
        self.assertSnapshotClose("parentSelection_topInstancer.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        rootPath = self._stagePathSegment + "," + "/Root"
        rootItem = ufe.Hierarchy.createItem(ufe.PathString.path(rootPath))
        sn.clear()
        sn.append(rootItem)
        self.assertSnapshotClose("rootSelection_all.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

    def test_InstanceSelection(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 5, 10, 5, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        topInstancerFirstInstancePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/0"
        secondInstancerSecondInstancePath = self._stagePathSegment + "," + "/Root/SecondInstancer/1"

        topInstancerFirstInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerFirstInstancePath))
        secondInstancerSecondInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(secondInstancerSecondInstancePath))

        sn.clear()
        sn.append(topInstancerFirstInstanceItem)
        self.assertSnapshotClose("topInstancerFirstInstance.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(secondInstancerSecondInstanceItem)
        self.assertSnapshotClose("secondInstancerSecondInstance.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)


    def test_PrototypeSelection(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 10, 10, 10, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        prototypePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/prototypes/NestedInstancerXform/NestedInstancer/prototypes/Cube/Geom/Cube"
        prototypeParentPath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/prototypes/NestedInstancerXform/NestedInstancer/prototypes/Cube"

        prototypeItem = ufe.Hierarchy.createItem(ufe.PathString.path(prototypePath))
        prototypeParentItem = ufe.Hierarchy.createItem(ufe.PathString.path(prototypeParentPath))

        sn.clear()
        sn.append(prototypeItem)
        self.assertSnapshotClose("prototype_directSelection.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        sn.clear()
        sn.append(prototypeParentItem)
        self.assertSnapshotClose("prototype_parentSelection.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_PointInstancerWireframeColorChange(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 10, 10, 10, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        topInstancerPath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer"
        secondInstancerPath = self._stagePathSegment + "," + "/Root/SecondInstancer"
        thirdInstancerPath = self._stagePathSegment + "," + "/Root/ThirdInstancer"
        fourthInstancerPath = self._stagePathSegment + "," + "/Root/FourthInstancer"

        topInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerPath))
        secondInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(secondInstancerPath))
        thirdInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(thirdInstancerPath))
        fourthInstancerItem = ufe.Hierarchy.createItem(ufe.PathString.path(fourthInstancerPath))

        sn.clear()
        sn.append(topInstancerItem)
        self.assertSnapshotClose("pointInstancerWireframeColorChange_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(secondInstancerItem)
        self.assertSnapshotClose("pointInstancerWireframeColorChange_2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        sn.append(thirdInstancerItem)
        self.assertSnapshotClose("pointInstancerWireframeColorChange_3.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        sn.append(fourthInstancerItem)
        self.assertSnapshotClose("pointInstancerWireframeColorChange_4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        sn.remove(fourthInstancerItem)
        self.assertSnapshotClose("pointInstancerWireframeColorChange_5.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
    
    def test_InstanceWireframeColorChange(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 5, 10, 5, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        topInstancerFirstInstancePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/0"
        topInstancerSecondInstancePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/1"

        topInstancerFirstInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerFirstInstancePath))
        topInstancerSecondInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(topInstancerSecondInstancePath))

        sn.clear()
        sn.append(topInstancerFirstInstanceItem)
        self.assertSnapshotClose("instanceWireframeColorChange_before.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(topInstancerSecondInstanceItem)
        #Has antialiasing issues, logged already, but the selection highlight colors are as expected
        self.assertSnapshotClose("instanceWireframeColorChange_after.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
    
    def test_PrototypeWireframeColorChange(self):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 10, 10, 10, type='float3')

        sn = ufe.GlobalSelection.get()
        sn.clear()

        cubePrototypePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/prototypes/NestedInstancerXform/NestedInstancer/prototypes/Cube/Geom/Cube"
        pyramidPrototypePath = self._stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/prototypes/NestedInstancerXform/NestedInstancer/prototypes/Pyramid/Geom/Pyramid"

        cubePrototypeItem = ufe.Hierarchy.createItem(ufe.PathString.path(cubePrototypePath))
        pyramidPrototypeItem = ufe.Hierarchy.createItem(ufe.PathString.path(pyramidPrototypePath))

        sn.clear()
        sn.append(cubePrototypeItem)
        self.assertSnapshotClose("prototypeWireframeColorChange_before.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        sn.append(pyramidPrototypeItem)
        self.assertSnapshotClose("prototypeWireframeColorChange_after.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
