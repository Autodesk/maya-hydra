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
import unittest

import maya.cmds as cmds
import maya.mel as mel

import fixturesUtils
import mtohUtils

class TestCommand(unittest.TestCase):
    _file = __file__
    _has_embree = None

    @classmethod
    def setUpClass(cls):
        loaded = cmds.loadPlugin('mayaHydra', quiet=True)
        if loaded != ['mayaHydra']:
            raise RuntimeError('mayaHydra plugin load failed.')

    @classmethod
    def has_embree(cls):
        import pxr.Plug
        if cls._has_embree is None:
            plug_reg = pxr.Plug.Registry()
            cls._has_embree = bool(plug_reg.GetPluginWithName('hdEmbree'))
        return cls._has_embree

    def test_invalidFlag(self):
        self.assertRaises(TypeError, cmds.mayaHydra, nonExistantFlag=1)

    def test_listRenderers(self):
        renderers = cmds.mayaHydra(listRenderers=1)
        self.assertEqual(renderers, cmds.mayaHydra(lr=1))
        self.assertIn(mtohUtils.HD_STORM, renderers)
        if self.has_embree():
            self.assertIn("HdEmbreeRendererPlugin", renderers)

    def test_listActiveRenderers(self):
        activeRenderers = cmds.mayaHydra(listActiveRenderers=1)
        self.assertEqual(activeRenderers, cmds.mayaHydra(lar=1))
        self.assertEqual(activeRenderers, [])

        activeEditor = cmds.playblast(ae=1)
        cmds.modelEditor(
            activeEditor, e=1,
            rendererOverrideName=mtohUtils.HD_STORM_OVERRIDE)
        cmds.refresh(f=1)

        activeRenderers = cmds.mayaHydra(listActiveRenderers=1)
        self.assertEqual(activeRenderers, cmds.mayaHydra(lar=1))
        self.assertEqual(activeRenderers, [mtohUtils.HD_STORM])

        if self.has_embree():
            cmds.modelEditor(
                activeEditor, e=1,
                rendererOverrideName="mtohRenderOverride_HdEmbreeRendererPlugin")
            cmds.refresh(f=1)

            activeRenderers = cmds.mayaHydra(listActiveRenderers=1)
            self.assertEqual(activeRenderers, cmds.mayaHydra(lar=1))
            self.assertEqual(activeRenderers, ["HdEmbreeRendererPlugin"])

        cmds.modelEditor(activeEditor, rendererOverrideName="", e=1)
        cmds.refresh(f=1)

        activeRenderers = cmds.mayaHydra(listActiveRenderers=1)
        self.assertEqual(activeRenderers, cmds.mayaHydra(lar=1))
        self.assertEqual(activeRenderers, [])

    def test_getRendererDisplayName(self):
        # needs at least one arg
        self.assertRaises(RuntimeError, mel.eval,
                          "mayaHydra -getRendererDisplayName")

        displayName = cmds.mayaHydra(renderer=mtohUtils.HD_STORM,
                                getRendererDisplayName=True)
        self.assertEqual(displayName, cmds.mayaHydra(r=mtohUtils.HD_STORM, gn=True))
        self.assertEqual(displayName, "GL")

        if self.has_embree():
            displayName = cmds.mayaHydra(renderer="HdEmbreeRendererPlugin",
                                    getRendererDisplayName=True)
            self.assertEqual(displayName, cmds.mayaHydra(r="HdEmbreeRendererPlugin",
                                                    gn=True))
            self.assertEqual(displayName, "Embree")

    def test_createRenderGlobals(self):
        for flag in ("createRenderGlobals", "crg"):
            cmds.file(f=1, new=1)
            self.assertFalse(cmds.objExists(
                "defaultRenderGlobals.mtohMotionSampleStart"))
            cmds.mayaHydra(**{flag: 1})
            self.assertTrue(cmds.objExists(
                "defaultRenderGlobals.mtohMotionSampleStart"))
            self.assertFalse(cmds.getAttr(
                "defaultRenderGlobals.mtohMotionSampleStart"))            

    def test_versionInfo(self):
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(majorVersion=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(mjv=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(minorVersion=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(mnv=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(patchVersion=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(pv=True), 0)

    def test_buildInfo(self):
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(buildNumber=True), 0)
        self.assertGreaterEqual(cmds.mayaHydraBuildInfo(bn=True), 0)
        self.assertNotEqual(cmds.mayaHydraBuildInfo(gitCommit=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(gc=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(gitBranch=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(gb=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(buildDate=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(bd=True), '')
        self.assertNotEqual(cmds.mayaHydraBuildInfo(cutIdentifier=True), "DEVBLD")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
