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
import platform
import testUtils

class TestRefinement(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01

    @property
    def IMAGEDIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3.5
        return 0.1

    #This function is called before each test is launched
    def setUp(self):
        #call parent function first
        super(TestRefinement, self).setUp()
        #modify light intensity for usd 24.11+
        self.modifyDefaultLightIntensityByUsdVersion()

    def verifySnapshot(self, imageName, imageVersion=None):
        cmds.refresh()
        imageVersion = None
        if useDynamicVersion:
            frame_passes_count = self.framePassesCount
            if frame_passes_count == 2:
                imageVersion = "two_passes" + (f"_{usdImageVersion}" if usdImageVersion is not None else "")
            elif frame_passes_count == 1 and usdImageVersion is not None:
                imageVersion = usdImageVersion
                
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT,
                                 imageVersion)

    def test_usdPrim(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')
        usdUtils.createStageFromFile(usdScenePath)

        cmds.select(clear=True)#Clear selection

        self.setBasicCam(1)
        self.setHdStormRenderer()
        cmds.mayaHydra(createRenderGlobals=1)
        
        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot("usd_cube_refined_0.png")

        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 2)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot("usd_cube_refined_2.png")

        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 4)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot("usd_cube_refined_4.png")

        #restore the default
        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
    
    def test_basisCurves(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testRefinement', 'basisCurves.usda')
        usdUtils.createStageFromFile(usdScenePath)

        cmds.select(clear=True)#Clear selection

        cmds.setAttr('persp.translate', 10, 7.5, 20, type='float3')
        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        self.setHdStormRenderer()
        cmds.mayaHydra(createRenderGlobals=1)

        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot(imageName="basisCurves_refined_0.png", useDynamicVersion=True)

        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot(imageName="basisCurves_refined_1.png", useDynamicVersion=True)

        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 2)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot(imageName="basisCurves_refined_2.png", useDynamicVersion=True)

        #Refinement level 3 was changed in USD 25.8+
        _usdImageVersion = 'USD2508+' if self._usdVersion >= (0, 25, 8) else None
        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 3)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot("basisCurves_refined_3.png", _usdImageVersion)

        #restore the default
        cmds.setAttr("defaultRenderGlobals.mayaHydraRefinementLevel", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRefinementLevel")
        self.verifySnapshot(imageName="basisCurves_refined_0.png", useDynamicVersion=True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
