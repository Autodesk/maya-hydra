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
import functools
import mayaUsd_createStageWithNewLayer
import mayaUsd.lib
import mayaUtils
import mtohUtils
import testUtils
import usdUtils

# Note : the order of the bit flags does not correspond to the order 
# of the options in the "Show" -> "Viewport" UI.
kExcludeNurbsCurves        = 1 << 0
kExcludeNurbsSurfaces      = 1 << 1
kExcludeMeshes             = 1 << 2
kExcludePlanes             = 1 << 3
kExcludeLights             = 1 << 4
kExcludeCameras            = 1 << 5
kExcludeJoints             = 1 << 6
kExcludeIkHandles          = 1 << 7
kExcludeDeformers          = 1 << 8
kExcludeDynamics           = 1 << 9
kExcludeParticleInstancers = 1 << 10
kExcludeLocators           = 1 << 11
kExcludeDimensions         = 1 << 12
kExcludeSelectHandles      = 1 << 13
kExcludePivots             = 1 << 14
kExcludeTextures           = 1 << 15
kExcludeGrid               = 1 << 16
kExcludeCVs                = 1 << 17
kExcludeHulls              = 1 << 18
kExcludeStrokes            = 1 << 19
kExcludeSubdivSurfaces     = 1 << 20
kExcludeFluids             = 1 << 21
kExcludeFollicles          = 1 << 22
kExcludeHairSystems        = 1 << 23
kExcludeImagePlane         = 1 << 24
kExcludeNCloths            = 1 << 25
kExcludeNRigids            = 1 << 26
kExcludeDynamicConstraints = 1 << 27
kExcludeManipulators       = 1 << 28
kExcludeNParticles         = 1 << 29
kExcludeMotionTrails       = 1 << 30
kExcludeHoldOuts           = 1 << 31
kExcludePluginShapes       = 1 << 32
kExcludeHUD                = 1 << 33
kExcludeClipGhosts         = 1 << 34
kExcludeGreasePencils      = 1 << 35
kExcludeControllers        = 1 << 36
kExcludeBluePencil         = 1 << 37

class TestViewportFilters(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.5

    CURVE_POINTS = [(0,-5,10), (-10, 0, 0), (0, 5, -10), (10, 0, 0), (0, -5, 5), (-5,0,0), (0,5,-5), (5, 0, 0), (0,0,0)]

    def setUp(self):
        super(TestViewportFilters, self).setUp()
        # Include everything
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, excludeObjectMask=0)

    # This is a somewhat hacky workaround around the limitation that line width is currently (2024-05-02)
    # not supported by mayaHydra : we stack multiple instances of the same item very close together.
    # If we just used a single wireframe, its line width would be 1-pixel wide, which is much too thin
    # to be reliable : by having multiple of them, it can take up more of the screen and provide more
    # robust image comparison.
    def stackInstances(self, itemCreationCallable, nbInstances, offset):
        for i in range(nbInstances):
            itemCreationCallable()
            cmds.move(*[i * o for o in offset], relative=True)
            cmds.select(clear=True)

    def compareSnapshot(self, referenceFilename, cameraDistance):
        self.setBasicCam(cameraDistance)
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def checkFilter(self, name, exclusionMask, cameraDistance=15):
        activeViewport = mayaUtils.activeModelPanel()
        oldMask = cmds.modelEditor(activeViewport, query=True, excludeObjectMask=True)

        # Should start by being included
        self.assertEqual(oldMask & exclusionMask, 0)
        self.compareSnapshot(name + "_included.png", cameraDistance)

        # Exclude and ensure the items are no longer displayed
        newMask = oldMask | exclusionMask
        cmds.modelEditor(activeViewport, edit=True, excludeObjectMask=newMask)
        self.compareSnapshot(name + "_excluded.png", cameraDistance)

        # Restore old mask
        cmds.modelEditor(activeViewport, edit=True, excludeObjectMask=oldMask)

    # --- Maya data ---

    # TODO : Construction planes (not working in Hydra as of 2024-05-03)

    def test_Dimensions(self):
        cmds.distanceDimension(startPoint=[-1, -2, 3], endPoint=[3, 2, -1])
        cmds.select(clear=True)
        self.checkFilter("dimensions", kExcludeDimensions, 6)

    def test_NurbsCurves(self):
        self.stackInstances(cmds.circle, 50, [0, 0, 0.005])
        cmds.select(clear=True)
        self.checkFilter("nurbs_curves", kExcludeNurbsCurves, 2)

    # TODO : Nurbs CVs (not working in Hydra as of 2024-05-03)

    def test_NurbsSurfaces(self):
        cmds.sphere()
        cmds.select(clear=True)
        self.checkFilter("nurbs_surfaces", kExcludeNurbsSurfaces, 3)

    def test_NurbsHulls(self):
        def curveCreator():
            curve = cmds.curve(point=self.CURVE_POINTS)
            cmds.setAttr(curve + ".dispHull", True)
        self.stackInstances(curveCreator, 50, [0, 0.005, 0.005])
        cmds.select(clear=True)
        self.checkFilter("nurbs_hull", kExcludeHulls, 10)

    def test_Polygons(self):
        cmds.polySphere()
        cmds.select(clear=True)
        self.checkFilter("polygons", kExcludeMeshes, 3)

    def test_SubdivisionSurfaces(self):
        cylinderNode = cmds.polyCylinder()[0]
        cmds.polyToSubdiv(cylinderNode)
        cmds.select(clear=True)
        cmds.delete(cylinderNode)
        self.checkFilter("subdivision_surfaces", kExcludeSubdivSurfaces, 3)

    # TODO : Clip ghosts (filtering option not working as of 2024-05-03)

    def test_Controllers(self):
        def controllerCreator():
            circle = cmds.circle()[0]
            cmds.select(circle)
            cmds.controller()
        self.stackInstances(controllerCreator, 50, [0, 0, 0.005])
        cmds.select(clear=True)
        self.checkFilter("controllers", kExcludeControllers, 2)

    def test_Deformers(self):
        self.stackInstances(cmds.lattice, 50, [0, 0.005, 0])
        cmds.select(clear=True)
        self.checkFilter("deformers", kExcludeDeformers, 2)
    
    # TODO : Selection Handles (not working in Hydra as of 2024-05-03)
    # To display them (currently only working in VP2), select an object for which to display 
    # a selection handle, and toggle Display -> Transform Display -> Selection Handles.

    def test_IkHandles(self):
        def ikHandleCreator():
            joint1 = cmds.joint(position=[0,-2,0])
            joint2 = cmds.joint(position=[0,2,0])
            cmds.joint(joint1, edit=True, zeroScaleOrient=True, orientJoint="xyz", secondaryAxisOrient="yup")
            cmds.joint(joint2, edit=True, zeroScaleOrient=True, orientJoint="none")
            ikHandle = cmds.ikHandle(startJoint=joint1, endEffector=joint2, solver="ikRPsolver")[0]
            cmds.select([joint1, ikHandle], replace=True)
        self.stackInstances(ikHandleCreator, 50, [0, 0, 0.005])
        cmds.select(all=True)
        self.checkFilter("ikHandles", kExcludeIkHandles, 5)

    def test_Joints(self):
        def jointCreator():
            joint1 = cmds.joint(position=[0,-2,0])
            joint2 = cmds.joint(position=[0,2,0])
            cmds.joint(joint1, edit=True, zeroScaleOrient=True, orientJoint="xyz", secondaryAxisOrient="yup")
            cmds.joint(joint2, edit=True, zeroScaleOrient=True, orientJoint="none")
            cmds.select([joint1], replace=True)
        self.stackInstances(jointCreator, 50, [0, 0, 0.005])
        cmds.select(clear=True)
        self.checkFilter("joints", kExcludeJoints, 5)

    def test_Locators(self):
        self.stackInstances(cmds.spaceLocator, 50, [0, 0.005, 0])
        cmds.select(clear=True)
        self.checkFilter("locators", kExcludeLocators, 2)
    
    def test_MotionTrails(self):
        mayaUtils.openTestScene("testViewportFilters", "motionTrails.ma")
        self.setHdStormRenderer()
        self.checkFilter("motion_trails", kExcludeMotionTrails, 2)

    # TODO : Pivots (not working in Hydra as of 2024-05-03)
    # To display them (currently only working in VP2), see the attributes under the 
    # "Pivots" category on a transform.

    def test_Cameras(self):
        self.stackInstances(cmds.camera, 50, [0, 0.005, 0])
        cmds.select(clear=True)
        self.checkFilter("cameras", kExcludeCameras, 3)

    def test_ImagePlanes(self):
        # Note : the images themselves are not working in Hydra as of 2024-05-03, but the selection highlight does.
        self.stackInstances(cmds.imagePlane, 50, [0, 0, 0.005])
        cmds.select(all=True)
        self.checkFilter("image_planes", kExcludeImagePlane, 8)

    def test_Lights(self):
        self.stackInstances(cmds.directionalLight, 50, [0, 0.005, 0])
        cmds.select(clear=True)
        self.checkFilter("lights", kExcludeLights, 3)

    def test_Strokes(self):
        mayaUtils.openTestScene("testViewportFilters", "strokes.ma")
        self.setHdStormRenderer()
        self.checkFilter("strokes", kExcludeStrokes, 4)

    # TODO : Texture Placements (not working in Hydra as of 2024-05-03)

    # --- USD data ---

    def test_UsdPolygons(self):
        from pxr import UsdGeom

        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()

        capsuleName = "UsdCapsule"
        capsuleXform = UsdGeom.Xform.Define(stage, "/" + capsuleName + "Xform")
        capsuleXform.AddTranslateOp().Set(value=(6, 0, 0))
        UsdGeom.Capsule.Define(stage, str(capsuleXform.GetPath()) + "/" + capsuleName)
        cmds.select(clear=True)

        coneName = "UsdCone"
        coneXform = UsdGeom.Xform.Define(stage, "/" + coneName + "Xform")
        coneXform.AddTranslateOp().Set(value=(2, 0, -2))
        UsdGeom.Cone.Define(stage, str(coneXform.GetPath()) + "/" + coneName)
        cmds.select(clear=True)

        cubeName = "UsdCube"
        cubeXform = UsdGeom.Xform.Define(stage, "/" + cubeName + "Xform")
        cubeXform.AddTranslateOp().Set(value=(-3, 0, -3))
        UsdGeom.Cube.Define(stage, str(cubeXform.GetPath()) + "/" + cubeName)
        cmds.select(clear=True)

        cylinderName = "UsdCylinder"
        cylinderXform = UsdGeom.Xform.Define(stage, "/" + cylinderName + "Xform")
        cylinderXform.AddTranslateOp().Set(value=(-2, 0, 2))
        UsdGeom.Cylinder.Define(stage, str(cylinderXform.GetPath()) + "/" + cylinderName)
        cmds.select(clear=True)

        sphereName = "UsdSphere"
        sphereXform = UsdGeom.Xform.Define(stage, "/" + sphereName + "Xform")
        sphereXform.AddTranslateOp().Set(value=(0, 0, 6))
        UsdGeom.Sphere.Define(stage, str(sphereXform.GetPath()) + "/" + sphereName)
        cmds.select(clear=True)

        cmds.polyTorus()
        cmds.move(3, 0, 3)
        cmds.select(clear=True)

        cmds.refresh()
        self.checkFilter("polygons_USD", kExcludeMeshes, 10)

    def test_UsdNurbsCurves(self):
        def createUsdCurve(stagePath):
            circleName = cmds.circle()
            usdCircleName = mayaUsd.lib.PrimUpdaterManager.duplicate(cmds.ls(circleName[0], long=True)[0], stagePath)
            cmds.select(circleName)
            cmds.delete()
            cmds.select(usdCircleName)
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        self.stackInstances(functools.partial(createUsdCurve, stagePath), 50, [0, 0, 0.005])
        self.checkFilter("nurbsCurves_USD", kExcludeNurbsCurves, 2)

    def test_UsdNurbsPatches(self):
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        torusName = cmds.torus(sections=20, spans=10, heightRatio=0.5)
        mayaUsd.lib.PrimUpdaterManager.duplicate(cmds.ls(torusName[0], long=True)[0], stagePath)
        cmds.select(torusName)
        cmds.delete()
        cmds.select(clear=True)
        self.checkFilter("nurbsPatches_USD", kExcludeNurbsSurfaces, 3)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
