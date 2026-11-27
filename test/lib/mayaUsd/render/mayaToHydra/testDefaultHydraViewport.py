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
"""Verify that HdFlash ("Viewport") is enforced as the default Hydra viewport renderer."""

import unittest

import maya.cmds as cmds
import maya.mel as mel

import fixturesUtils
import mayaUtils
import mtohUtils
import rendererUtils

FLASH_PLUGIN = rendererUtils.getRendererPluginName(rendererUtils.TEST_RENDERER_FLASH)
FLASH_OVERRIDE = rendererUtils.getRendererOverrideName(rendererUtils.TEST_RENDERER_FLASH)
STORM_OVERRIDE = rendererUtils.getRendererOverrideName(rendererUtils.TEST_RENDERER_STORM)


#@unittest.skipUnless(mayaUtils.hydraFixLevel() >= 7, "Requires Maya Hydra fix level 7 or later.")
class TestDefaultHydraViewport(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    # Do not let MayaHydraBaseTestCase.setUp() force MAYAHYDRA_TEST_RENDERER
    # (e.g. HdStorm), so these tests observe Maya's own default renderer logic.
    _setTestRenderer = False

    def setUp(self):
        super().setUp()
        if not mtohUtils.mayaHasHydraMode():
            self.skipTest("Requires Maya with Global Hydra support (mayaUseHydra).")
        #if not mel.eval("exists applyDefaultHydraToModelPanels"):
        #    self.skipTest("Requires Maya with default Hydra viewport enforcement.")
        if FLASH_PLUGIN not in (cmds.mayaHydra(listRenderers=True) or []):
            self.skipTest("Requires HdFlashRendererPlugin.")

    @staticmethod
    def _nonStereoModelEditors():
        """Return all model editors managed by Global Hydra (excluding stereo views)."""
        editors = []
        for panel in cmds.getPanel(type="modelPanel") or []:
            editor = cmds.modelPanel(panel, query=True, modelEditor=True)
            if editor and cmds.modelEditor(editor, query=True, viewType=True) != "stereoCameraView":
                editors.append(editor)
        return editors

    def _enableGlobalHydra(self):
        """Transition from Viewport 2.0 to Global Hydra as the Preferences UI does."""
        mtohUtils.setRenderer("modelPanel4", "")
        cmds.optionVar(intValue=("mayaUseHydra", 1))
        # Maya applies mayaUseHydra changes via an idle-time scriptJob, which does
        # not execute inside Maya's startup -command test harness; invoke it directly.
        mel.eval("applyUseHydraToModelPanels")
    
    def _setResetModelViewsOnNewScene(self, value):
        origExists = cmds.optionVar(exists="ResetModelViewsOnNewScene")
        origValue = cmds.optionVar(query="ResetModelViewsOnNewScene") if origExists else 1
        self.addCleanup(
            lambda: cmds.optionVar(intValue=("ResetModelViewsOnNewScene", origValue))
        )
        cmds.optionVar(intValue=("ResetModelViewsOnNewScene", value))

    def test_flashIsFirstHydraRenderer(self):
        # HdFlash must be registered first in both mayaHydra's plugin list and
        # Maya's viewport override list (which drives Renderer menu order), even
        # when higher-priority renderer plugins are present.
        renderers = cmds.mayaHydra(listRenderers=True)
        self.assertEqual(renderers[0], FLASH_PLUGIN)

        overrides = cmds.modelEditor("modelPanel4", query=True, rendererOverrideList=True) or []
        hydraOverrides = [o for o in overrides if o.startswith("mayaHydra")]
        self.assertGreater(len(hydraOverrides), 0)
        self.assertEqual(hydraOverrides[0], FLASH_OVERRIDE)

    def test_firstEnableSetsAllPanelsToFlash(self):
        # First enabling Use Hydra must move every model panel (visible and
        # unparented orthographic views) from VP2 to HdFlash.
        self._enableGlobalHydra()
        editors = self._nonStereoModelEditors()
        self.assertGreaterEqual(len(editors), 4)
        for editor in editors:
            self.assertEqual(
                cmds.modelEditor(editor, query=True, rendererOverrideName=True),
                FLASH_OVERRIDE,
            )

    def test_newSceneResetsToFlashWhenResetModelViewsEnabled(self):
        # With ResetModelViewsOnNewScene on, a new scene returns every panel to
        # the default Hydra renderer (HdFlash), discarding other delegates.
        self._enableGlobalHydra()
        self._setResetModelViewsOnNewScene(1)
        cmds.modelEditor("modelPanel4", edit=True, rendererOverrideName=STORM_OVERRIDE)

        cmds.file(force=True, new=True)

        for editor in self._nonStereoModelEditors():
            self.assertEqual(
                cmds.modelEditor(editor, query=True, rendererOverrideName=True),
                FLASH_OVERRIDE,
            )

    def test_newSceneKeepsDelegateWhenResetModelViewsDisabled(self):
        # With ResetModelViewsOnNewScene off, a new scene respects the delegate
        # already chosen on each panel.
        self._enableGlobalHydra()
        self._setResetModelViewsOnNewScene(0)
        cmds.modelEditor("modelPanel4", edit=True, rendererOverrideName=STORM_OVERRIDE)

        cmds.file(force=True, new=True)

        self.assertEqual(
            cmds.modelEditor("modelPanel4", query=True, rendererOverrideName=True),
            STORM_OVERRIDE,
        )

    def test_savedViewportSettingsAndStartupRespectDelegate(self):
        self._enableGlobalHydra()
        cmds.modelEditor("modelPanel4", edit=True, rendererOverrideName=STORM_OVERRIDE)

        # Saving preferences on exit calls saveViewportSettings, which serializes
        # each modelPanel into an optionVar; the Hydra delegate is now persisted
        # so user preferences are restored on the next session.
        cmds.saveViewportSettings()
        savedCmd = cmds.optionVar(query="modelPanel4") or ""
        self.assertIn(STORM_OVERRIDE, savedCmd)

        # Simulate the deferred startup / plugin-load callback (initAfter.mel):
        # panels with a Hydra delegate keep it.
        mel.eval("applyUseHydraToModelPanels")
        self.assertEqual(
            cmds.modelEditor("modelPanel4", query=True, rendererOverrideName=True),
            STORM_OVERRIDE,
        )

        # Panels with no Hydra renderer (plain VP2) move to the default, HdFlash.
        # setRenderer(..., "") also turns Use Hydra off, so turn it back on
        # directly (without applying) to leave modelPanel4 with no Hydra override.
        mtohUtils.setRenderer("modelPanel4", "")
        cmds.optionVar(intValue=("mayaUseHydra", 1))
        mel.eval("applyUseHydraToModelPanels")
        self.assertEqual(
            cmds.modelEditor("modelPanel4", query=True, rendererOverrideName=True),
            FLASH_OVERRIDE,
        )

    def test_singleViewportToFourViewKeepsPerspOnStormAndOtherThreeOnFlash(self):
        # Switching a single perspective viewport to Storm and then entering
        # Four View must keep the perspective pane on Storm while the other
        # three panes (Top, Side, Front) are on HdFlash.
        self.addCleanup(
            lambda: mel.eval('setNamedPanelLayout (localizedPanelLabel("Single Perspective View"))')
        )
        self._enableGlobalHydra()
        mel.eval('setNamedPanelLayout (localizedPanelLabel("Single Perspective View"))')

        perspControl = cmds.paneLayout("viewPanes", query=True, pane1=True)
        perspPanel = cmds.getPanel(containing=perspControl)
        perspEditor = cmds.modelPanel(perspPanel, query=True, modelEditor=True)

        cmds.modelEditor(perspEditor, edit=True, rendererOverrideName=STORM_OVERRIDE)
        mel.eval('setNamedPanelLayout (localizedPanelLabel("Four View"))')

        paneControls = [
            cmds.paneLayout("viewPanes", query=True, pane1=True),
            cmds.paneLayout("viewPanes", query=True, pane2=True),
            cmds.paneLayout("viewPanes", query=True, pane3=True),
            cmds.paneLayout("viewPanes", query=True, pane4=True),
        ]
        paneEditors = [
            cmds.modelPanel(cmds.getPanel(containing=ctrl), query=True, modelEditor=True)
            for ctrl in paneControls
        ]
        self.assertEqual(len(paneEditors), 4)
        self.assertIn(perspEditor, paneEditors)

        for editor in paneEditors:
            expected = STORM_OVERRIDE if editor == perspEditor else FLASH_OVERRIDE
            self.assertEqual(
                cmds.modelEditor(editor, query=True, rendererOverrideName=True),
                expected,
            )

    def test_newModelPanelUsesFlash(self):
        # Newly created model panels must initialize directly to HdFlash (not VP2
        # or another panel's active delegate) while Use Hydra is enabled.
        self._enableGlobalHydra()
        cmds.modelEditor("modelPanel4", edit=True, rendererOverrideName=STORM_OVERRIDE)

        newPanel = cmds.modelPanel(unParent=True)
        self.addCleanup(
            lambda: cmds.deleteUI(newPanel, panel=True)
            if cmds.panel(newPanel, exists=True)
            else None
        )
        newEditor = cmds.modelPanel(newPanel, query=True, modelEditor=True)
        self.assertEqual(
            cmds.modelEditor(newEditor, query=True, rendererOverrideName=True),
            FLASH_OVERRIDE,
        )


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
