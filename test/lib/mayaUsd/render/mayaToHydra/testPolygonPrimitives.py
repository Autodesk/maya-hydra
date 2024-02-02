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

class TestPolygonPrimitives(mtohUtils.MtohTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def compareSnapshot(self, referenceFilename):
        cmds.refresh()
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def setupScene(self, polygonCreationCallable):
        cmds.loadPlugin('modelingToolkit') # Provides polyDisc, polyPlatonic, polyGear and polySuperShape
        self.setHdStormRenderer()
        self.setBasicCam(15)
        polygonResult = polygonCreationCallable()
        cmds.refresh()
        return polygonResult
    
    # Cube attributes is a superset of plane attributes
    def test_PolygonCube(self):
        polyCreatorNodeName = self.setupScene(cmds.polyCube)[1]
        self.compareSnapshot("cube_fresh.png")

        cmds.setAttr(polyCreatorNodeName + ".width", 6)
        cmds.setAttr(polyCreatorNodeName + ".height", 10)
        cmds.setAttr(polyCreatorNodeName + ".depth", 4)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", -0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsWidth", 5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 6)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsDepth", 7)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -1)
        self.compareSnapshot("cube_modified.png")

    # Cylinder attributes is a superset of sphere and cone
    def test_PolygonCylinder(self):
        polyCreatorNodeName = self.setupScene(cmds.polyCylinder)[1]
        self.compareSnapshot("cylinder_fresh.png")
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 3)
        cmds.setAttr(polyCreatorNodeName + ".height", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsAxis", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCaps", 5)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -1)
        self.compareSnapshot("cylinder_modified.png")

        cmds.setAttr(polyCreatorNodeName + ".roundCap", True)
        self.compareSnapshot("cylinder_roundCap.png")

        cmds.setAttr(polyCreatorNodeName + ".roundCapHeightCompensation", True)
        self.compareSnapshot("cylinder_roundCapHeightCompensation.png")
        
    def test_PolygonTorus(self):
        polyCreatorNodeName = self.setupScene(cmds.polyTorus)[1]
        self.compareSnapshot("torus_fresh.png")
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 4)
        cmds.setAttr(polyCreatorNodeName + ".sectionRadius", 2)
        cmds.setAttr(polyCreatorNodeName + ".twist", 15)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 1)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsAxis", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 10)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -1)
        self.compareSnapshot("torus_modified.png")

    def test_PolygonDisc(self):
        polyCreatorNodeName = self.setupScene(cmds.polyDisc)[1]
        self.compareSnapshot("disc_fresh.png")

        cmds.setAttr(polyCreatorNodeName + ".sides", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisions", 2)
        cmds.setAttr(polyCreatorNodeName + ".radius", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)

        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 0)
        self.compareSnapshot("disc_subdivision_quads.png")

        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 1)
        self.compareSnapshot("disc_subdivision_triangles.png")

        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 2)
        self.compareSnapshot("disc_subdivision_pie.png")

        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 3)
        self.compareSnapshot("disc_subdivision_caps.png")

        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 4)
        self.compareSnapshot("disc_subdivision_circle.png")

    def test_PolygonPlatonicSolid(self):
        polyCreatorNodeName = self.setupScene(cmds.polyPlatonic)[1]
        self.compareSnapshot("platonic_fresh.png")

        cmds.setAttr(polyCreatorNodeName + ".subdivisions", 2)
        cmds.setAttr(polyCreatorNodeName + ".radius", 4)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".sphericalInflation", 0.5)

        cmds.setAttr(polyCreatorNodeName + ".primitive", 0)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 0)
        self.compareSnapshot("platonic_tetrahedron_quads.png")

        cmds.setAttr(polyCreatorNodeName + ".primitive", 1)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 1)
        self.compareSnapshot("platonic_cube_triangles.png")

        cmds.setAttr(polyCreatorNodeName + ".primitive", 2)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 2)
        self.compareSnapshot("platonic_octahedron_pie.png")

        cmds.setAttr(polyCreatorNodeName + ".primitive", 3)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 3)
        self.compareSnapshot("platonic_dodecahedron_caps.png")

        cmds.setAttr(polyCreatorNodeName + ".primitive", 4)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionMode", 1)
        self.compareSnapshot("platonic_icosahedron_triangles.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
