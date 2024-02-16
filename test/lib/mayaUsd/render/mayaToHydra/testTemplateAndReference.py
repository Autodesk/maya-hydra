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
import platform

class TestTemplateAndReference(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        return 0.01
    
    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 3
        return 0.2

    #Test object display & selection under Template & Reference modes.
    def test_TemplateAndReference(self):
        cmds.file(new=True, force=True)

        # Load a maya scene with a maya native cubic and sphere
        testFile = mayaUtils.openTestScene(
                "testTemplateAndReference",
                "testTemplateAndReference.ma")
        cmds.refresh()

        #Verify Default display
        panel = mayaUtils.activeModelPanel()
        self.assertSnapshotClose("default" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Verify template mode
        #Display
        cmds.setAttr('layer_sphere.displayType', 1)
        cmds.refresh()
        self.assertSnapshotClose("templateMode" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        #Selection
        cmds.select( clear=True )
        cmds.select( 'pSphere1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("templateModeSelection" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Verify reference mode
        #Display
        cmds.setAttr('layer_cubic.displayType', 2)
        cmds.refresh()
        self.assertSnapshotClose("referenceMode" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        #Selection
        cmds.select( clear=True )
        cmds.select( 'pCube1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("referenceModeSelection" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
