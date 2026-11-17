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

BOUNDS_SUFFIX = "/__unloadedPayloadBounds"
EXTENTS_HINT = [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)]

class TestUnloadedPayloadBounds(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setUp(self):
        super(TestUnloadedPayloadBounds, self).setUp()
        import mayaUsd_createStageWithNewLayer
        from mayaUsd import lib as mayaUsdLib

        mayaUtils.openNewScene()
        self.psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        self.stage = mayaUsdLib.GetPrim(self.psPathStr).GetStage()
        self.cubeFile = testUtils.getTestScene("testStagePayloadsReferences", "cube.usda")
        self.setTestRenderer()
        self.setBasicCam(10)
        cmds.select(clear=True)

    def definePayloadPrim(self, path, extentsHint=None):
        prim = self.stage.DefinePrim(path, "Xform")
        prim.GetPayloads().AddPayload(Sdf.Payload(self.cubeFile))
        if extentsHint is not None:
            UsdGeom.ModelAPI.Apply(prim).SetExtentsHint(extentsHint)
        return prim

    def boundsPrimsFor(self, usdPath):
        cmds.refresh(force=True)
        suffix = usdPath + BOUNDS_SUFFIX
        return [p for p in self.getIndex() if p.endswith(suffix)]

    def assertBoundsShown(self, usdPath):
        self.assertEqual(len(self.boundsPrimsFor(usdPath)), 1)

    def assertBoundsHidden(self, usdPath):
        self.assertEqual(len(self.boundsPrimsFor(usdPath)), 0)

    def test_NoExtentsHint(self):
        # Without an extents hint there is nothing to size a box from.
        prim = self.definePayloadPrim("/A")
        prim.Unload()
        self.assertFalse(prim.IsLoaded())
        self.assertBoundsHidden("/A")

    def test_ExtentsHintLoadUnload(self):
        prim = self.definePayloadPrim("/A", EXTENTS_HINT)

        # Loaded: real geometry is drawn, no bounding box.
        self.assertTrue(prim.IsLoaded())
        self.assertBoundsHidden("/A")

        # Unloaded: Bounding box is present.
        prim.Unload()
        self.assertFalse(prim.IsLoaded())
        self.assertBoundsShown("/A")

        # Re-loaded: real geometry is drawn again, no bounding box.
        prim.Load()
        self.assertTrue(prim.IsLoaded())
        self.assertBoundsHidden("/A")

    def test_PickBoundingBox(self):
        # The box is wireframe, so its center is empty. Click on a corner
        # located by a marker prim instead.
        prim = self.definePayloadPrim("/A", EXTENTS_HINT)
        prim.Unload()
        self.assertBoundsShown("/A")

        marker = self.stage.DefinePrim("/CornerMarker", "Xform")
        UsdGeom.XformCommonAPI(marker).SetTranslate(Gf.Vec3d(1, 1, 1))
        cmds.refresh(force=True)

        with PluginLoaded("mayaHydraCppTests"):
            cmds.mayaHydraCppTest(
                self.psPathStr + ",/A",
                self.psPathStr + ",/CornerMarker",
                f="TestUsdPicking.pickPrimWithMarker")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
