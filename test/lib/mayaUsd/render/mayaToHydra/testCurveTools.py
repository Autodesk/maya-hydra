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

class TestCurveTools(mtohUtils.MtohTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.75

    POINTS = [(0,-5,10), (-10, 0, 0), (0, 5, -10), (10, 0, 0), (0, -5, 5), (-5,0,0), (0,5,-5), (5, 0, 0), (0,0,0)]
    CUSTOM_KNOTS = [0,1,2,3,4,5,6,7,8,9,10]

    def compareSnapshot(self, referenceFilename, cameraDistance=10):
        self.setBasicCam(cameraDistance)
        cmds.refresh()
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def setUp(self):
        super(TestCurveTools, self).setUp()
        self.setHdStormRenderer()
        cmds.refresh()
    
    def createCircularArc(self, makeCircularArcNodeType):
        arcNode = cmds.createNode(makeCircularArcNodeType)
        curveNode = cmds.createNode("nurbsCurve")
        cmds.connectAttr(arcNode + ".oc", curveNode + ".cr")
        return arcNode

    def test_CurveBasic(self):
        cmds.curve(point=self.POINTS)
        self.compareSnapshot("curveBasic.png")

    def test_CurveCustomKnots(self):
        cmds.curve(point=self.POINTS, knot=self.CUSTOM_KNOTS)
        self.compareSnapshot("curveCustomKnots.png")

    def test_CurveDegree1(self):
        cmds.curve(point=self.POINTS, degree=1)
        self.compareSnapshot("curveDegree1.png")

    def test_CurveDegree2(self):
        cmds.curve(point=self.POINTS, degree=2)
        self.compareSnapshot("curveDegree2.png")

    def test_CurveBezier(self):
        cmds.curve(point=self.POINTS, bezier=True)
        self.compareSnapshot("curveBezier.png")

    def test_TwoPointCircularArc(self):
        arcNode = self.createCircularArc("makeTwoPointCircularArc")
        self.compareSnapshot("twoPointCircularArc_fresh.png")

        cmds.setAttr(arcNode + ".point1X", 0)
        cmds.setAttr(arcNode + ".point1Y", 0)
        cmds.setAttr(arcNode + ".point1Z", 5)
        cmds.setAttr(arcNode + ".point2X", 5)
        cmds.setAttr(arcNode + ".point2Y", 0)
        cmds.setAttr(arcNode + ".point2Z", -5)
        cmds.setAttr(arcNode + ".directionVectorX", -1)
        cmds.setAttr(arcNode + ".directionVectorY", 0)
        cmds.setAttr(arcNode + ".directionVectorZ", 0)
        cmds.setAttr(arcNode + ".radius", 6)
        cmds.setAttr(arcNode + ".toggleArc", False)
        cmds.setAttr(arcNode + ".degree", 1)
        cmds.setAttr(arcNode + ".sections", 4)
        self.compareSnapshot("twoPointCircularArc_linear.png")

        cmds.setAttr(arcNode + ".toggleArc", True)
        self.compareSnapshot("twoPointCircularArc_toggleArc.png")

        cmds.setAttr(arcNode + ".degree", 3)
        self.compareSnapshot("twoPointCircularArc_cubic.png")

    def test_ThreePointCircularArc(self):
        arcNode = self.createCircularArc("makeThreePointCircularArc")
        self.compareSnapshot("threePointCircularArc_fresh.png")

        cmds.setAttr(arcNode + ".point1X", -1)
        cmds.setAttr(arcNode + ".point1Y", -1)
        cmds.setAttr(arcNode + ".point1Z", 1)
        cmds.setAttr(arcNode + ".point2X", 2)
        cmds.setAttr(arcNode + ".point2Y", 2)
        cmds.setAttr(arcNode + ".point2Z", -2)
        cmds.setAttr(arcNode + ".point3X", 3)
        cmds.setAttr(arcNode + ".point3Y", -3)
        cmds.setAttr(arcNode + ".point3Z", 3)
        cmds.setAttr(arcNode + ".degree", 1)
        cmds.setAttr(arcNode + ".sections", 5)
        self.compareSnapshot("threePointCircularArc_linear.png")

        cmds.setAttr(arcNode + ".degree", 3)
        self.compareSnapshot("threePointCircularArc_cubic.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
