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
import ufe
from pxr import Gf, Sdf, Usd, UsdGeom

PAYLOAD_BOUNDS_FILTER = "mayaUsdPayloadBoundsDisplayFilter"
BOUNDS_SUFFIX = "/__unloadedPayloadBounds"

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

    def tearDown(self):
        cmds.modelEditor(self.activeEditor, edit=True,
                         pluginObjects=(PAYLOAD_BOUNDS_FILTER, True))
        cmds.modelEditor(self.activeEditor, edit=True, displayAppearance="smoothShaded")
        super(TestUnloadedPayloadBounds, self).tearDown()

    def definePayloadPrim(self, path, assetPath, extentsHint=None):
        prim = self.stage.DefinePrim(path, "Xform")
        prim.GetPayloads().AddPayload(Sdf.Payload(assetPath))
        if extentsHint is not None:
            UsdGeom.ModelAPI.Apply(prim).SetExtentsHint(extentsHint)
        return prim

    def boundsPrims(self):
        cmds.refresh(force=True)
        return [p for p in self.getIndex() if p.endswith(BOUNDS_SUFFIX)]

    def boundsPrimsFor(self, usdPath):
        suffix = usdPath + BOUNDS_SUFFIX
        return [p for p in self.boundsPrims() if p.endswith(suffix)]

    def assertBoundsShown(self, usdPath):
        self.assertEqual(len(self.boundsPrimsFor(usdPath)), 1)

    def assertBoundsHidden(self, usdPath):
        self.assertEqual(len(self.boundsPrimsFor(usdPath)), 0)

    def assertBBoxClose(self, bbox, expectedMin, expectedMax):
        for actual, expected in zip(bbox, list(expectedMin) + list(expectedMax)):
            self.assertAlmostEqual(actual, expected, places=4)

    def test_LoadUnloadToggle(self):
        prim = self.definePayloadPrim(
            "/A", self.cubeFile, [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])

        # Loaded by default: geometry, no stand-in.
        self.assertTrue(prim.IsLoaded())
        self.assertBoundsHidden("/A")

        prim.Unload()
        self.assertFalse(prim.IsLoaded())
        self.assertBoundsShown("/A")

        prim.Load()
        self.assertBoundsHidden("/A")

        prim.Unload()
        self.assertBoundsShown("/A")

    def test_NeverLoaded(self):
        layerFile = testUtils.getTestScene("testStagePayloadsReferences", "payloadBbox.usda")
        cmds.setAttr(self.psPathStr + ".loadPayloads", False)
        cmds.setAttr(self.psPathStr + ".filePath", layerFile, type="string")
        self.assertBoundsShown("/PayloadTest")

    def test_DisplayModes(self):
        prim = self.definePayloadPrim(
            "/A", self.cubeFile, [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])
        prim.Unload()

        for appearance in ["smoothShaded", "wireframe", "flatShaded"]:
            cmds.modelEditor(self.activeEditor, edit=True, displayAppearance=appearance)
            self.assertBoundsShown("/A")

        cmds.modelEditor(self.activeEditor, edit=True,
                         displayAppearance="smoothShaded", wireframeOnShaded=True)
        self.assertBoundsShown("/A")
        cmds.modelEditor(self.activeEditor, edit=True, wireframeOnShaded=False)

    def test_ShowMenuToggle(self):
        prim = self.definePayloadPrim(
            "/A", self.cubeFile, [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])
        prim.Unload()
        self.assertBoundsShown("/A")

        cmds.modelEditor(self.activeEditor, edit=True,
                         pluginObjects=(PAYLOAD_BOUNDS_FILTER, False))
        self.assertBoundsHidden("/A")

        cmds.modelEditor(self.activeEditor, edit=True,
                         pluginObjects=(PAYLOAD_BOUNDS_FILTER, True))
        self.assertBoundsShown("/A")

    def test_NoExtentsHint(self):
        prim = self.definePayloadPrim("/A", self.cubeFile)
        prim.Unload()
        self.assertBoundsHidden("/A")

        # Authoring an extentsHint while unloaded makes the stand-in appear.
        UsdGeom.ModelAPI.Apply(prim).SetExtentsHint(
            [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])
        self.assertBoundsShown("/A")

    def test_Selection(self):
        prim = self.definePayloadPrim(
            "/A", self.cubeFile, [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])
        prim.Unload()

        cmds.select(self.psPathStr + ",/A")
        self.assertBoundsShown("/A")
        cmds.select(clear=True)
        self.assertBoundsShown("/A")

    def test_NestedPayloads(self):
        innerLayer = Sdf.Layer.CreateAnonymous(".usda")
        innerStage = Usd.Stage.Open(innerLayer)
        outerRoot = innerStage.DefinePrim("/Outer", "Xform")
        innerStage.SetDefaultPrim(outerRoot)
        inner = innerStage.DefinePrim("/Outer/Inner", "Xform")
        inner.GetPayloads().AddPayload(Sdf.Payload(self.cubeFile))
        UsdGeom.ModelAPI.Apply(inner).SetExtentsHint(
            [Gf.Vec3f(-1, -1, -1), Gf.Vec3f(1, 1, 1)])

        outer = self.definePayloadPrim(
            "/Outer", innerLayer.identifier, [Gf.Vec3f(-2, -2, -2), Gf.Vec3f(2, 2, 2)])

        outer.Unload()
        self.assertBoundsShown("/Outer")
        self.assertBoundsHidden("/Outer/Inner")

        self.stage.Load("/Outer", Usd.LoadWithoutDescendants)
        self.assertBoundsHidden("/Outer")
        self.assertBoundsShown("/Outer/Inner")

        self.stage.Load("/Outer", Usd.LoadWithDescendants)
        self.assertBoundsHidden("/Outer")
        self.assertBoundsHidden("/Outer/Inner")

    def test_BoundingBoxes(self):
        import mayaUsd.ufe
        prim = self.definePayloadPrim(
            "/A", self.cubeFile, [Gf.Vec3f(-1, -2, -3), Gf.Vec3f(2, 3, 4)])
        UsdGeom.XformCommonAPI(prim).SetTranslate(Gf.Vec3d(5, 0, 0))
        prim.Unload()

        proxyTransform = cmds.listRelatives(self.psPathStr, parent=True, fullPath=True)[0]
        self.assertBBoxClose(
            cmds.exactWorldBoundingBox(proxyTransform), (4, -2, -3), (7, 3, 4))
        self.assertBBoxClose(
            cmds.getAttr(self.psPathStr + ".boundingBoxMin")[0] +
            cmds.getAttr(self.psPathStr + ".boundingBoxMax")[0],
            (4, -2, -3), (7, 3, 4))

        item = ufe.Hierarchy.createItem(ufe.PathString.path(self.psPathStr + ",/A"))
        ufeBBox = ufe.Object3d.object3d(item).boundingBox()
        self.assertBBoxClose(
            [ufeBBox.min.x(), ufeBBox.min.y(), ufeBBox.min.z(),
             ufeBBox.max.x(), ufeBBox.max.y(), ufeBBox.max.z()],
            (-1, -2, -3), (2, 3, 4))

        # Once loaded, the bounds come from the geometry.
        prim.Load()
        self.assertBBoxClose(
            cmds.exactWorldBoundingBox(proxyTransform), (4.5, -0.5, -0.5), (5.5, 0.5, 0.5))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
