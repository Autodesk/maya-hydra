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
# LookdevX MaterialX edits made while the viewport is in Hydra, to materials
# Viewport 2.0 never drew, must show in the Hydra viewport right away.
#
import os

import maya.cmds as cmds
import ufe

import fixturesUtils
import imageUtils
import mtohUtils


class TestMaterialXLiveUpdates(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['LookdevXMaya']

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1

    BLUE = (0.0, 0.0, 1.0)

    def _setBaseColor(self, shaderItem, color):
        # The command the Attribute Editor runs. Executed directly: the ufeCmd
        # Maya command that ufeCmdWrapper needs isn't registered in test sessions.
        attr = ufe.Attributes.attributes(shaderItem).attribute("inputs:base_color")
        self.assertIsNotNone(attr, "No inputs:base_color on " + str(shaderItem.path()))
        value = ufe.Color3f(*color)
        cmd = attr.setCmd(value)
        if cmd is not None:
            cmd.execute()
        else:
            attr.set(value)
        got = attr.get()
        self.assertEqual(tuple(got.color) if hasattr(got, "color") else (got.r(), got.g(), got.b()),
                         color)

    def _snapshot(self, name):
        path = os.path.join(self.getSnapshotDir(), name)
        imageUtils.snapshot(path)
        return path

    def _assertLiveMatchesRebuilt(self, name):
        '''The live Hydra image must match the one Hydra draws after a full rebuild.

        The rebuild (VP2, then back to the test renderer) reads the current
        MaterialX document, so it shows the latest edit whether or not the live
        viewport was told about it.
        '''
        livePath = self._snapshot(name + "_live.png")
        self.setViewport2Renderer()
        self.setTestRenderer()
        rebuiltPath = self._snapshot(name + "_rebuilt.png")
        self.assertImagesClose(rebuiltPath, livePath,
                               self.IMAGEDIFF_FAIL_THRESHOLD,
                               self.IMAGEDIFF_FAIL_PERCENT)

    def _assignNewOpenPbrSurface(self, mayaPath):
        '''Right-click > Assign New Material > LookdevX > BXDF > OpenPBR Surface.

        Returns the new MaterialXSurfaceShader node.
        '''
        materialsBefore = set(cmds.ls(type="MaterialXSurfaceShader"))
        item = ufe.Hierarchy.createItem(ufe.PathString.path(mayaPath))
        contextOps = ufe.ContextOps.contextOps(item)
        # As LookdevXViewportMenuBuilder.doOp does: LookdevX runs the assignment
        # (materialxAssign) inside doOpCmd and returns no command.
        cmd = contextOps.doOpCmd(
            ["AssignNewMaterial", "BXDF", "ND_open_pbr_surface_surfaceshader"])
        if cmd is not None:
            cmd.execute()
        newMaterials = sorted(set(cmds.ls(type="MaterialXSurfaceShader")) - materialsBefore)
        self.assertEqual(len(newMaterials), 1,
                         "New MaterialXSurfaceShader nodes: %s" % newMaterials)
        return newMaterials[0]

    def _surfaceShaderOf(self, materialNode):
        '''The UFE item of the shader feeding a MaterialXSurfaceShader's surface material.'''
        materialPath = ufe.PathString.path(cmds.getAttr(materialNode + ".up"))
        document = ufe.Hierarchy.createItem(materialPath.pop())
        shaders = [child for child in ufe.Hierarchy.hierarchy(document).children()
                   if "open_pbr_surface" in child.nodeType()]
        self.assertEqual(len(shaders), 1,
                         "Expected one OpenPBR shader in %s, got %s" % (
                             document.path(),
                             [(str(c.path()), c.nodeType())
                              for c in ufe.Hierarchy.hierarchy(document).children()]))
        return shaders[0]

    # What: an input edit on a MaterialX material loaded while in Hydra (HYDRA-2739).
    # How: save a cube with a new OpenPBR Surface, reopen it in Hydra, set base_color
    #      to blue through UFE. The reopened material was never drawn by VP2.
    #      (RedMtlxSphere.ma can't be used: LookdevX locks the inputs of its
    #      MaterialX 1.38 document.)
    # Expect: the live image matches the image after a full rebuild (blue cube).
    def test_EditInput(self):
        cmds.polyCube(name="pCube1")
        self._assignNewOpenPbrSurface("|pCube1")
        scenePath = os.path.join(self.getSnapshotDir(), "openPbrCube.ma")
        cmds.file(rename=scenePath)
        cmds.file(save=True, type="mayaAscii", force=True)

        cmds.file(scenePath, open=True, force=True)
        self.applySelectionHighlightMode()
        self.setTestRenderer()
        cmds.select(clear=True)
        self.setBasicCam(3)
        # Hydra draws the reopened material before the edit.
        cmds.refresh()

        material = cmds.ls(type="MaterialXSurfaceShader")
        self.assertEqual(len(material), 1, "MaterialXSurfaceShader nodes: %s" % material)
        self._setBaseColor(self._surfaceShaderOf(material[0]), self.BLUE)
        self._assertLiveMatchesRebuilt("editInput")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
