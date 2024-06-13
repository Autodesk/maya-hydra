#
# Copyright 2024 Autodesk, Inc. All rights reserved.
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

class TestFlowPluginsHierarchicalProperties(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mayaHydraFlowViewportAPILocator']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def keyframeAttribute(self, object, attribute, value):
        cmds.setAttr(object + "." + attribute, value)
        cmds.setKeyframe(object, attribute=attribute)

    def locatorSetup(self):
        # Sphere that should be filtered by the locator filtering scene index (>10k vertices)
        sphereNode = cmds.polySphere()[1]
        cmds.move(0, 0, 5)
        cmds.setAttr(sphereNode + ".subdivisionsAxis", 200)
        cmds.setAttr(sphereNode + ".subdivisionsHeight", 200)
        cmds.select(clear=True)

        locatorGrandParent = cmds.group(empty=True)
        locatorParent = cmds.group(empty=True, parent=locatorGrandParent)
        return locatorGrandParent, locatorParent
    
    def usdStageSetup(self):
        # Set up a USD stage a with a cube
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib
        from pxr import UsdGeom
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        UsdGeom.Cube.Define(stage, "/Cube")
        cmds.select(clear=True)
        stageParent = cmds.group(empty=True)
        cmds.parent(stagePath, stageParent)
        stageShape = stagePath.split('|')[-1]
        return stageParent, stageShape

    def usdStageAnimatedPrimSetup(self):
        usdScenePath = testUtils.getTestScene('testFlowPluginsHierarchicalProperties', 'animated_prim2.usda')
        stagePath =  usdUtils.createStageFromFile(usdScenePath)
        stageParent = cmds.group(empty=True)
        cmds.parent(stagePath, stageParent)
        stageTransform = stagePath.split('|')[1]
        return stageParent, stageTransform
    
    def assertSnapshotAndCompareVp2(self, referenceFile):
        self.setHdStormRenderer()
        self.assertSnapshotClose(referenceFile, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        self.setViewport2Renderer()
        self.assertSnapshotSilhouetteClose(referenceFile, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        self.setHdStormRenderer()

    def test_Authoring_Locator(self):
        self.setBasicCam(10)

        locatorGrandParent, locatorParent = self.locatorSetup()

        # Ensure the initial visibility is set correctly if a parent's visibility affects it
        cmds.setAttr(locatorGrandParent + ".visibility", False)
        locatorShape = cmds.createNode("MhFlowViewportAPILocator", parent=locatorParent)
        self.assertSnapshotClose("authoring_locator_visibility_off.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        # Check that updating a parent's visibility works after creation
        cmds.setAttr(locatorGrandParent + ".visibility", True)
        self.assertSnapshotClose("authoring_locator_visibility_on.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        # Check that changing the visibility on the shape itself works
        cmds.setAttr(locatorShape + ".visibility", False)
        self.assertSnapshotClose("authoring_locator_visibility_off.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        # Restore visibility
        cmds.setAttr(locatorShape + ".visibility", True)
        self.assertSnapshotClose("authoring_locator_visibility_on.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Change a parent transform
        cmds.xform(locatorGrandParent, translation=[1,-2,3], rotation=[5,-10,15], scale=[1,-2,3])
        self.assertSnapshotClose("authoring_locator_parentTransformChanged.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        # Change the shape's transform directly
        cmds.xform(cmds.listRelatives(locatorShape, parent=True)[0], translation=[-3,2,-1], rotation=[-15,10,-5], scale=[-2.5, 2.0, -1.5])
        self.assertSnapshotClose("authoring_locator_shapeTransformChanged.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_Authoring_UsdStage(self):
        self.setBasicCam(10)

        stageParent, stageShape = self.usdStageSetup()

        # Hide/unhide parent
        self.assertSnapshotAndCompareVp2("usdStage_visibility_on.png")
        cmds.setAttr(stageParent + ".visibility", False)
        self.assertSnapshotAndCompareVp2("usdStage_visibility_off.png")
        cmds.setAttr(stageParent + ".visibility", True)
        self.assertSnapshotAndCompareVp2("usdStage_visibility_on.png")
        # Hide/unhide the shape directly
        cmds.setAttr(stageShape + ".visibility", False)
        self.assertSnapshotAndCompareVp2("usdStage_visibility_off.png")
        cmds.setAttr(stageShape + ".visibility", True)
        self.assertSnapshotAndCompareVp2("usdStage_visibility_on.png")

        # Change a parent transform
        cmds.xform(stageParent, translation=[1,-2,3], rotation=[5,-10,15], scale=[1,-2,3])
        self.assertSnapshotAndCompareVp2("usdStage_parentTransformChanged.png")
        # Change the shape's transform directly
        cmds.xform(cmds.listRelatives(stageShape, parent=True)[0], translation=[-3,2,-1], rotation=[-15,10,-5], scale=[-2.5, 2.0, -1.5])
        self.assertSnapshotAndCompareVp2("usdStage_shapeTransformChanged.png")

    def test_Playback_Locator(self):
        self.setBasicCam(10)

        locatorGrandParent, locatorParent = self.locatorSetup()
        cmds.createNode("MhFlowViewportAPILocator", parent=locatorParent)

        cmds.currentTime(0)
        self.keyframeAttribute(locatorGrandParent, "visibility", True)
        self.keyframeAttribute(locatorGrandParent, "translateX", 0)

        cmds.currentTime(5)
        self.keyframeAttribute(locatorGrandParent, "visibility", False)
        self.keyframeAttribute(locatorGrandParent, "translateX", 15)

        cmds.currentTime(0)
        self.assertSnapshotClose("locator_playback_initial.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.currentTime(2)
        self.assertSnapshotClose("locator_playback_translated.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.currentTime(7)
        self.assertSnapshotClose("locator_playback_hidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_Playback_UsdStage(self):
        self.setBasicCam(10)

        stageParent, _ = self.usdStageSetup()

        cmds.currentTime(0)
        self.keyframeAttribute(stageParent, "visibility", True)
        self.keyframeAttribute(stageParent, "translateX", 0)

        cmds.currentTime(5)
        self.keyframeAttribute(stageParent, "visibility", False)
        self.keyframeAttribute(stageParent, "translateX", 15)

        cmds.currentTime(0)
        self.assertSnapshotAndCompareVp2("usdStage_playback_initial.png")

        cmds.currentTime(2)
        self.assertSnapshotAndCompareVp2("usdStage_playback_translated.png")

        cmds.currentTime(7)
        self.assertSnapshotAndCompareVp2("usdStage_playback_hidden.png")

    def test_UsdStageAnimatedPrim(self):
        # Setup
        self.setBasicCam(10)

        stageParent, stageTransform = self.usdStageAnimatedPrimSetup()

        cmds.currentTime(0)
        self.keyframeAttribute(stageParent, "translateY", 0)
        self.keyframeAttribute(stageTransform, "translateZ", 0)

        cmds.currentTime(4)
        self.keyframeAttribute(stageParent, "translateY", 5)
        self.keyframeAttribute(stageTransform, "translateZ", 5)

        def assertSnapshotsAtTime(time):
            cmds.currentTime(time)
            self.assertSnapshotAndCompareVp2("usdStageAnimatedPrim_t" + str(time) + ".png")

        # Translation
        assertSnapshotsAtTime(0)
        assertSnapshotsAtTime(1)
        assertSnapshotsAtTime(3)
        assertSnapshotsAtTime(5)
        assertSnapshotsAtTime(7)
        assertSnapshotsAtTime(10)
        assertSnapshotsAtTime(12)

        # Visibility
        assertSnapshotsAtTime(15)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
