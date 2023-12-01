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

class TestFlowViewportAPIFilterPrims(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupParentChildScene(self):
        self.setHdStormRenderer()
        #Create a maya sphere named parentSphere
        cmds.polyCube(name="parentCube", w=2, h=2, d=2) 
        cmds.polySphere(name="smallSphere") 
        #Create a big sphere which has more than 10 000 vertices
        cmds.polySphere(name="bigSphere", subdivisionsAxis=200, subdivisionsHeight= 200) 
        cmds.refresh()

    def test_FilterPrimitives(self):
        self.setupParentChildScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="FlowViewportAPI.filterPrimitives")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
