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

import platform

class TestMayaIsolateSelect(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 0.1
        return 0.01
    
    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 4
        return 0.2

    def test_IsolateSelect(self):

        sphere = cmds.sphere( n='sphere1' )
        cone = cmds.cone( n='cone1' )
        cmds.move( 1, 5, 1 )
        self.assertSnapshotClose("default" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        # Use the current modelPanel for isolation
        isolated_panel = cmds.paneLayout('viewPanes', q=True, pane1=True)  
        # locks the current list of objects within the mainConnection
        cmds.editor( isolated_panel, edit=True, lockMainConnection=True, mainListConnection='activeList' )
        
        cmds.select(sphere)
        cmds.isolateSelect( isolated_panel, state=1 )
        self.assertSnapshotClose("isolateSelect" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())