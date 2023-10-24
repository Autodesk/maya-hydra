# Copyright 2023 Autodesk
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

class TestMayaSceneFlattening(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupParentChildScene(self):
        self.setHdStormRenderer()
        cmds.polySphere(name="parentSphere")
        cmds.polyCube(name="childCube")
        cmds.parent("childCube", "parentSphere")
        cmds.move(1, 2, 3, "childCube", relative=True)
        cmds.move(-6, -4, -2, "parentSphere", relative=True)
        cmds.refresh()

    def test_ChildHasFlattenedTransform(self):
        self.setupParentChildScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaSceneFlattening.childHasFlattenedTransform")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
