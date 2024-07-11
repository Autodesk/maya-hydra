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

from testUtils import PluginLoaded

class TestSinglePicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setUp(self):
        super(TestSinglePicking, self).setUp()
        mayaUtils.openTestScene( 
                "testSinglePicking",
                "testSinglePicking.ma")
        cmds.select(clear=True)
        cmds.optionVar(
                sv=('mayaHydra_GeomSubsetsPickMode', 'None'))
        cmds.optionVar(
                sv=('mayaUsd_PointInstancesPickMode', 'Prototypes'))
        cmds.setAttr('persp.translate', 0, 0, 10, type='float3')
        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        cmds.refresh()

    def test_SinglePick(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="TestSinglePicking.singlePick")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
