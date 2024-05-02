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
import maya.mel as mel
import fixturesUtils
import mayaUtils
import mtohUtils

# Note : the order of the bit flags does not correspond to the order 
# of the options in the "Show" -> "Viewport" UI at all.
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
kExcludeHoldOuts		   = 1 << 31
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

    # This is a somewhat hacky workaround around the limitation that line width is currently (2024-05-02)
    # not supported by mayaHydra : we stack multiple instances of the same item very close together.
    # If we just used a single wireframe, its line width would be 1-pixel wide, which is much too thin
    # to be reliable : by having multiple of them, it can take up more of the screen, and it reduces
    # the impact of anti-aliasing on the image comparison.
    def stackInstances(self, itemCreationCallable, nbInstances, offset):
        for i in range(nbInstances):
            itemCreationCallable()
            cmds.move(*[i * o for o in offset])

    def compareSnapshot(self, referenceFilename, cameraDistance):
        self.setBasicCam(cameraDistance)
        cmds.refresh(force=True)
        self.assertSnapshotClose(referenceFilename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def checkFilter(self, name, exclusionMask, cameraDistance=15):
        activeViewport = mayaUtils.activeModelPanel()
        oldMask = cmds.modelEditor(activeViewport, query=True, excludeObjectMask=True)
        # Should start by being excluded
        self.assertNotEqual(oldMask & exclusionMask, 0)

        # Reuse the same image for all checks when an item is excluded. Since we start by excluding everything,
        # exclusion checks should all result in an empty image.
        self.compareSnapshot("excluded.png", cameraDistance)

        # Enable the given items and check that they are displayed
        newMask = oldMask & (~exclusionMask)
        cmds.modelEditor(activeViewport, edit=True, excludeObjectMask=newMask)
        self.compareSnapshot(name + ".png", cameraDistance)

        # Restore old mask
        cmds.modelEditor(activeViewport, edit=True, excludeObjectMask=oldMask)

    def test_ViewportFilters(self):
        # Start by excluding everything
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, excludeObjectMask=0xffffffff)
        
        # Increase line width so they take up a larger portion of the images
        cmds.displayPref(lineWidth=9)
        mel.eval('displayPref -lineWidth 9')

        cmds.polySphere()
        cmds.select(clear=True)
        self.checkFilter("polygons", kExcludeMeshes, 3)

        self.stackInstances(cmds.circle, 50, [0, 0, 0.005])
        cmds.select(clear=True)
        self.checkFilter("nurbs_curves", kExcludeNurbsCurves, 2)

        self.stackInstances(cmds.directionalLight, 50, [0, 0.005, 0])
        cmds.select(clear=True)
        self.checkFilter("lights", kExcludeLights, 3)

        cmds.sphere()
        cmds.select(clear=True)
        self.checkFilter("nurbs_surfaces", kExcludeNurbsSurfaces, 3)

        cmds.distanceDimension(startPoint=[-1, -2, 3], endPoint=[3, 2, -1])
        cmds.select(clear=True)
        self.checkFilter("dimensions", kExcludeDimensions, 6)

        curve = cmds.curve(point=self.CURVE_POINTS)
        cmds.setAttr(curve + ".dispHull", True)
        cmds.select(clear=True)
        # Hulls are dependent on curves being visible in the first place
        self.checkFilter("nurbs_hull", kExcludeHulls | kExcludeNurbsCurves, 3)

        cylinderNode = cmds.polyCylinder()[0]
        cmds.polyToSubdiv(cylinderNode)
        cmds.select(clear=True)
        self.checkFilter("subdivision_surfaces", kExcludeSubdivSurfaces, 3)

        # TODO
        # Here are some of the object types mayaHydra does not support currently (2024-05-02) :
        # - Construction Planes
        # - NURBS CVs
        # - 
        # And some that are supported by secondary graphics, and not through Hydra itself :
        # - 

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
