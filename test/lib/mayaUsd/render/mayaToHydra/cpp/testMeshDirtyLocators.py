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
# Python wrapper for the MeshDirtyLocators suite in testDirtyLocators.cpp. Builds a cube
# scene with MAYA_HYDRA_USE_MESH_ADAPTER enabled and runs mesh-adapter dirty-locator cases
# through mayaHydraCppTest.
#
import platform
import unittest

import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded


class TestMeshDirtyLocators(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def setupScene(self):
        cmds.file(new=True, force=True)
        cmds.polyCube(name="testCube")
        mesh_shape = cmds.ls("testCubeShape", long=True)[0]
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhMeshShape", mesh_shape))
        cmds.optionVar(stringValue=("mhMeshTransform", cmds.ls("testCube", long=True)[0]))
        cmds.refresh()

    # What: vertex deformation should dirty primvars/points without topology locators.
    # How: build a cube scene, move one vertex via C++, inspect dirty locators.
    # Expect: points dirty; no mesh topology or broad primvars.
    def test_deformationVertexMoveEmitsPointsNotTopology(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.DeformationVertexMoveEmitsPointsNotTopology")

    # What: topology edits should dirty mesh topology and broad primvars.
    # How: build a cube scene, extrude a face via C++, inspect dirty locators.
    # Expect: topology + broad primvars + points; no extComputationPrimvars.
    @unittest.skipIf(
        platform.system() == "Darwin",
        "HYDRA-2407: polyExtrudeFacet SIGSEGV on OSX Maya 2026+ with Hydra viewport active")
    def test_topologyExtrudeEmitsTopologyAndBroadPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.TopologyExtrudeEmitsTopologyAndBroadPrimvars")

    # What: a dynamic attribute change on a mesh (rprim) must emit extComputationPrimvars.
    #       Exercises the open branch of the _maybeDirtyExtComputationPrimvars rprim gate.
    # How: addAttr+setAttr a dynamic float on the cube shape via C++, inspect dirty locators.
    # Expect: extCompPrimvars=true.
    def test_dynamicAttrOnMeshEmitsExtComputationPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.DynamicAttrOnMeshEmitsExtComputationPrimvars")

    # What: a dynamic attribute change on a camera (sprim) must NOT emit extComputationPrimvars.
    #       Exercises the closed branch of the _maybeDirtyExtComputationPrimvars rprim gate.
    # How: use the default persp camera, then addAttr+setAttr a dynamic float via C++ and
    #      inspect dirty locators.
    # Expect: extCompPrimvars=false.
    def test_dynamicAttrOnCameraSkipsExtComputationPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="ExtCompGate.DynamicAttrOnCameraSkipsExtComputationPrimvars")

    # What: editing UVs via uvPivot should dirty only primvars/st (no topology, no broad primvars).
    # How: build a cube scene, set uvPivot via C++, inspect dirty locators.
    # Expect: uvs; no meshTopology, no broadPrimvars, no points.
    def test_uvEditEmitsGranularUVsOnly(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.UVEditEmitsGranularUVsOnly")

    # What: editing UVs via polyEditUV should dirty only primvars/st (UVSetChangedCallback path).
    # How: build a cube scene, select UVs and polyEditUV via C++, inspect dirty locators.
    # Expect: uvs; no meshTopology, no broadPrimvars, no points.
    def test_uvEditViaPolyEditUVEmitsGranularUVsOnly(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.UVEditViaPolyEditUVEmitsGranularUVsOnly")

    # What: toggling smooth mesh preview should dirty displayStyle + topology + subdivisionTags,
    #       but NOT the broad primvars locator.
    # How: build a cube scene, set displaySmoothMesh=2 via C++, inspect dirty locators.
    # Expect: displayStyle, meshTopology, subdivisionTags; no broadPrimvars.
    def test_smoothMeshToggleEmitsDisplayStyleAndTopology(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.SmoothMeshToggleEmitsDisplayStyleAndTopology")

    # What: intermediateObject toggle should dirty visibility and refresh the Hydra schema.
    # How: mesh adapter mode, toggle shape intermediateObject via C++, inspect locators + schema.
    # Expect: visibility locator on each toggle; visibility schema matches intermediateObject.
    def test_intermediateObjectToggleEmitsVisibilityAndUpdatesSchema(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators.IntermediateObjectToggleEmitsVisibilityAndUpdatesSchema")

    # What: instanced shapes route visibility plug dirties through _InstancerNodeDirty.
    # How: duplicate -rr -ilf, toggle master transform visibility via C++, inspect locators + schema.
    # Expect: instancer + visibility locators on each toggle; schema matches master visibility.
    def test_instancedTransformVisibilityToggleEmitsVisibilityAndUpdatesSchema(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators."
                f"InstancedTransformVisibilityToggleEmitsVisibilityAndUpdatesSchema")

    # What: duplicate-instance visibility dirty instancer locators, not prototype visibility.
    # How: duplicate -rr -ilf, hide/show duplicate transform via C++, inspect locators + schema.
    # Expect: instancer locators only; prototype visibility schema stays visible.
    def test_instancedNonMasterVisibilityToggleEmitsInstancerNotPrototypeVisibility(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MeshDirtyLocators."
                f"InstancedNonMasterVisibilityToggleEmitsInstancerNotPrototypeVisibility")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
