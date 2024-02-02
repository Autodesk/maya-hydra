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

    def compareSnapshot(self, referenceFilename, cameraDistance=15):
        self.setBasicCam(cameraDistance)
        cmds.refresh()
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def setupScene(self, polygonCreationCallable):
        cmds.loadPlugin('modelingToolkit') # Provides polyDisc, polyPlatonic, polyGear and polySuperShape
        self.setHdStormRenderer()
        polygonResult = polygonCreationCallable()
        cmds.refresh()
        return polygonResult
    
    def getSuperShapeCreationCallable(self, shapeType):
        # Base parameters to have an actually visible mesh
        return lambda: cmds.polySuperShape(
            shape=shapeType,
            radius=1,
            horizontalDivisions=1,
            verticalDivisions=1,
            mergeVertices=True)
    
    # Cube attributes is a superset of plane attributes
    def test_PolygonCube(self):
        polyCreatorNodeName = self.setupScene(cmds.polyCube)[1]
        self.compareSnapshot("cube_fresh.png", 5)

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
        self.compareSnapshot("cylinder_fresh.png", 5)
        
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
        self.compareSnapshot("torus_fresh.png", 5)
        
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
        self.compareSnapshot("disc_fresh.png", 5)

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
        self.compareSnapshot("platonic_fresh.png", 5)

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

    def test_PolygonPyramid(self):
        polyCreatorNodeName = self.setupScene(cmds.polyPyramid)[1]
        self.compareSnapshot("pyramid_fresh.png", 5)

        cmds.setAttr(polyCreatorNodeName + ".sideLength", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", -0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCaps", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", -3)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -0.25)

        cmds.setAttr(polyCreatorNodeName + ".numberOfSides", 3)
        self.compareSnapshot("pyramid_3sides.png")

        cmds.setAttr(polyCreatorNodeName + ".numberOfSides", 4)
        self.compareSnapshot("pyramid_4sides.png")

        cmds.setAttr(polyCreatorNodeName + ".numberOfSides", 5)
        self.compareSnapshot("pyramid_5sides.png")

    def test_PolygonPrism(self):
        polyCreatorNodeName = self.setupScene(cmds.polyPrism)[1]
        self.compareSnapshot("prism_fresh.png", 5)

        cmds.setAttr(polyCreatorNodeName + ".length", 4)
        cmds.setAttr(polyCreatorNodeName + ".sideLength", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCaps", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 0.25)
        cmds.setAttr(polyCreatorNodeName + ".axisY", -3)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -0.5)

        cmds.setAttr(polyCreatorNodeName + ".numberOfSides", 5)
        self.compareSnapshot("prism_5sides.png")

        cmds.setAttr(polyCreatorNodeName + ".numberOfSides", 8)
        self.compareSnapshot("prism_8sides.png")

    def test_PolygonPipe(self):
        polyCreatorNodeName = self.setupScene(cmds.polyPipe)[1]
        self.compareSnapshot("pipe_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 3)
        cmds.setAttr(polyCreatorNodeName + ".height", 5)
        cmds.setAttr(polyCreatorNodeName + ".thickness", 1)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsAxis", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsHeight", 5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCaps", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -0.25)
        self.compareSnapshot("pipe_modified.png")

        cmds.setAttr(polyCreatorNodeName + ".roundCap", True)
        self.compareSnapshot("pipe_roundCap.png")

        cmds.setAttr(polyCreatorNodeName + ".roundCapHeightCompensation", True)
        self.compareSnapshot("pipe_roundCapHeightCompensation.png")

    def test_PolygonHelix(self):
        polyCreatorNodeName = self.setupScene(cmds.polyHelix)[1]
        self.compareSnapshot("helix_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".coils", 2)
        cmds.setAttr(polyCreatorNodeName + ".height", 5)
        cmds.setAttr(polyCreatorNodeName + ".width", 5)
        cmds.setAttr(polyCreatorNodeName + ".radius", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsAxis", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCoil", 10)
        cmds.setAttr(polyCreatorNodeName + ".subdivisionsCaps", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -0.25)
        self.compareSnapshot("helix_modified.png")

        cmds.setAttr(polyCreatorNodeName + ".roundCap", True)
        self.compareSnapshot("helix_roundCap.png")

        cmds.setAttr(polyCreatorNodeName + ".direction", 0)
        self.compareSnapshot("helix_clockwise.png")

    def test_PolygonGear(self):
        polyCreatorNodeName = self.setupScene(cmds.polyGear)[1]
        self.compareSnapshot("gear_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".sides", 10)
        cmds.setAttr(polyCreatorNodeName + ".radius", 5)
        cmds.setAttr(polyCreatorNodeName + ".internalRadius", 2)
        cmds.setAttr(polyCreatorNodeName + ".height", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightDivisions", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".gearSpacing", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".gearOffset", 2)
        cmds.setAttr(polyCreatorNodeName + ".gearTip", 1)
        cmds.setAttr(polyCreatorNodeName + ".gearMiddle", 0.75)
        cmds.setAttr(polyCreatorNodeName + ".twist", 10)
        cmds.setAttr(polyCreatorNodeName + ".taper", 1.25)
        self.compareSnapshot("gear_modified.png")

    def test_PolygonSoccerBall(self):
        polyCreatorNodeName = self.setupScene(cmds.polyPrimitive)[1]
        self.compareSnapshot("soccerball_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 4)
        cmds.setAttr(polyCreatorNodeName + ".sideLength", 2)
        # Radius and side length are tied together, changing one changes the other
        self.assertAlmostEqual(cmds.getAttr(polyCreatorNodeName + ".radius"), 4.955, 3)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".axisX", 1)
        cmds.setAttr(polyCreatorNodeName + ".axisY", 2)
        cmds.setAttr(polyCreatorNodeName + ".axisZ", -1)
        self.compareSnapshot("soccerball_modified.png")

    def test_PolygonSuperEllipse(self):
        polyCreatorNodeName = self.setupScene(self.getSuperShapeCreationCallable("SuperEllipse"))[1]
        self.compareSnapshot("superEllipse_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".horizontalDivisions", 4)
        cmds.setAttr(polyCreatorNodeName + ".verticalDivisions", 4)
        cmds.setAttr(polyCreatorNodeName + ".ellipse0", 1.25)
        cmds.setAttr(polyCreatorNodeName + ".ellipse1", 1.25)
        self.compareSnapshot("superEllipse_modified.png")

    def test_PolygonSphericalHarmonics(self):
        polyCreatorNodeName = self.setupScene(self.getSuperShapeCreationCallable("SphericalHarmonics"))[1]
        self.compareSnapshot("sphericalHarmonics_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".horizontalDivisions", 4)
        cmds.setAttr(polyCreatorNodeName + ".verticalDivisions", 4)
        cmds.setAttr(polyCreatorNodeName + ".harmonics0", 1)
        cmds.setAttr(polyCreatorNodeName + ".harmonics1", 2)
        cmds.setAttr(polyCreatorNodeName + ".harmonics2", 1)
        cmds.setAttr(polyCreatorNodeName + ".harmonics3", 2)
        cmds.setAttr(polyCreatorNodeName + ".harmonics4", 0.1)
        cmds.setAttr(polyCreatorNodeName + ".harmonics5", 2)
        cmds.setAttr(polyCreatorNodeName + ".harmonics6", 0.1)
        cmds.setAttr(polyCreatorNodeName + ".harmonics7", 2)
        self.compareSnapshot("sphericalHarmonics_modified.png")

    def test_PolygonUltra(self):
        polyCreatorNodeName = self.setupScene(self.getSuperShapeCreationCallable("UltraShape"))[1]
        self.compareSnapshot("ultra_fresh.png", 5)
        
        cmds.setAttr(polyCreatorNodeName + ".radius", 5)
        cmds.setAttr(polyCreatorNodeName + ".heightBaseline", 0.5)
        cmds.setAttr(polyCreatorNodeName + ".horizontalDivisions", 4)
        cmds.setAttr(polyCreatorNodeName + ".verticalDivisions", 4)
        # Horizontal attributes
        cmds.setAttr(polyCreatorNodeName + ".ultra0", 2) # Horizontal Multiplier 1
        cmds.setAttr(polyCreatorNodeName + ".ultra1", 2) # Horizontal Exponent 1
        cmds.setAttr(polyCreatorNodeName + ".ultra2", 2) # Horizontal Exponent 2
        cmds.setAttr(polyCreatorNodeName + ".ultra3", 0.25) # Horizontal Mixer
        cmds.setAttr(polyCreatorNodeName + ".ultra4", 3) # Horizontal Multiplier 2
        cmds.setAttr(polyCreatorNodeName + ".ultra5", 3) # Horizontal Exponent 3
        cmds.setAttr(polyCreatorNodeName + ".ultra6", 3) # Horizontal Exponent 4
        cmds.setAttr(polyCreatorNodeName + ".ultra7", 1.5) # Horizontal Exponent 5
        # Vertical attributes
        cmds.setAttr(polyCreatorNodeName + ".ultra8", 3) # Vertical Multiplier 1
        cmds.setAttr(polyCreatorNodeName + ".ultra9", 3) # Vertical Exponent 1
        cmds.setAttr(polyCreatorNodeName + ".ultra10", 3) # Vertical Exponent 2
        cmds.setAttr(polyCreatorNodeName + ".ultra11", 0.75) # Vertical Mixer
        cmds.setAttr(polyCreatorNodeName + ".ultra12", 2) # Vertical Multiplier 2
        cmds.setAttr(polyCreatorNodeName + ".ultra13", 2) # Vertical Exponent 3
        cmds.setAttr(polyCreatorNodeName + ".ultra14", 2) # Vertical Exponent 4
        cmds.setAttr(polyCreatorNodeName + ".ultra15", 1.5) # Vertical Exponent 5
        self.compareSnapshot("ultra_modified.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
