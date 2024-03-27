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

import platform

class TestCurveTools(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    POINTS = [(0,-5,10), (-10, 0, 0), (0, 5, -10), (10, 0, 0), (0, -5, 5), (-5,0,0), (0,5,-5), (5, 0, 0), (0,0,0)]
    CUSTOM_KNOTS = [0,1,2,3,4,5,6,7,8,9,10]

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def compareSnapshot(self, referenceFilename, cameraDistance=15):
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

    def test_CurveControlVertices(self):
        curve = cmds.curve(point=self.POINTS)
        self.compareSnapshot("curveControlVertices_basic.png")
        cmds.delete(curve)

        curve = cmds.curve(point=self.POINTS, knot=self.CUSTOM_KNOTS)
        self.compareSnapshot("curveControlVertices_customKnots.png")
        cmds.delete(curve)

        curve = cmds.curve(point=self.POINTS, degree=1)
        self.compareSnapshot("curveControlVertices_degree1.png")
        cmds.delete(curve)

        curve = cmds.curve(point=self.POINTS, degree=2)
        self.compareSnapshot("curveControlVertices_degree2.png")
        cmds.delete(curve)

        curve = cmds.curve(point=self.POINTS, degree=5)
        self.compareSnapshot("curveControlVertices_degree5.png")
        cmds.delete(curve)

        curve = cmds.curve(point=self.POINTS, bezier=True)
        self.compareSnapshot("curveControlVertices_bezier.png")
        cmds.delete(curve)

    def test_CurveEditPoints(self):
        curve = cmds.curve(editPoint=self.POINTS)
        self.compareSnapshot("curveEditPoints_basic.png")
        cmds.delete(curve)

        curve = cmds.curve(editPoint=self.POINTS, degree=1)
        self.compareSnapshot("curveEditPoints_degree1.png")
        cmds.delete(curve)

        curve = cmds.curve(editPoint=self.POINTS, degree=2)
        self.compareSnapshot("curveEditPoints_degree2.png")
        cmds.delete(curve)

        curve = cmds.curve(editPoint=self.POINTS, degree=5)
        self.compareSnapshot("curveEditPoints_degree5.png")
        cmds.delete(curve)

        curve = cmds.curve(editPoint=self.POINTS, bezier=True)
        self.compareSnapshot("curveEditPoints_bezier.png")
        cmds.delete(curve)

    def test_TwoPointCircularArc(self):
        arcNode = self.createCircularArc("makeTwoPointCircularArc")
        self.compareSnapshot("twoPointCircularArc_fresh.png", 5)

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
        self.compareSnapshot("threePointCircularArc_fresh.png", 5)

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
