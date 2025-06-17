#
# Copyright 2025 Autodesk, Inc. All rights reserved.
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
from testUtils import PluginLoaded
import platform

class TestCustomShadersNode(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 2 #We have errors on Windows and Linux of about 1%, so we need to increase the threshold

    def test_FootPrintNodeForSecondPass(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex
            footPrintNodeName = cmds.createNode("MhFootPrint")
            # Frame the currently selected objects in the active viewport
            cmds.viewFit()
            #Clear selection
            cmds.select(clear=True)
            
            failures = []
            
            def run_assertion(filename, mode):
                try:
                    self.assertSnapshotClose(filename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
                except AssertionError as e:
                    failures.append(f"Failed on {filename} ({mode}): {str(e)}")
                    # Still continue to the next test
            
            # Enable only the second render pass
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[1])
            run_assertion("asGeom_2ndPass.png", "second pass")
            
            # Enable only the main render pass
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[0])
            run_assertion("asGeom_1stPass.png", "first pass")
            
            # Enable all render passes
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[0, 1])
            #Workaround for a bug when re-enabling all passes 
            self.setViewport2Renderer()
            self.setHdStormRenderer()
            run_assertion("asGeom_AllPasses.png", "all passes")

            cmds.setAttr(footPrintNodeName + '.drawAsGuide', True)
            cmds.refresh()
            # Frame the currently selected objects in the active viewport
            cmds.viewFit()

            # Enable only the second render pass
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[1])
            run_assertion("asGuide_2ndPass.png", "guide second pass")
            
            # Enable only the main render pass
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[0])
            run_assertion("asGuide_1stPass.png", "guide first pass")
            
            # Enable all render passes
            #Workaround for a bug when re-enabling all passes 
            self.setViewport2Renderer()
            self.setHdStormRenderer()
            cmds.mayaHydraSetVisibleRenderPasses(edit=True, visible=[0, 1])
            run_assertion("asGuide_AllPasses.png", "guide all passes")
            
            # After all tests have run, report any failures
            if failures:
                self.fail(f"The following assertions failed:\n" + "\n".join(failures))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
