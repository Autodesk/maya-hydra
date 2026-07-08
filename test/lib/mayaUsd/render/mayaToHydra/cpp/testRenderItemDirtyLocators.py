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
# Python wrapper for testDirtyLocators.cpp in render-items mode (no mesh adapter env var).
# Runs RenderItemDirtyLocators and ExtCompGate suites via mayaHydraCppTest to verify
# granular dirty locators on the MRenderItem adapter path.
#
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded


class TestRenderItemDirtyLocators(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def setupScene(self):
        cmds.file(new=True, force=True)
        cmds.polyCube(name="testCube")
        mesh_shape = cmds.ls("testCubeShape", long=True)[0]
        mesh_transform = cmds.ls("testCube", long=True)[0]
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhMeshShape", mesh_shape))
        cmds.optionVar(stringValue=("mhMeshTransform", mesh_transform))
        cmds.refresh()

    # What: vertex deformation should dirty granular primvars but never broad primvars.
    #       Render items mode may also emit mesh/topology when Maya flags topoChanged on
    #       component moves — unlike the mesh adapter path.
    # How: build a cube scene (render items mode), move one vertex via C++, inspect dirty locators.
    # Expect: points + uvs + meshTopology; no broad primvars, no extComputationPrimvars.
    def test_deformationVertexMoveEmitsGranularPrimvarsNotBroadPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.DeformationVertexMoveEmitsGranularPrimvarsNotBroadPrimvars")

    # What: topology edits (face extrude) should dirty mesh topology but NOT broadPrimvars or
    #       extComputationPrimvars in render items mode.  The render item adapter intentionally
    #       skips both: broadPrimvars would subsume granular locators; extCompPrimvars is
    #       reserved for skinning/blendshape, not connectivity changes.
    # How: build a cube scene (render items mode), extrude a face via C++, inspect dirty locators.
    # Expect: meshTopology + points; no broadPrimvars, no extComputationPrimvars.
    def test_topologyExtrudeEmitsTopologyNotBroadPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.TopologyExtrudeEmitsTopologyNotBroadPrimvars")

    # What: a dynamic attribute change on a mesh (rprim) must emit extComputationPrimvars,
    #       even in render items mode.  The render item adapter registers the same
    #       attribute-changed callback as the mesh adapter, gated on IsRprim().
    # How: build a cube scene (render items mode), addAttr+setAttr a dynamic float via C++,
    #      inspect dirty locators.
    # Expect: extCompPrimvars=true.
    def test_dynamicAttrOnMeshEmitsExtComputationPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.DynamicAttrOnMeshEmitsExtComputationPrimvars")

    # What: intermediateObject toggle should dirty visibility and refresh the Hydra schema.
    # How: render items mode (default viewport); toggle shape intermediateObject via C++.
    # Expect: visibility locator on each toggle; visibility schema matches intermediateObject.
    #       Exercises UpdateFromDelta (MVS_changedVisibility), not dag-adapter plug callbacks.
    def test_intermediateObjectToggleEmitsVisibilityAndUpdatesSchema(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators."
                f"IntermediateObjectToggleEmitsVisibilityAndUpdatesSchema")

    # What: edge flip retriangulates without changing position-buffer size; topology locators must
    #       emit when index connectivity changes (same vertex count, topo+geom flags from Maya).
    # How: render items mode, polyFlipEdge on a cube edge via C++, inspect dirty locators.
    # Expect: meshTopology=true; no broadPrimvars.
    def test_connectivityChangeSameVertexCountEmitsTopology(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.ConnectivityChangeSameVertexCountEmitsTopology")

    # What: UV edits in render items mode arrive via MRenderItem geomChanged, not uvPivot callbacks.
    # How: build a cube scene (render items mode), select UVs and polyEditUV via C++, inspect locators.
    # Expect: uvs + points; no broadPrimvars, no extComputationPrimvars.
    def test_uvEditViaGeomChangedEmitsPointsAndUVsNotTopology(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.UVEditViaGeomChangedEmitsPointsAndUVsNotTopology")

    # What: smooth mesh preview toggles subdivision via render item topoChanged, not mesh-adapter
    #       attribute handlers.  Topology helper emits mesh topology only; not displayStyle or
    #       subdivisionTags (those are mesh-adapter locators).
    # How: build a cube scene (render items mode), set displaySmoothMesh=2 via C++, inspect locators.
    # Expect: meshTopology; no broadPrimvars, no displayStyle, no subdivisionTags.
    def test_smoothMeshToggleEmitsTopologyNotDisplayStyle(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemDirtyLocators.SmoothMeshToggleEmitsTopologyNotDisplayStyle")

    # What: a dynamic attribute change on a camera (sprim) must NOT emit extComputationPrimvars.
    #       The ExtCompGate test is mode-agnostic: the camera adapter gates on IsRprim()
    #       regardless of whether mesh adapter or render items mode is active.
    # How: create a dedicated scene camera, then addAttr+setAttr a dynamic float via C++ and
    #      inspect dirty locators.
    # Expect: extCompPrimvars=false.
    def test_dynamicAttrOnCameraSkipsExtComputationPrimvars(self):
        self.setupScene()
        _, camera_shape = cmds.camera()
        camera_shape = cmds.ls(camera_shape, long=True)[0]
        cmds.optionVar(stringValue=("mhCameraShape", camera_shape))
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="ExtCompGate.DynamicAttrOnCameraSkipsExtComputationPrimvars")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
