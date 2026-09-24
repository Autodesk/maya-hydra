#
# Copyright 2026 Autodesk, Inc. All rights reserved.
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
import shutil

import maya.cmds as cmds
import fixturesUtils
import mtohUtils

# Reuse the existing renderCurrentFrame scene (basic/renderCurrentFrame.ma):
# it already has valid USD render settings/products and requires -cf, so it
# only needs the -renderer flag to be omitted (rather than a brand new scene)
# to exercise selection of the renderer from the USD currentRenderer
# attribute on the UsdDefaultRenderDescription node.
#The basic/renderCurrentFrame.ma scene has arnold (not hydra arnold) in defaultRenderGlobals.ren (current renderer)

_SCENES_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "scenes", "basic")

_SCENE_FILE_NAME = "renderCurrentFrame.ma"

_USD_DEFAULT_RENDER_DESCRIPTION_NODE = "UsdDefaultRenderDescription"

# renderCurrentFrame.ma's mayaUsdProxyShape node and the RenderProduct prim on
# its stage (see renderCurrentFrame.usda), used by _useAbsoluteRenderProductPath()
# below.
_PROXY_SHAPE_NODE = "renderSettingsShape"
_RENDER_PRODUCT_PATH = "/Render/BeautyProduct"

# renderCurrentFrame.usda's single RenderProduct ("BeautyProduct") authors
# productName "../images/renderCurrentFrame.png", resolved relative to the
# USD stage root layer (the .usda file, which sits next to the .ma scene
# file). That relative-path resolution (ResolveRenderProductImagePath() in
# hydraRenderCmdHydraV1RenderSettings.cpp) only runs for the Hydra V1 render
# settings strategy.
# Hydra V2 (used here for HdArnoldRendererPlugin) uses absolute filename.
_RENDERED_IMAGE_NAME = "renderCurrentFrame.png"


def _scenePath(sceneFileName):
    return os.path.join(_SCENES_DIR, sceneFileName)


class TestCurrentRendererSelection(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _initializeStandalone = True
    _setHdStormRenderer = False
    _requiredPlugins = ['mtoa']

    # Baselines live alongside the scene under scenes/basic/, matching the
    # convention used elsewhere in this directory (e.g. renderCurrentFrame.png
    # itself, used by the mayabatch CTest variant of this scene).
    _USD_CURRENT_RENDERER_BASELINE = "renderCurrentFrame_UsdCurrentRendererHonored.png"
    _USD_CURRENT_RENDERER_STORM_BASELINE = "renderCurrentFrame_UsdCurrentRendererStorm.png"
    _LEGACY_ARNOLD_BASELINE = "renderCurrentFrame_LegacyArnold.png"

    # Thresholds for image comparisons: tolerate only small, rare differences (renderer dithering/AA jitter)
    _IMAGE_DIFF_FAIL = 0.01
    _IMAGE_DIFF_FAILPERCENT = 1.0

    def _renderedImagePath(self, scenePath):
        sceneDir = os.path.dirname(scenePath)
        imagesDir = os.path.normpath(os.path.join(sceneDir, "..", "images"))
        return os.path.join(imagesDir, _RENDERED_IMAGE_NAME)

    def _assertRenderedImageMatchesBaseline(self, baselineName, renderedImagePath):
        self.assertImagesClose(
            _scenePath(baselineName),
            renderedImagePath,
            fail=self._IMAGE_DIFF_FAIL,
            failpercent=self._IMAGE_DIFF_FAILPERCENT)

    def _openScene(self):
        # Copy the scene into the build directory test folder, matching the
        # pattern used by the other cmdLineRender mayabatch Python tests.
        tempAppDir = os.environ.get("MAYA_APP_DIR")
        if tempAppDir:
            destDir = os.path.join(tempAppDir, "projects", "default", "scenes")
            if not os.path.isdir(destDir):
                os.makedirs(destDir)
            baseName = os.path.splitext(_SCENE_FILE_NAME)[0]
            for ext in (".ma", ".usda"):
                shutil.copy(_scenePath(baseName + ext), os.path.join(destDir, baseName + ext))
            scenePath = os.path.join(destDir, _SCENE_FILE_NAME)
        else:
            scenePath = _scenePath(_SCENE_FILE_NAME)

        cmds.file(scenePath, open=True, force=True)
        self._useAbsoluteRenderProductPath(scenePath)
        return scenePath

    def _useAbsoluteRenderProductPath(self, scenePath):
        # See the module-level comment above _RENDERED_IMAGE_NAME: required
        # for HdArnoldRendererPlugin under Hydra V2 render settings, and a
        # harmless no-op for Hydra V1 (Storm), which already passes absolute
        # productName values through unchanged.
        #
        import mayaUsd.lib as mayaUsdLib
        stage = mayaUsdLib.GetPrim(_PROXY_SHAPE_NODE).GetStage()
        self.assertIsNotNone(stage)
        renderProduct = stage.GetPrimAtPath(_RENDER_PRODUCT_PATH)
        self.assertTrue(renderProduct.IsValid())
        renderProduct.GetAttribute("productName").Set(
            self._renderedImagePath(scenePath).replace(os.sep, "/"))

    def _setCurrentRenderer(self, rendererName):
        cmds.setAttr(
            "%s.currentRenderer" % _USD_DEFAULT_RENDER_DESCRIPTION_NODE,
            rendererName,
            type="string")

    # No -renderer flag: the currentRenderer attribute on
    # UsdDefaultRenderDescription must be read and used to select the
    # renderer.  Success here is only possible if that attribute (rather
    # than a hard-coded default) supplied the renderer name.
    #
    # Runs HdArnoldRendererPlugin under Hydra V2 render settings
    def test_UsdCurrentRendererAttributeIsHonored(self):
        scenePath = self._openScene()
        self._setCurrentRenderer("HdArnoldRendererPlugin")

        # Must not raise: GetRenderer() should select "HdArnoldRendererPlugin"
        # from the USD attribute since no -renderer/-r flag is passed.
        cmds.hydraRender(currentFrame=True)

        # Verify the actual rendered pixels, not just that rendering
        # succeeded without raising: this proves HdArnoldRendererPlugin (as
        # opposed to some other default) really did the rendering.
        self._assertRenderedImageMatchesBaseline(
            self._USD_CURRENT_RENDERER_BASELINE,
            self._renderedImagePath(scenePath))

    # Same as test_UsdCurrentRendererAttributeIsHonored, but setting the
    # currentRenderer via the public UsdDefaultRenderDescription.setCurrentRenderer()
    # API instead of setAttr directly on the node, to prove that API also
    # ends up authoring the attribute GetRenderer() reads.
    def test_UsdCurrentRendererSetViaApiIsHonored(self):
        from mayaUsd.lib import UsdDefaultRenderDescription

        scenePath = self._openScene()
        UsdDefaultRenderDescription.setCurrentRenderer("HdArnoldRendererPlugin")

        # Must not raise: GetRenderer() should select "HdArnoldRendererPlugin"
        # from the USD attribute since no -renderer/-r flag is passed.
        cmds.hydraRender(currentFrame=True)

        # Same expected output as test_UsdCurrentRendererAttributeIsHonored:
        # setCurrentRenderer() authors the same attribute setAttr does, so
        # the rendered pixels should match the same baseline.
        self._assertRenderedImageMatchesBaseline(
            self._USD_CURRENT_RENDERER_BASELINE,
            self._renderedImagePath(scenePath))

    # Same as test_UsdCurrentRendererAttributeIsHonored, but with the USD
    # currentRenderer attribute set to Storm rather than Arnold, to prove
    # GetRenderer() reads the attribute's value rather than being hard-coded
    # to a single renderer name.
    def test_UsdCurrentRendererStormIsHonored(self):
        scenePath = self._openScene()
        self._setCurrentRenderer("HdStormRendererPlugin")

        cmds.hydraRender(currentFrame=True)

        self._assertRenderedImageMatchesBaseline(
            self._USD_CURRENT_RENDERER_STORM_BASELINE,
            self._renderedImagePath(scenePath))

    # When both the -renderer/-r flag and the USD currentRenderer attribute
    # are set to different renderers, the flag must win.
    #
    # Runs HdArnoldRendererPlugin under Hydra V2 render settings, same as
    # test_UsdCurrentRendererAttributeIsHonored above.
    def test_ExplicitRendererFlagOverridesUsdCurrentRenderer(self):
        scenePath = self._openScene()
        self._setCurrentRenderer("HdStormRendererPlugin")

        cmds.hydraRender(renderer="HdArnoldRendererPlugin", currentFrame=True)

        # Same expected output as the Arnold-via-USD-attribute case: since
        # the flag forces HdArnoldRendererPlugin here too, the rendered
        # pixels should match the same baseline.
        self._assertRenderedImageMatchesBaseline(
            self._USD_CURRENT_RENDERER_BASELINE,
            self._renderedImagePath(scenePath))

    # An invalid currentRenderer value must fail that one render call with a
    # clean error (no Maya crash/exit), and must not prevent a subsequent,
    # valid hydraRender call from succeeding in the same Maya session.
    def test_InvalidCurrentRendererDoesNotCrashSession(self):
        self._openScene()
        self._setCurrentRenderer("NotARealRendererPlugin")

        with self.assertRaises(RuntimeError):
            cmds.hydraRender(currentFrame=True)

        # The failed render above must not have disturbed the Maya session:
        # a subsequent render with a valid, explicit renderer must succeed.
        # (Also Hydra V2 for HdArnoldRendererPlugin, like the tests above.)
        cmds.hydraRender(renderer="HdArnoldRendererPlugin", currentFrame=True)

    # No -renderer flag and an empty/unauthored currentRenderer must fail before
    # any render delegate is created (no silent Storm fallback).
    def test_MissingCurrentRendererFailsWithoutRendererFlag(self):
        self._openScene()
        self._setCurrentRenderer("")

        # with self.assertRaisesRegex(RuntimeError, "no renderer specified"):
        #     cmds.hydraRender(currentFrame=True)
        with self.assertRaises(RuntimeError):
            cmds.hydraRender(currentFrame=True)

    # An explicitly empty -renderer/-r flag is a hard error, distinct from
    # omitting the flag and relying on currentRenderer.
    def test_EmptyRendererFlagFails(self):
        self._openScene()

        # with self.assertRaisesRegex(RuntimeError, "empty renderer name"):
        #     cmds.hydraRender(renderer="", currentFrame=True)
        with self.assertRaises(RuntimeError):
            cmds.hydraRender(renderer="", currentFrame=True)

    # The legacy (non-Hydra) Maya `render` command -- driven by MtoA's classic
    # Arnold renderer via defaultRenderGlobals.currentRenderer ("arnold", see
    # renderCurrentFrame.ma's ":defaultRenderGlobals.ren" attribute) -- is a
    # separate code path from cmds.hydraRender()/BatchRenderer, and must keep
    # working unaffected by the USD currentRenderer resolution logic added for
    # the Hydra batch-render path.  Authoring a USD currentRenderer value here
    # proves the legacy render is not hijacked by that logic.
    def test_LegacyArnoldRenderIsUnaffected(self):
        self._openScene()
        self._setCurrentRenderer("HdArnoldRendererPlugin")

        self.assertEqual(
            cmds.getAttr("defaultRenderGlobals.currentRenderer"), "arnold")

        cmds.setAttr("defaultResolution.width", 1920)
        cmds.setAttr("defaultResolution.height", 1080)

        # Must not raise: the classic `render` command must be unaffected by
        # GetCurrentRenderer()/GetRenderer(), which are only consulted by
        # hydraRender/BatchRenderer. `render()` returns the full path to the
        # image it created.
        #
        # Pass the camera explicitly: opening a USD stage containing a light
        # (renderCurrentFrame.usda's DistantLight1) makes mayaUsd/UFE add a
        # non-camera "ufeLightDirectional" proxy DAG node for it, which ends
        # up in defaultRenderGlobals' renderable-cameras enumeration and
        # makes camera-less `render()` fail with "Non camera object in the
        # list." Naming "persp" (always present) sidesteps that lookup.
        renderedImagePath = cmds.render("persp")

        # Verify the actual rendered pixels, not just that rendering
        # succeeded without raising.
        self._assertRenderedImageMatchesBaseline(
            self._LEGACY_ARNOLD_BASELINE, renderedImagePath)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
