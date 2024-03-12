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

class TestNurbsSurfaces(mtohUtils.MtohTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupScene(self, nurbsCreationCallable):
        self.setHdStormRenderer()
        cmds.loadPlugin('ArubaTessellator')
        nurbsCreationCallable()
        cmds.refresh()

    def test_NurbsTorus(self):
        self.setupScene(cmds.torus)
        self.runCppTest("NurbsSurfaces.nurbsTorus")

    def test_NurbsCube(self):
        self.setupScene(cmds.nurbsCube)
        self.runCppTest("NurbsSurfaces.nurbsCube")

    def test_NurbsCircle(self):
        self.setupScene(cmds.circle)
        self.runCppTest("NurbsSurfaces.nurbsCircle")

    def test_NurbsSquare(self):
        self.setupScene(cmds.nurbsSquare)
        self.runCppTest("NurbsSurfaces.nurbsSquare")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
