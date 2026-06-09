# Copyright 2026 Autodesk
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


class TestMeshGeomSubsets(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    # Create a shading group named 'name' driven by a fresh lambert shader.
    # The shading group node name becomes the leaf of the Hydra material path, so the
    # C++ test can identify which material a geomSubset is bound to.
    def _makeShadingGroup(self, name):
        shader = cmds.shadingNode("lambert", asShader=True, name=name + "Mtl")
        sg = cmds.sets(name=name, renderable=True, noSurfaceShader=True, empty=True)
        cmds.connectAttr(shader + ".outColor", sg + ".surfaceShader", force=True)
        return sg

    # Multi-material cube: whole-object shaderASG plus a per-face shaderBSG on faces 0-2.
    def setupPerFaceScene(self):
        cmds.file(new=True, force=True)
        cmds.polyCube(name="multiCube")
        mesh_shape = cmds.ls("multiCubeShape", long=True)[0]
        sgA = self._makeShadingGroup("shaderASG")
        sgB = self._makeShadingGroup("shaderBSG")
        cmds.sets("multiCubeShape", edit=True, forceElement=sgA)
        cmds.sets("multiCubeShape.f[0:2]", edit=True, forceElement=sgB)
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhGeomSubsetMeshShape", mesh_shape))
        cmds.refresh()

    # Single-material cube: one whole-object shading group, no per-face assignment.
    def setupSingleScene(self):
        cmds.file(new=True, force=True)
        cmds.polyCube(name="multiCube")
        mesh_shape = cmds.ls("multiCubeShape", long=True)[0]
        sgA = self._makeShadingGroup("shaderASG")
        cmds.sets("multiCubeShape", edit=True, forceElement=sgA)
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhGeomSubsetMeshShape", mesh_shape))
        cmds.refresh()

    # Non-mesh shape: a NURBS sphere with a single shading group.
    def setupNurbsScene(self):
        cmds.file(new=True, force=True)
        cmds.sphere(name="nurbsBall")
        nurbs_shape = cmds.ls("nurbsBallShape", long=True)[0]
        sgA = self._makeShadingGroup("shaderASG")
        cmds.sets("nurbsBallShape", edit=True, forceElement=sgA)
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhGeomSubsetMeshShape", nurbs_shape))
        cmds.refresh()

    # What: per-face material assignments create faceSet geomSubsets bound to materials.
    # How: build the multi-material cube, then run the C++ inspection test.
    # Expect: the faces {0,1,2} subset binds to shaderBSG; no full-coverage subset.
    def test_perFaceCreatesGeomSubsets(self):
        self.setupPerFaceScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MultiMaterialMesh.PerFaceCreatesGeomSubsets")

    # What: a single whole-object assignment must not create geomSubsets.
    # How: build the single-material cube, then run the C++ inspection test.
    # Expect: no geomSubset prims under the mesh.
    def test_singleAssignmentNoGeomSubset(self):
        self.setupSingleScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MultiMaterialMesh.SingleAssignmentNoGeomSubset")

    # What: non-mesh shapes must not produce geomSubsets.
    # How: build a NURBS sphere with a material, then run the C++ inspection test.
    # Expect: no geomSubset prims carrying the NURBS shape name; no crash.
    def test_nonMeshNoGeomSubset(self):
        self.setupNurbsScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MultiMaterialMesh.NonMeshNoGeomSubset")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
