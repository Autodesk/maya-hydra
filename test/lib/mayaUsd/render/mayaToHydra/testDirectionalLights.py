#
# Copyright 2023 Autodesk, Inc. All rights reserved.
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
import unittest

class TestDirectionalLights(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def activeModelPanel(self):
        """Return the model panel that will be used for playblasting etc..."""
        for panel in cmds.getPanel(type="modelPanel"):
            if cmds.modelEditor(panel, q=1, av=1):
                return panel

    #Test directional lights with a sphere in a usd stage 
    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_DirectionalLights(self):
        cmds.file(new=True, force=True)
        cmds.refresh()

        # Load a maya scene with a sphere in a UsdStage and a directional light, with HdStorm already being the viewport renderer.
        # The sphere is not at the origin on purpose
        testFile = mayaUtils.openTestScene(
                "testDirectionalLights",
                "UsdStageWithSphereMatXStdSurf.ma")
        cmds.refresh()
        self.assertSnapshotClose("directionalLight.png", None, None)
        #Do a view fit
        cmds.viewFit('persp')
        cmds.refresh()
        self.assertSnapshotClose("directionalLightFit.png", None, None)

        #delete the directional Light
        cmds.delete('directionalLight1')
        cmds.refresh()

        # Switch the default lighting on, the view is still fit on the sphere
        panel = self.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayLights="default")
        cmds.refresh()
        self.assertSnapshotClose("defaultLightFit.png", None, None)
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
