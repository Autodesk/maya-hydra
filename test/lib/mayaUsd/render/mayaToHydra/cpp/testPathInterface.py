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

import pxr.Tf

import fixturesUtils
import mtohUtils

import ufe

from testUtils import PluginLoaded

class TestPathInterface(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    CUBE_PRIM_PATH = "/cube"

    def cubeAppPath(self, stageIndex):
        return self.ps[stageIndex] + ',' + self.CUBE_PRIM_PATH

    def setupScene(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom

        self.setHdStormRenderer()
        cmds.refresh()

        # Create multiple USD stages.  As of 18-Dec-2023, with Arnold loaded,
        # initial stage creation fails with the following exception:
        # pxr.Tf.ErrorException:
        # Error in 'pxrInternal_v0_23__pxrReserved__::UsdImagingAdapterRegistry' at line 146 in file S:\jenkins\workspace\ECP\ecg-usd-build\ecg-usd-full-python3.11-windows\ecg-usd-build\usd\pxr\usdImaging\usdImaging\adapterRegistry.cpp : '[PluginDiscover] A prim adapter for primType 'GeometryLight' already exists! Overriding prim adapters at runtime is not supported. The last discovered adapter (ArnoldMeshLightAdapter) will be used. The previously discovered adapter (UsdImagingGeometryLightAdapter) will be discarded.'
        #
        # Loading the mayaUsdPlugin is not sufficient to cause the issue.
        # Fixed by the Arnold team in ARNOLD-14437, entered for maya-hydra
        # integration as HYDRA-546, remove workaround when HYDRA-546 is fixed.
        for i in range(3):
            try:
                mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
            except pxr.Tf.ErrorException:
                pass

        self.ps = cmds.ls(type='mayaUsdProxyShape', l=True)

        for i in range(3):
            stage = mayaUsd.lib.GetPrim(self.ps[i]).GetStage()

            # Define a cube prim in the stage (implicit surface)
            UsdGeom.Cube.Define(stage, self.CUBE_PRIM_PATH)

        cmds.refresh()

    def test_PathInterface(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):

            #------------------------------------------------------------------
            # Test multiple stages creating children under scene index prefix
            #------------------------------------------------------------------

            # Confirm that the number of scene indices under the top-level 
            # prim representing the plugin matches the number of proxy shapes.
            cmds.mayaHydraCppTest(f="TestPathInterface.testSceneIndices")

            #------------------------------------------------------------------
            # Test that selection works for multiple proxy shapes.
            #------------------------------------------------------------------

            # Select the cube in one of the stages, confirm it's selected
            # in Hydra, and that the other cubes are not.
            for selected in range(3):
                cmds.select(self.cubeAppPath(selected))
                self.assertEqual(self.cubeAppPath(selected), cmds.ls(sl=True, ufe=True)[0])
                for i in range(3):
                    cmds.mayaHydraCppTest(self.cubeAppPath(i), f="TestPathInterface.testSelected" if i==selected else "TestPathInterface.testUnselected")

            #------------------------------------------------------------------
            # Test that selection works after proxy shape rename.
            #------------------------------------------------------------------

            # Rename one of the proxy shape transforms, and select its
            # descendant cube.  The cube should be selected in the Hydra scene.
            result = cmds.rename(self.ps[0], 'renamedShape')
            xform = ufe.PathString.string(ufe.PathString.path(self.ps[0]).pop())
            self.ps[0] = xform + '|' + result
            cmds.select(self.cubeAppPath(0))
            self.assertEqual(self.cubeAppPath(0), cmds.ls(sl=True, ufe=True)[0])
            
            cmds.mayaHydraCppTest(self.cubeAppPath(0), f="TestPathInterface.testSelected")

            #------------------------------------------------------------------
            # Test that selection works after proxy shape reparent.
            #------------------------------------------------------------------

            # Reparent the proxy shape transform
            cmds.group(xform)
            self.ps[0] = '|group1' + self.ps[0]
            cmds.select(self.cubeAppPath(0))
            self.assertEqual(self.cubeAppPath(0), cmds.ls(sl=True, ufe=True)[0])

            cmds.mayaHydraCppTest(self.cubeAppPath(0), f="TestPathInterface.testSelected")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
