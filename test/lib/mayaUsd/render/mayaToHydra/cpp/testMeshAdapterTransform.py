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

class TestMeshAdapterTransform(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupScene(self):
        self.setHdStormRenderer()
        cmds.polyCube(name="testCube")
        cmds.refresh() # Refresh to create the MeshAdapter and its mesh prim at the origin

    def test_Dirtying(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MeshAdapterTransform.testDirtying")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
