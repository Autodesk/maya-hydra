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

class TestStageVariants(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.3

    def test_UsdStageVariants(self):
        import usdUtils # usdUtils imports mayaUsd.ufe
        from mayaUsd import lib as mayaUsdLib
        
        testFile = mayaUtils.openTestScene(
                "testStageVariants",
                "testStageVariants.ma")
        self.setHdStormRenderer()
        cmds.refresh()

        shapeNode = "variantsShape"
        shapeStage = mayaUsdLib.GetPrim(shapeNode).GetStage()
        
        #Check geometry variant
        rootPrim = shapeStage.GetPrimAtPath('/Cubes')
        self.assertIsNotNone(rootPrim)
        modVariant = rootPrim.GetVariantSet('modelingVariant')
        self.assertIsNotNone(modVariant)
        self.assertEqual(modVariant.GetVariantSelection(), 'OneCube')

        #Select the USD Cubes to see the selection highlight
        cubesPath = ufe.Path([
            mayaUtils.createUfePathSegment("|variants|variantsShape"), 
            usdUtils.createUfePathSegment("/Cubes")])
        cubesItems = ufe.Hierarchy.createItem(cubesPath)
        self.assertIsNotNone(cubesItems)
        ufeGlobalSel =  ufe.GlobalSelection.get()
        ufeGlobalSel.clear()
        ufeGlobalSel.append(cubesItems)
        self.assertSnapshotClose("oneCube.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.clear()
        
        modVariant.SetVariantSelection('TwoCubes')
        self.assertEqual(modVariant.GetVariantSelection(), 'TwoCubes')
        ufeGlobalSel.append(cubesItems)
        self.assertSnapshotClose("twoCubes.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.clear()
        
        modVariant.SetVariantSelection('ThreeCubes')
        self.assertEqual(modVariant.GetVariantSelection(), 'ThreeCubes')
        ufeGlobalSel.append(cubesItems)
        self.assertSnapshotClose("threeCubes.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.clear()
        
        #Check displacement variant for CubeOne (not an actual displacement but a different transform)
        cube1Prim = shapeStage.GetPrimAtPath('/Cubes/Geom/CubeOne')
        self.assertIsNotNone(cube1Prim)
        displacementVariant = cube1Prim.GetVariantSet('displacement')
        self.assertIsNotNone(displacementVariant)
        self.assertEqual(displacementVariant.GetVariantSelection(), 'none')
        displacementVariant .SetVariantSelection('moved')
        self.assertEqual(displacementVariant.GetVariantSelection(), 'moved')
        ufeGlobalSel.append(cubesItems)
        self.assertSnapshotClose("threeCubesWithDisplacement.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.clear()

        #Check if it works when the prims are instanceable
        cube2Prim = shapeStage.GetPrimAtPath('/Cubes/Geom/CubeTwo')
        self.assertIsNotNone(cube2Prim)
        cube3Prim = shapeStage.GetPrimAtPath('/Cubes/Geom/CubeThree')
        self.assertIsNotNone(cube3Prim)
        cube1Prim.SetInstanceable(True)
        self.assertTrue(cube1Prim.IsInstanceable())
        cube2Prim.SetInstanceable(True)
        self.assertTrue(cube2Prim.IsInstanceable())
        cube3Prim.SetInstanceable(True)
        self.assertTrue(cube3Prim.IsInstanceable())
        modVariant.SetVariantSelection('OneCube')
        self.assertEqual(modVariant.GetVariantSelection(), 'OneCube')
        ufeGlobalSel.append(cubesItems)
        self.assertSnapshotClose("oneCubeInstanceable.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.clear()

        #Enable the following tests when HYDRA-820 is fixed
        #modVariant.SetVariantSelection('TwoCubes')
        #self.assertEqual(modVariant.GetVariantSelection(), 'TwoCubes')
        #ufeGlobalSel.append(cubesItems)
        #self.assertSnapshotClose("twoCubesInstanceable.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        #ufeGlobalSel.clear()

        #modVariant.SetVariantSelection('ThreeCubes')
        #self.assertEqual(modVariant.GetVariantSelection(), 'ThreeCubes')
        #ufeGlobalSel.append(cubesItems) 
        #self.assertSnapshotClose("threeCubesInstanceable.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
