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
# Viewport snapshot regression for isolate select exercised on each shape type
# individually and pairwise: a Maya sphere, a USD sphere, a USD camera and a
# USD light, all living in the same scene.  Visibility is verified both visually
# (snapshot comparison) and programmatically (Hydra scene index visibility).
#
import fixturesUtils
import mayaUsd
import mayaUsd.lib
import mayaUsd_createStageWithNewLayer
import mtohUtils
import mayaUtils
import maya.cmds as cmds
import maya.mel as mel
from pxr import UsdGeom, UsdLux, Gf


def enableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 1" % modelPanel)


def disableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 0" % modelPanel)


class TestIsolateSelectPerShape(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    _requiredPlugins = ['mayaHydraCppTests', 'drawUfe']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def assertVisible(self, paths):
        for p in paths:
            cmds.mayaHydraCppTest(p, f="TestHydraPrim.isVisible")

    def assertNotVisible(self, paths):
        for p in paths:
            cmds.mayaHydraCppTest(p, f="TestHydraPrim.notVisible")

    def test_isolateSelectPerShape(self):
        """Create a scene with a Maya sphere, a USD sphere (duplicated from
        Maya), a USD camera and a USD directional light.  For each shape
        individually and for every pairwise combination, isolate-select and
        verify visibility + compare the viewport snapshot against the
        baseline.
        """

        # 1. Create a Maya sphere at the origin.
        sphere = cmds.polySphere(radius=1)
        cmds.refresh()

        # 2. Create a USD stage and duplicate the Maya sphere into it.
        proxyShapePathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(proxyShapePathStr).GetStage()
        mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(sphere[0], long=True)[0], proxyShapePathStr)

        # 3. Move the Maya sphere aside so it doesn't overlap with its USD copy.
        cmds.move(3.0, 0.0, 0.0, sphere[0], absolute=True)

        # 4. Create a Maya camera and duplicate it into the USD stage.
        camera = cmds.camera()
        cmds.move(-3.0, 0.0, 0.0, camera[0], absolute=True)
        mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(camera[0], long=True)[0], proxyShapePathStr)
        cmds.delete(camera[0])

        # 5. Create a USD distant light directly on the stage.
        lightXform = UsdGeom.Xform.Define(stage, "/directionalLight1")
        lightXform.AddTranslateOp().Set(Gf.Vec3d(0.0, 0.0, -3.0))
        lightXform.AddRotateXYZOp().Set(Gf.Vec3f(-30, 45, 0))
        UsdLux.DistantLight.Define(stage, "/directionalLight1/directionalLightShape")

        cmds.refresh()

        self.setBasicCam(dist=6)
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayAppearance='smoothShaded')
        cmds.modelEditor(panel, edit=True, displayLights='all')
        cmds.select(clear=True)
        cmds.refresh()

        # Snapshot the full scene before any isolate select to verify all
        # four shapes are present and visible.
        self.assertSnapshotClose(
            "isolateSelectPerShape_fullScene.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT,
        )

        modelPanel = "modelPanel4"
        stagePath = "|stage1|stageShape1"
        mayaSpherePath = "|pSphere1"
        mayaSphereShapePath = "|pSphere1|pSphereShape1"
        usdSpherePath = stagePath + ",/pSphere1"
        usdCamPath = stagePath + ",/camera1"
        usdLightPath = stagePath + ",/directionalLight1/directionalLightShape"

        # Lights are intentionally kept visible at the scene-index level even
        # when excluded from isolate select (they keep contributing lighting,
        # matching VP2 behavior).  So the light path is NOT in allPaths used
        # for notVisible assertions — only its gizmo appearance is validated
        # visually via snapshots.
        allPaths = [mayaSpherePath, mayaSphereShapePath,
                    usdSpherePath, usdCamPath]

        # Each entry: (selectPaths, visiblePaths, snapshotName).
        # visiblePaths lists non-light prims expected visible; the light
        # is always visible at the scene-index level (see comment above).
        # Single-object isolate selection.
        cases = [
            ([mayaSpherePath],
             [mayaSpherePath, mayaSphereShapePath],
             "isolateSelectPerShape_mayaSphere"),
            ([usdCamPath],
             [usdCamPath],
             "isolateSelectPerShape_usdCamera"),
            ([usdSpherePath],
             [usdSpherePath],
             "isolateSelectPerShape_usdSphere"),
            ([usdLightPath],
             [],
             "isolateSelectPerShape_usdLight"),
        ]

        # Pairwise (2-by-2) isolate selection — all combinations of 4 objects.
        cases += [
            ([mayaSpherePath, usdSpherePath],
             [mayaSpherePath, mayaSphereShapePath, usdSpherePath],
             "isolateSelectPerShape_mayaSphere_usdSphere"),
            ([mayaSpherePath, usdCamPath],
             [mayaSpherePath, mayaSphereShapePath, usdCamPath],
             "isolateSelectPerShape_mayaSphere_usdCamera"),
            ([mayaSpherePath, usdLightPath],
             [mayaSpherePath, mayaSphereShapePath],
             "isolateSelectPerShape_mayaSphere_usdLight"),
            ([usdSpherePath, usdCamPath],
             [usdSpherePath, usdCamPath],
             "isolateSelectPerShape_usdSphere_usdCamera"),
            ([usdSpherePath, usdLightPath],
             [usdSpherePath],
             "isolateSelectPerShape_usdSphere_usdLight"),
            ([usdCamPath, usdLightPath],
             [usdCamPath],
             "isolateSelectPerShape_usdCamera_usdLight"),
        ]

        for selectPaths, visiblePaths, snapshotName in cases:
            cmds.select(selectPaths)
            enableIsolateSelect(modelPanel)

            notVisiblePaths = [p for p in allPaths if p not in visiblePaths]
            self.assertVisible(visiblePaths)
            self.assertNotVisible(notVisiblePaths)

            cmds.select(clear=True)
            cmds.refresh()

            self.assertSnapshotClose(
                snapshotName + ".png",
                self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT,
            )

            disableIsolateSelect(modelPanel)
            cmds.refresh()


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
