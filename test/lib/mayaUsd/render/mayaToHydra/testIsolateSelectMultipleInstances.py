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
# Regression test for the bug where pre-selecting multiple point instances of
# the same PointInstancer together with a USD camera and then enabling isolate
# select produced an incorrect (empty or stale) isolate selection.
#
# The scene contains a large Bifrost-generated PointInstancer
# (/bots_world/many_robots).  Three of its instances (225, 230, 250) are
# selected alongside a freshly-created USD camera, and isolate select is then
# enabled.  Only those four items must be visible in the resulting viewport
# snapshot.
#
import fixturesUtils
import mayaUsd
import mayaUsd.lib
import mtohUtils
import mayaUtils
import maya.cmds as cmds
import maya.mel as mel
import unittest

from pxr import Usd


def enableIsolateSelect(modelPanel):
    # See comments in cpp/testIsolateSelect.py for why the MEL script must be
    # used instead of cmds.isolateSelect(..., state=1).
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 1" % modelPanel)


def disableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 0" % modelPanel)


# skip at class level to avoid loading Bifrost.
@unittest.skipIf(Usd.GetVersion()[1] == 24, "Bifrost errors with OpenUSD 0.24.11.")
class TestIsolateSelectMultipleInstances(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    # drawUfe is required because USD cameras are drawn by UFE; without it the
    # USD camera gizmo is invisible in the viewport snapshot.
    _requiredPlugins = ['bifrostGraph', 'drawUfe']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 3

    def test_isolateSelectMultipleInstances(self):
        """Regression test: pre-selecting multiple instances of the same
        PointInstancer together with a USD camera, then enabling isolate
        select, must show only those selected items.

        The underlying bug was that Maya represents multiple selected instances
        of the same PointInstancer as a single component-style view-selected
        entry whose objectStrings array has one element per instance
        (length > 1).  The old code silently dropped any such entry, producing
        an empty isolate selection that triggered a full-scene dirty pass and
        resulted in a frozen or incorrectly empty viewport.
        """

        mayaUtils.openTestScene("testBifrost", "bifrost_node_update_hydra.ma")
        self.setHdStormRenderer()

        proxyShapePathStr = "|mayaUsdProxy1|mayaUsdProxyShape1"

        # Create a Maya camera, duplicate it into the existing USD stage so it
        # gets a full USD Camera schema representation (focal length, aperture,
        # etc.) and therefore shows up as a camera gizmo in the viewport, then
        # remove the original Maya camera.  Using stage.DefinePrim() directly
        # would produce a bare prim with no schema attributes and no visible
        # gizmo.
        camera = cmds.camera()
        mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(camera[0], long=True)[0], proxyShapePathStr)
        cmds.delete(camera[0])

        usdCamPath  = proxyShapePathStr + ",/camera1"
        instance250 = proxyShapePathStr + ",/bots_world/many_robots/250"
        instance225 = proxyShapePathStr + ",/bots_world/many_robots/225"
        instance230 = proxyShapePathStr + ",/bots_world/many_robots/230"

        self.setBasicCam(dist=15)
        cmds.refresh()

        modelPanel = mayaUtils.activeModelPanel()
        cmds.modelEditor(modelPanel, edit=True, cameras=True)

        # Pre-select the 3 instances and the USD camera, then enable isolate
        # select.  Selecting multiple instances of the same PointInstancer
        # before enabling (rather than using loadSelected=True afterwards) is
        # the exact workflow that triggered the bug.
        cmds.select([instance250, instance225, instance230])
        cmds.select(usdCamPath, toggle=True)

        enableIsolateSelect(modelPanel)
        cmds.refresh()

        self.assertSnapshotClose(
            "isolateSelectMultipleInstances.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT,
        )

        disableIsolateSelect(modelPanel)
        cmds.refresh()


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
