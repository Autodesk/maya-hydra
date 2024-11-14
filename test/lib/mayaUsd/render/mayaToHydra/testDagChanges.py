# Copyright 2020 Luma Pictures
# Copyright 2023 Autodesk
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
import maya.mel

import fixturesUtils
import mayaUtils
import mtohUtils

from string import digits

def fullPath(nodeName):
    return cmds.ls(nodeName, l=True)[0]

class DagChangesBaseTestCase(mtohUtils.MayaHydraBaseTestCase):

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2

    def rprimPath(self, mayaPath):
        return '/MayaHydraViewportRenderer/rprims/Lighted' + mayaPath.replace('|', '/')

class TestDagChanges(DagChangesBaseTestCase):
    _file = __file__

    def setUp(self):
        self.makeCubeScene()

        self.grp1 = cmds.createNode('transform', name='group1')
        self.grp2 = cmds.createNode('transform', name='group2')

        self.imageVersion = maya.mel.eval("defaultShaderName").rstrip(digits)

    def test_reparent_transform(self):
        cmds.parent(self.cubeTrans, self.grp1)
        grp1ShapeRprim = self.rprimPath(fullPath(self.cubeShape))
        self.assertEqual(
            grp1ShapeRprim,
            self.rprimPath("|{self.grp1}|{self.cubeTrans}|{self.cubeShape}"
                           .format(self=self)))
        cmds.refresh()
        self.assertNodeNameInIndex(grp1ShapeRprim)
        self.assertNodeNameNotInIndex(self.cubeRprim)

        cmds.parent(self.grp1, self.grp2)
        grp2ShapeRprim = self.rprimPath(fullPath(self.cubeShape))
        self.assertEqual(
            grp2ShapeRprim,
            self.rprimPath("|{self.grp2}|{self.grp1}|{self.cubeTrans}|{self.cubeShape}"
                           .format(self=self)))
        cmds.refresh()
        self.assertNodeNameInIndex(grp2ShapeRprim)
        self.assertNodeNameNotInIndex(grp1ShapeRprim)
        self.assertNodeNameNotInIndex(self.cubeRprim)

        cmds.parent(self.cubeTrans, world=True)
        origShapePrim = self.rprimPath(fullPath(self.cubeShape))
        # A Maya Dag node that is reparented back to its original path will
        # have a StandardShadedItem with a different numerical suffix, but its
        # path will match.
        self.assertIn(origShapePrim.rstrip(digits), self.cubeRprim)
        cmds.refresh()
        self.assertNodeNameInIndex(self.cubeRprim.rstrip(digits))
        self.assertNodeNameNotInIndex(grp2ShapeRprim)
        self.assertNodeNameNotInIndex(grp1ShapeRprim)

    def test_reparent_shape(self):
        cmds.parent(self.cubeShape, self.grp1, shape=1, r=1)
        grp1ShapeRprim = self.rprimPath(fullPath(self.cubeShape))
        self.assertEqual(
            grp1ShapeRprim,
            self.rprimPath("|{self.grp1}|{self.cubeShape}"
                           .format(self=self)))
        cmds.refresh()
        self.assertNodeNameInIndex(grp1ShapeRprim)
        self.assertNodeNameNotInIndex(self.cubeRprim)

        cmds.parent(self.cubeShape, self.cubeTrans, shape=1, r=1)
        origShapePrim = self.rprimPath(fullPath(self.cubeShape))
        self.assertIn(origShapePrim.rstrip(digits), self.cubeRprim)
        cmds.refresh()
        self.assertNodeNameInIndex(self.cubeRprim.rstrip(digits))
        self.assertNodeNameNotInIndex(grp1ShapeRprim)

    def test_new_shape(self):
        otherCube = cmds.polyCube()[0]
        otherCubeShape = cmds.listRelatives(otherCube, fullPath=1)[0]
        otherRprim = self.rprimPath(fullPath(otherCubeShape))
        cmds.refresh()
        self.assertNodeNameInIndex(self.cubeRprim)
        self.assertNodeNameInIndex(otherRprim)

    def test_instances(self):
        self.setBasicCam(dist=10)
        undoWasEnabled = cmds.undoInfo(q=1, state=1)

        cmds.undoInfo(state=0)
        try:
            pCube2 = cmds.createNode('transform', name='pCube2')

            cmds.setAttr('{}.tz'.format(self.grp1), 5)
            cmds.setAttr('{}.tx'.format(self.grp2), 8)
            cmds.setAttr('{}.ty'.format(pCube2), 5)

            # No instances to start
            #   (1) |pCube1|pCubeShape1
            self.assertSnapshotClose(
                "instances_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Add |group1|pCube1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            cmds.parent(self.cubeTrans, self.grp1, add=1, r=1)
            cmds.select(clear=1)
            self.assertSnapshotClose(
                "instances_12.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Add |pCube2|pCubeShape1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            cmds.parent(self.cubeShape, pCube2, add=1, r=1, shape=1)
            cmds.select(clear=1)
            self.assertSnapshotClose(
                "instances_123.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Add |group2|group1|pCube1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (4) |group2||group1|pCube1|pCubeShape1
            cmds.parent(self.grp1, self.grp2, add=1, r=1)
            cmds.select(clear=1)
            self.assertSnapshotClose(
                "instances_1234.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Add |group1|pCube2 instance
            #   (1) |pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (4) |group2||group1|pCube1|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            #   (6) |group2||group1|pCube2|pCubeShape1
            cmds.parent(pCube2, self.grp1, add=1, r=1)
            cmds.select(clear=1)
            self.assertSnapshotClose(
                "instances_123456.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Delete group2
            #   [no shapes]
            cmds.undoInfo(state=1)
            cmds.undoInfo(openChunk=1)
            try:
                cmds.delete(self.grp2)
                self.assertNotIn(self.cubeRprim, self.getIndex())
                self.assertSnapshotClose(
                    "instances_0.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(closeChunk=1)

            # Undo group2 deletion
            #   (1) |pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (4) |group2||group1|pCube1|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            #   (6) |group2||group1|pCube2|pCubeShape1
            cmds.undo()
            self.assertSnapshotClose(
                "instances_123456.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Remove |group2|group1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            cmds.parent('|{self.grp2}|{self.grp1}'.format(self=self),
                        removeObject=1)
            self.assertSnapshotClose(
                "instances_1235.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Remove pCube2|pCubeShape1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            cmds.undoInfo(openChunk=1)
            try:
                cmds.parent('|{pCube2}|{self.cubeShape}'.format(self=self,
                                                                    pCube2=pCube2),
                            removeObject=1, shape=1)
                self.assertSnapshotClose(
                    "instances_12.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(closeChunk=1)

            # Undo remove pCube2|pCubeShape1 instance
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            cmds.undo()
            self.assertSnapshotClose(
                "instances_1235.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Remove pCube1|pCubeShape1 (the "master" instance)
            #   (3) |pCube2|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            cmds.undoInfo(openChunk=1)
            try:
                cmds.parent('|{self.cubeTrans}|{self.cubeShape}'.format(self=self),
                            removeObject=1, shape=1)
                self.assertSnapshotClose(
                    "instances_35.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(closeChunk=1)

            # Undo remove pCube1|pCubeShape1 (the "master" instance)
            #   (1) |pCube1|pCubeShape1
            #   (2) |group1|pCube1|pCubeShape1
            #   (3) |pCube2|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            cmds.undo()

            # the playblast command is entered into the undo queue, so we
            # need to disable without flusing the queue, so we can test redo
            cmds.undoInfo(stateWithoutFlush=0)
            try:
                self.assertSnapshotClose(
                    "instances_1235.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(stateWithoutFlush=1)

            # Redo remove pCube2|pCubeShape1 instance
            #   (3) |pCube2|pCubeShape1
            #   (5) |group1|pCube2|pCubeShape1
            cmds.redo()
            self.assertSnapshotClose(
                "instances_35.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

            # Remove |group1|pCube2 instance
            #   (3) |pCube2|pCubeShape1
            cmds.parent('{self.grp1}|{pCube2}'.format(self=self,
                                                      pCube2=pCube2),
                        removeObject=1)
            self.assertSnapshotClose(
                "instances_3.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
        finally:
            cmds.undoInfo(state=undoWasEnabled)

    def test_move(self):
        self.setBasicCam(dist=10)
        self.assertSnapshotClose(
            "instances_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
        cmds.setAttr('{}.ty'.format(self.cubeTrans), 5)
        self.assertSnapshotClose(
            "instances_3.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

    def test_instance_move(self):
        self.setBasicCam(dist=10)
        # Add |group1|pCube1 instance
        #   (1) |pCube1|pCubeShape1
        #   (2) |group1|pCube1|pCubeShape1
        cmds.parent(self.cubeTrans, self.grp1, add=1, r=1)
        cmds.select(clear=1)

        # because we haven't moved anything, it should initially look like only
        # one cube...
        self.assertSnapshotClose(
            "instances_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)

        cmds.setAttr('{}.tz'.format(self.grp1), 5)
        # Now that we moved one, it should look like 2 cubes
        self.assertSnapshotClose(
            "instances_12.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)


class TestUndo(DagChangesBaseTestCase):
    _file = __file__

    def test_node_creation_undo(self):
        undoWasEnabled = cmds.undoInfo(q=1, state=1)

        self.imageVersion = maya.mel.eval("defaultShaderName").rstrip(digits)

        cmds.undoInfo(state=0)
        try:
            mayaUtils.openNewScene()
            self.setBasicCam(dist=10)

            self.setHdStormRenderer()

            if maya.mel.eval("defaultShaderName") == "standardSurface1":
                color = (0.8, 0.8, 0.8)
                cmds.setAttr("standardSurface1.baseColor", type='float3', *color)
                cmds.setAttr("standardSurface1.specularRoughness", 0.4)

            cmds.undoInfo(state=1)
            cmds.undoInfo(openChunk=1)
            try:
                cubeTrans = cmds.polyCube()
                cubeShape = cmds.listRelatives(cubeTrans)[0]
                cubeRprim = self.rprimPath(fullPath(cubeShape))
                cmds.select(clear=1)
                cmds.refresh()
                self.assertIn(cubeRprim, self.getIndex()[0])
                self.assertSnapshotClose(
                    "instances_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(closeChunk=1)

            cmds.undo()

            # the playblast command is entered into the undo queue, so we
            # need to disable without flusing the queue, so we can test redo
            cmds.undoInfo(stateWithoutFlush=0)
            try:
                cmds.refresh()
                self.assertEqual([], self.getIndex())
                self.assertSnapshotClose(
                    "instances_0.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(stateWithoutFlush=1)

            cmds.redo()

            cmds.undoInfo(stateWithoutFlush=0)
            try:
                cmds.refresh()
                self.assertIn(cubeRprim, self.getIndex()[0])
                self.assertSnapshotClose(
                    "instances_1.png", self.IMAGE_DIFF_FAIL_THRESHOLD,
                    self.IMAGE_DIFF_FAIL_PERCENT, self.imageVersion)
            finally:
                cmds.undoInfo(stateWithoutFlush=1)

        finally:
            cmds.undoInfo(state=undoWasEnabled)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
