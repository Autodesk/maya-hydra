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
import os

import maya.cmds as cmds

import fixturesUtils
import mtohUtils


class TestMeshAdapterVisibility(mtohUtils.MayaHydraBaseTestCase):
    '''Validate that the mesh-adapter InsertDag path skips invisible shapes.

    In batch / mesh-adapter mode the scene index is populated by iterating the whole DAG
    (MItDag), which visits LOD meshes and corrective blend-shape targets that VP2 never makes
    render items for. InsertDag must skip shapes that are not visible so only renderable
    geometry reaches Hydra, matching viewport behaviour.
    '''

    _file = __file__

    def matchingRprims(self, rprims, matching):
        return len([rprim for rprim in rprims if matching in rprim])

    # Force a full repopulate so InsertDag re-runs with the current visibility state.
    def repopulate(self):
        self.setViewport2Renderer()
        self.setHdStormRenderer()
        cmds.refresh()

    # What: an invisible mesh must not be inserted into the Hydra render index.
    # How: create a visible and a hidden cube, repopulate, then inspect the render index.
    # Expect: the visible cube's rprim is present; the hidden cube's rprim is absent.
    def test_invisibleMeshIsSkipped(self):
        if not os.getenv('MAYA_HYDRA_USE_MESH_ADAPTER'):
            self.skipTest("Visibility filtering on InsertDag only applies to the mesh adapter path.")

        self.setHdStormRenderer()

        cmds.polyCube(name="visibleCube")
        cmds.polyCube(name="hiddenCube")
        cmds.setAttr("hiddenCube.visibility", 0)

        # Repopulate so the hidden cube is evaluated by InsertDag while already invisible.
        self.repopulate()

        rprims = self.getIndex()
        self.assertGreaterEqual(
            self.matchingRprims(rprims, "visibleCubeShape"), 1,
            "Visible mesh should be present in the render index")
        self.assertEqual(
            self.matchingRprims(rprims, "hiddenCubeShape"), 0,
            "Hidden mesh must be skipped by the mesh-adapter InsertDag path")

    # What: a mesh hidden via an invisible ancestor transform must also be skipped.
    # How: parent a cube under a hidden group, repopulate, then inspect the render index.
    # Expect: the cube's rprim is absent (dag.isVisible() accounts for ancestor visibility).
    def test_meshUnderHiddenGroupIsSkipped(self):
        if not os.getenv('MAYA_HYDRA_USE_MESH_ADAPTER'):
            self.skipTest("Visibility filtering on InsertDag only applies to the mesh adapter path.")

        self.setHdStormRenderer()

        cmds.polyCube(name="childCube")
        group = cmds.group("childCube", name="hiddenGroup")
        cmds.setAttr(group + ".visibility", 0)

        self.repopulate()

        rprims = self.getIndex()
        self.assertEqual(
            self.matchingRprims(rprims, "childCubeShape"), 0,
            "Mesh under a hidden group must be skipped by the mesh-adapter InsertDag path")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
