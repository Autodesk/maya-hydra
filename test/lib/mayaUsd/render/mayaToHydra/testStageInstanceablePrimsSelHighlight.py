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
import mayaUtils
import ufe

class TestStageInstanceablePrimsSelHighlight(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.3

    def SelectAndDoSnapshots( self, cylinderItems, sphereItem, snapshotSuffix):
        self.assertIsNotNone(cylinderItems)
        self.assertIsNotNone(sphereItem)
        #Select the cylinder and do a snapshot
        ufe.GlobalSelection.get().clear()
        ufe.GlobalSelection.get().append(cylinderItems)
        cylinderSnapshotName = "cylSel"+ snapshotSuffix +".png"
        self.assertSnapshotClose(cylinderSnapshotName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Select the sphere and do a snapshot
        ufe.GlobalSelection.get().clear()
        ufe.GlobalSelection.get().append(sphereItem)
        sphereSnapshotName = "spherSel"+ snapshotSuffix +".png"
        self.assertSnapshotClose(sphereSnapshotName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_UsdStageInstanceablePrimsSelHighlight(self):
        import usdUtils # usdUtils imports mayaUsd.ufe
        from mayaUsd import lib as mayaUsdLib
        
        #Load a scene with a usd stage with 2 prims that have a Hierarchy
        #Cylinder1 is the parent of Sphere1
        testFile = mayaUtils.openTestScene(
                "testStageInstanceablePrimsSelHighlight",
                "testInstanceablePrimsSelHighlight.ma")
        self.setHdStormRenderer()
        cmds.refresh()

        #Get the usd prims
        mayaUsdProxyShapeNode = "stageShape1"
        theStage = mayaUsdLib.GetPrim(mayaUsdProxyShapeNode).GetStage()
        
        cylinderPrim = theStage.GetPrimAtPath('/Root/Cylinder1')
        self.assertIsNotNone(cylinderPrim)
        spherePrim = theStage.GetPrimAtPath('/Root/Cylinder1/Sphere1')
        self.assertIsNotNone(spherePrim)
        
        #Creater the ufe paths to the cylinder and sphere prims
        cylinderPath = ufe.Path([
            mayaUtils.createUfePathSegment("|stage1|stageShape1"), #|stage1|stageShape1 is the UFE path to the proxy shape node
            usdUtils.createUfePathSegment("/Root/Cylinder1")])
        cylinderItems = ufe.Hierarchy.createItem(cylinderPath)
        self.assertIsNotNone(cylinderItems)

        spherePath = ufe.Path([
            mayaUtils.createUfePathSegment("|stage1|stageShape1"), #|stage1|stageShape1 is the UFE path to the proxy shape node
            usdUtils.createUfePathSegment("/Root/Cylinder1/Sphere1")])
        sphereItem = ufe.Hierarchy.createItem(spherePath)
        self.assertIsNotNone(sphereItem)
        
        #Select the cylinder and sphere alternativelely and do snapshots
        self.SelectAndDoSnapshots(cylinderItems, sphereItem, "")

        #Set the cylinder only as instanceable
        cylinderPrim.SetInstanceable(True)
        self.assertTrue(cylinderPrim.IsInstanceable())
        self.assertFalse(spherePrim.IsInstanceable())
        self.SelectAndDoSnapshots(cylinderItems, sphereItem, "_Cyl_Inst")

        #Set the sphere and cylinder as instanceable
        spherePrim.SetInstanceable(True)
        self.assertTrue(spherePrim.IsInstanceable())
        self.assertTrue(cylinderPrim.IsInstanceable())
        self.SelectAndDoSnapshots(cylinderItems, sphereItem, "_Cyl_Spher_Inst")
        
        #Set the sphere only as instanceable
        cylinderPrim.SetInstanceable(False)
        self.assertFalse(cylinderPrim.IsInstanceable())
        self.assertTrue(spherePrim.IsInstanceable())
        self.SelectAndDoSnapshots(cylinderItems, sphereItem, "_Spher_Inst")
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
