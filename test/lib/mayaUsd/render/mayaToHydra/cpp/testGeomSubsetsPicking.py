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
import mayaUtils
import mtohUtils
import testUtils
import usdUtils

from testUtils import PluginLoaded

class TestGeomSubsetsPicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testGeomSubsetsPicking', 'GeomSubsetsPickingTestScene.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestGeomSubsetsPicking, self).setUp()
        self.loadUsdScene()
        cmds.select(clear=True)
        cmds.optionVar(
                sv=('mayaHydra_GeomSubsetsPickMode', 'GeomSubsets'))
        cmds.setAttr('persp.translate', 0, 0, 15, type='float3')
        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        self.setHdStormRenderer()
        cmds.refresh()

    # def test_SinglePickGeomSubset(self):
    #     cubeObjectName = self.createMayaCube()
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestGeomSubsetsPicking.pickGeomSubset")

    # def test_PickMayaLight(self):
    #     directionalLightObjectName = self.createMayaDirectionalLight()
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(directionalLightObjectName, "simpleLight", f="TestGeomSubsetsPicking.pickObject")

    # def test_PickUsdMesh(self):
    #     import mayaUsd_createStageWithNewLayer
    #     stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
    #     cubeObjectName = self.createUsdCubeFromMaya(stagePath)
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestGeomSubsetsPicking.pickObject")

    # def test_PickUsdImplicitSurface(self):
    #     import mayaUsd_createStageWithNewLayer
    #     stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
    #     cubeObjectName = self.createUsdCube(stagePath)
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestGeomSubsetsPicking.pickObject")
    
    # def test_PickUsdLight(self):
    #     import mayaUsd_createStageWithNewLayer
    #     stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
    #     rectLightObjectName = self.createUsdRectLight(stagePath)
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(rectLightObjectName, "rectLight", f="TestGeomSubsetsPicking.pickObject")

    # def test_MarqueeSelection(self):
    #     import mayaUsd_createStageWithNewLayer
    #     stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
    #     mayaCubeName = self.createMayaCube()
    #     mayaDirectionalLightName = self.createMayaDirectionalLight()
    #     usdMayaCubeName = self.createUsdCubeFromMaya(stagePath)
    #     usdCubeName = self.createUsdCube(stagePath)
    #     usdRectLightName = self.createUsdRectLight(stagePath)
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(
    #             mayaCubeName, "mesh",
    #             mayaDirectionalLightName, "simpleLight",
    #             usdMayaCubeName, "mesh",
    #             usdCubeName, "mesh",
    #             usdRectLightName, "rectLight",
    #             f="TestGeomSubsetsPicking.marqueeSelect")

    def test_GeomSubsetPicking(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="TestGeomSubsetsPicking.geomSubsetPicking")
    
    # def test_FallbackPicking(self):
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(f="TestGeomSubsetsPicking.fallbackPicking")

    # def test_InstanceGeomSubsetPicking(self):
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(f="TestGeomSubsetsPicking.instanceGeomSubsetPicking")
    
    # def test_InstanceFallbackPicking(self):
    #     with PluginLoaded('mayaHydraCppTests'):
    #         cmds.mayaHydraCppTest(f="TestGeomSubsetsPicking.instanceFallbackPicking")

    def test_MarqueeSelect(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="TestGeomSubsetsPicking.marqueeSelect")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())