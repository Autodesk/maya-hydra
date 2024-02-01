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

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def compareSnapshot(self, referenceFilename):
        cmds.refresh()
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def setupScene(self, nurbsCreationCallable):
        cmds.loadPlugin('ArubaTessellator')
        self.setHdStormRenderer()
        self.setBasicCam(10)
        nurbsResult = nurbsCreationCallable()
        cmds.refresh()
        return nurbsResult

    # Torus attributes is a superset of sphere, cone, and cylinder attributes
    def test_NurbsTorus(self):
        makeNurbNodeName = self.setupScene(cmds.torus)[1]
        self.compareSnapshot("torus_fresh.png")
        
        cmds.setAttr(makeNurbNodeName + ".startSweep", 50)
        cmds.setAttr(makeNurbNodeName + ".endSweep", 300)
        cmds.setAttr(makeNurbNodeName + ".radius", 2)
        cmds.setAttr(makeNurbNodeName + ".degree", 1)
        cmds.setAttr(makeNurbNodeName + ".sections", 12)
        cmds.setAttr(makeNurbNodeName + ".spans", 6)
        cmds.setAttr(makeNurbNodeName + ".heightRatio", 0.8)
        cmds.setAttr(makeNurbNodeName + ".minorSweep", 250)
        self.compareSnapshot("torus_modified.png")

        cmds.setAttr(makeNurbNodeName + ".useTolerance", True)
        cmds.setAttr(makeNurbNodeName + ".tolerance", 0.05)
        self.compareSnapshot("torus_tolerance.png")

    # Cube attributes is a superset of plane attributes
    def test_NurbsCube(self):
        makeNurbNodeName = self.setupScene(cmds.nurbsCube)[1]
        self.compareSnapshot("cube_fresh.png")

        cmds.setAttr(makeNurbNodeName + ".degree", 1)
        cmds.setAttr(makeNurbNodeName + ".patchesU", 2)
        cmds.setAttr(makeNurbNodeName + ".patchesV", 3)
        cmds.setAttr(makeNurbNodeName + ".width", 4)
        cmds.setAttr(makeNurbNodeName + ".lengthRatio", 2)
        cmds.setAttr(makeNurbNodeName + ".heightRatio", 3)
        self.compareSnapshot("cube_modified.png")

    def test_NurbsCircle(self):
        makeNurbNodeName = self.setupScene(cmds.circle)[1]
        self.compareSnapshot("circle_fresh.png")

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
        self.compareSnapshot("circle_modified.png")

        cmds.setAttr(makeNurbNodeName + ".useTolerance", True)
        cmds.setAttr(makeNurbNodeName + ".tolerance", 0.05)
        self.compareSnapshot("circle_tolerance.png")

        cmds.setAttr(makeNurbNodeName + ".fixCenter", False)
        self.compareSnapshot("circle_unfixedCenter.png")

    def test_NurbsSquare(self):
        makeNurbNodeName = self.setupScene(cmds.nurbsSquare)[1]
        self.compareSnapshot("square_fresh.png")

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
        self.compareSnapshot("square_modified.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
