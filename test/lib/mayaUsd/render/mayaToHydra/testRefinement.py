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

class TestRefinement(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT)

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

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
