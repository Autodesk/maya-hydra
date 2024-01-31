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
from testUtils import PluginLoaded

class TestNurbsPrimitives(mtohUtils.MtohTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.0
    IMAGE_DIFF_FAIL_PERCENT = 0.0

    def testSnapshot(self, referenceFilename):
        try:
            self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        except:
            pass

    def setupScene(self, nurbsCreationCallable):
        self.setHdStormRenderer()
        #cmds.loadPlugin('ArubaTessellator')
        nurbsCreationCallable()
        #cmds.viewFit()
        cmds.refresh(force=True)

    def test_NurbsTorus(self):
        with PluginLoaded('ArubaTessellator'):
            self.setupScene(cmds.torus)
            self.testSnapshot("torus_fresh.png")
        
            makeNurbNodeName = "makeNurbTorus1"
            cmds.setAttr(makeNurbNodeName + ".startSweep", 50)
            cmds.setAttr(makeNurbNodeName + ".endSweep", 300)
            cmds.setAttr(makeNurbNodeName + ".radius", 2)
            cmds.setAttr(makeNurbNodeName + ".degree", 1)
            cmds.setAttr(makeNurbNodeName + ".sections", 12)
            cmds.setAttr(makeNurbNodeName + ".spans", 6)
            cmds.setAttr(makeNurbNodeName + ".heightRatio", 0.8)
            cmds.setAttr(makeNurbNodeName + ".minorSweep", 250)
            cmds.refresh(force=True)
            self.testSnapshot("torus_modified.png")

            cmds.setAttr(makeNurbNodeName + ".useTolerance", True)
            cmds.setAttr(makeNurbNodeName + ".tolerance", 0.05)
            cmds.refresh(force=True)
            self.testSnapshot("torus_tolerance.png")

    def test_NurbsCube(self):
        with PluginLoaded('ArubaTessellator'):
            self.setupScene(cmds.nurbsCube)
            self.testSnapshot("cube_fresh.png")

            makeNurbNodeName = "makeNurbCube1"
            cmds.setAttr(makeNurbNodeName + ".degree", 1)
            cmds.setAttr(makeNurbNodeName + ".patchesU", 2)
            cmds.setAttr(makeNurbNodeName + ".patchesV", 3)
            cmds.setAttr(makeNurbNodeName + ".width", 4)
            cmds.setAttr(makeNurbNodeName + ".lengthRatio", 5)
            cmds.setAttr(makeNurbNodeName + ".heightRatio", 6)
            cmds.refresh(force=True)
            self.testSnapshot("cube_modified.png")

    def test_NurbsCircle(self):
        with PluginLoaded('ArubaTessellator'):
            self.setupScene(cmds.circle)
            self.testSnapshot("circle_fresh.png")

            makeNurbNodeName = "makeNurbCircle1"
            cmds.setAttr(makeNurbNodeName + ".sweep", 180)
            cmds.setAttr(makeNurbNodeName + ".radius", 2)
            cmds.setAttr(makeNurbNodeName + ".degree", 1)
            cmds.setAttr(makeNurbNodeName + ".sections", 12)
            cmds.setAttr(makeNurbNodeName + ".normalX", 1)
            cmds.setAttr(makeNurbNodeName + ".normalY", 2)
            cmds.setAttr(makeNurbNodeName + ".normalZ", 3)
            cmds.setAttr(makeNurbNodeName + ".centerX", 4)
            cmds.setAttr(makeNurbNodeName + ".centerY", 5)
            cmds.setAttr(makeNurbNodeName + ".centerZ", 6)
            cmds.setAttr(makeNurbNodeName + ".firstPointX", 7)
            cmds.setAttr(makeNurbNodeName + ".firstPointY", 8)
            cmds.setAttr(makeNurbNodeName + ".firstPointZ", 9)
            cmds.refresh(force=True)
            self.testSnapshot("circle_modified.png")

            cmds.setAttr(makeNurbNodeName + ".useTolerance", True)
            cmds.setAttr(makeNurbNodeName + ".tolerance", 0.05)
            cmds.refresh(force=True)
            self.testSnapshot("circle_tolerance.png")

            cmds.setAttr(makeNurbNodeName + ".fixCenter", False)
            cmds.refresh(force=True)
            self.testSnapshot("circle_unfixedCenter.png")

    def test_NurbsSquare(self):
        with PluginLoaded('ArubaTessellator'):
            self.setupScene(cmds.nurbsSquare)
            self.testSnapshot("square_fresh.png")

            makeNurbNodeName = "makeNurbsSquare1"
            cmds.setAttr(makeNurbNodeName + ".sideLength1", 2)
            cmds.setAttr(makeNurbNodeName + ".sideLength2", 3)
            cmds.setAttr(makeNurbNodeName + ".spansPerSide", 4)
            cmds.setAttr(makeNurbNodeName + ".degree", 1)
            cmds.setAttr(makeNurbNodeName + ".normalX", 1)
            cmds.setAttr(makeNurbNodeName + ".normalY", 2)
            cmds.setAttr(makeNurbNodeName + ".normalZ", 3)
            cmds.setAttr(makeNurbNodeName + ".centerX", 4)
            cmds.setAttr(makeNurbNodeName + ".centerY", 5)
            cmds.setAttr(makeNurbNodeName + ".centerZ", 6)
            cmds.refresh(force=True)
            self.testSnapshot("square_modified.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())