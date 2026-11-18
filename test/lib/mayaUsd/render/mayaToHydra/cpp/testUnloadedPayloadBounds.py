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

import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import testUtils
import mayaUtils
from testUtils import PluginLoaded
from pxr import Gf, Sdf, UsdGeom

EXTENTS_HINT = [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)]

class TestUnloadedPayloadBounds(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Plugin object display filter backing the Show menu toggle, registered by MayaUsd.
    PAYLOAD_BOUNDS_DISPLAY_FILTER = 'mayaUsdPayloadBoundsDisplayFilter'

    IMAGEDIFF_FAIL_THRESHOLD = 0.05
    IMAGEDIFF_FAIL_PERCENT = 0.5

    def setUp(self):
        super(TestUnloadedPayloadBounds, self).setUp()

        # Keep the C++ tests plugin loaded for the whole test: PluginLoaded creates a new
        # scene on exit, which would invalidate the stage between assertions.
        self.enterContext(PluginLoaded("mayaHydraCppTests"))

        import mayaUsd_createStageWithNewLayer
        from mayaUsd import lib as mayaUsdLib

        mayaUtils.openNewScene()
        self.psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        self.stage = mayaUsdLib.GetPrim(self.psPathStr).GetStage()
        self.cubeFile = testUtils.getTestScene("testStagePayloadsReferences", "cube.usda")
        self.setTestRenderer()
        self.setBasicCam(3)
        cmds.select(clear=True)

    def definePayloadPrim(self, path, extentsHint=None):
        prim = self.stage.DefinePrim(path, "Xform")
        prim.GetPayloads().AddPayload(Sdf.Payload(self.cubeFile))
        if extentsHint is not None:
            UsdGeom.ModelAPI.Apply(prim).SetExtentsHint(extentsHint)
        return prim

    def assertBoundsCount(self, usdPath, expectedCount):
        # The boxes live in the secondary graphics pass (Storm), which the selected
        # renderer's rprims (e.g. Flash) do not expose, so check them from C++.
        cmds.refresh(force=True)
        cmds.mayaHydraCppTest(
            usdPath, str(expectedCount),
            f="UnloadedPayloadBounds.countBoundsPrims")

    def assertImageClose(self, imageName):
        cmds.refresh(force=True)
        self.assertSnapshotClose(
            imageName, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def assertBoundsShown(self, usdPath):
        self.assertBoundsCount(usdPath, 1)

    def assertBoundsHidden(self, usdPath):
        self.assertBoundsCount(usdPath, 0)

    def test_NoExtentsHint(self):
        # Without an extents hint there is nothing to size a box from.
        prim = self.definePayloadPrim("/A")
        prim.Unload()
        self.assertFalse(prim.IsLoaded())
        self.assertBoundsHidden("/A")
        self.assertImageClose("unloadedPayloadNoExtentsHint.png")

    def test_ExtentsHintLoadUnload(self):
        prim = self.definePayloadPrim("/A", EXTENTS_HINT)

        # Loaded: real geometry is drawn, no bounding box.
        self.assertTrue(prim.IsLoaded())
        self.assertBoundsHidden("/A")
        self.assertImageClose("unloadedPayloadLoaded.png")

        # Unloaded: Bounding box is present.
        prim.Unload()
        self.assertFalse(prim.IsLoaded())
        self.assertBoundsShown("/A")
        self.assertImageClose("unloadedPayloadBoundingBox.png")

        # Re-loaded: real geometry is drawn again, no bounding box.
        prim.Load()
        self.assertTrue(prim.IsLoaded())
        self.assertBoundsHidden("/A")
        self.assertImageClose("unloadedPayloadLoaded.png")

    def test_ShowMenuToggle(self):
        # The "USD Payload Bounding Boxes" toggle of the viewport Show menu is a plugin
        # object display filter registered by MayaUsd, set per model panel.
        panel = mayaUtils.activeModelPanel()
        self.assertIsNotNone(panel)
        filterName = self.PAYLOAD_BOUNDS_DISPLAY_FILTER

        def setToggle(value):
            cmds.modelEditor(panel, edit=True, pluginObjects=(filterName, value))

        def getToggle():
            return bool(cmds.modelEditor(panel, query=True, queryPluginObjects=filterName))

        # Restore the original state so other tests are not affected.
        self.addCleanup(setToggle, getToggle())
        cmds.setFocus(panel)

        prim = self.definePayloadPrim("/A", EXTENTS_HINT)
        prim.Unload()
        self.assertFalse(prim.IsLoaded())

        # Toggle on: boxes shown.
        setToggle(True)
        self.assertTrue(getToggle())
        self.assertBoundsShown("/A")
        self.assertImageClose("unloadedPayloadBoundingBox.png")

        # Toggle off: boxes hidden, even though the payload is still unloaded. Nothing is
        # drawn, as the payload has no geometry to show.
        setToggle(False)
        self.assertFalse(getToggle())
        self.assertBoundsHidden("/A")
        self.assertImageClose("unloadedPayloadNoExtentsHint.png")

        # Toggle back on: boxes shown again.
        setToggle(True)
        self.assertTrue(getToggle())
        self.assertBoundsShown("/A")
        self.assertImageClose("unloadedPayloadBoundingBox.png")

    def test_PickBoundingBox(self):
        # The box is wireframe, so its center is empty. Click on a corner
        # located by a marker prim instead.
        prim = self.definePayloadPrim("/A", EXTENTS_HINT)
        prim.Unload()
        self.assertBoundsShown("/A")

        marker = self.stage.DefinePrim("/CornerMarker", "Xform")
        UsdGeom.XformCommonAPI(marker).SetTranslate(Gf.Vec3d(1, 1, 1))
        cmds.refresh(force=True)

        cmds.mayaHydraCppTest(
            self.psPathStr + ",/A",
            self.psPathStr + ",/CornerMarker",
            f="TestUsdPicking.pickPrimWithMarker")

    def test_WireframeColors(self):
        prim = self.definePayloadPrim("/A", EXTENTS_HINT)
        prim.Unload()
        self.assertBoundsShown("/A")

        # /B only serves to take the lead from /A, so that /A can be selected without
        # being the lead object.
        self.stage.DefinePrim("/B", "Xform")

        pathA = self.psPathStr + ",/A"
        pathB = self.psPathStr + ",/B"

        # Nothing is selected.
        cmds.select(clear=True)
        self.assertImageClose("unloadedPayloadWireframe_unselected.png")

        # /A is selected first, it should be the lead.
        cmds.select(pathA, replace=True)
        self.assertImageClose("unloadedPayloadWireframe_lead.png")

        # /A remains selected, but is no longer the lead.
        cmds.select(pathB, add=True)
        self.assertImageClose("unloadedPayloadWireframe_active.png")

        # /A remains selected, and takes the lead back.
        cmds.select(pathB, deselect=True)
        self.assertImageClose("unloadedPayloadWireframe_lead.png")

        # Go back to nothing selected.
        cmds.select(clear=True)
        self.assertImageClose("unloadedPayloadWireframe_unselected.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
