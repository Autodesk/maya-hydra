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
from testUtils import PluginLoaded
import maya.mel as mel

class TestFlowViewportAPIViewportInformation(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_RendererSwitching(self):

        # reset all default viewports for VP2 as the test framework sets it to Storm in base class    
        self.setViewport2Renderer()

        with PluginLoaded('mayaHydraCppTests'):
            #Switch to Storm
            self.setHdStormRenderer()
            cmds.refresh()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationWithHydra")
            #Switch to VP2
            self.setViewport2Renderer()
            cmds.refresh()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationWithoutHydra")
            #Switch to Storm again
            self.setHdStormRenderer()
            cmds.refresh()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationWithHydraAgain")
            
    def test_MultipleViewports(self):

        # reset all default viewports for VP2 as the test framework sets it to Storm in base class    
        self.setViewport2Renderer()
        
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationMultipleViewportsInit")
            #switch to 4 views
            mel.eval('FourViewLayout')
            #Set focus on persp view
            cmds.setFocus ('modelPanel4') #Is the persp view
            #Set Storm as the renderer
            self.setHdStormRenderer()
            
            #Set focus on model Panel 2 (it's an orthographic view : right) 
            cmds.setFocus ('modelPanel2')
            #Set Storm as the renderer
            self.setHdStormRenderer()
            cmds.refresh()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationMultipleViewports2Viewports")
            
            #Set focus on persp view
            cmds.setFocus ('modelPanel4') #Is the persp view
            #Set VP2 as the renderer
            self.setViewport2Renderer()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationMultipleViewports1Viewport")
            
            #Set focus on model Panel 2 (it's an orthographic view : right) 
            cmds.setFocus ('modelPanel2')
            #Set VP2 as the renderer
            self.setViewport2Renderer()
            cmds.mayaHydraCppTest(f="FlowViewportAPI.viewportInformationMultipleViewports0Viewport")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
