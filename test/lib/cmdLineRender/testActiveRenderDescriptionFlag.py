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

import os

import maya.cmds as cmds
import maya.mel as mel
import fixturesUtils
import mtohUtils
from mayaHydra.renderSettings.activeRenderDescription import setActiveRenderDescription


_SCENE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "scenes", "activeRenderDescription", "twoRenderSettings.ma")

_NODE = "UsdDefaultRenderDescription"
_ATTR = _NODE + ".activeRenderDescriptionPath"
_MAIN = "|renderSettings|renderSettingsShape,/Render/MainRender"
_OVERRIDE = "|renderSettings|renderSettingsShape,/Render/OverrideRender"


class TestActiveRenderDescriptionFlag(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _initializeStandalone = True
    _setTestRenderer = False
    _requiredPlugins = ["mtoa", "mayaHydra"]

    def test_SetActiveRenderDescription(self):
        # Needs the mayaUsd plugin, so it cannot be imported at module level.
        from mayaHydra.renderSettings.utils import getRenderSettingsPrim

        # MayaUSD relies on the plugin load defining this proc.
        self.assertTrue(mel.eval('exists "mayaHydraSetActiveRenderDescription"'))

        cmds.file(_SCENE, open=True, force=True)
        self.assertEqual(cmds.getAttr(_ATTR), _MAIN)
        self.assertEqual(str(getRenderSettingsPrim().GetPath()), "/Render/MainRender")

        with self.assertRaises(RuntimeError):
            setActiveRenderDescription("")
        self.assertEqual(cmds.getAttr(_ATTR), _MAIN)

        mel.eval('mayaHydraSetActiveRenderDescription("%s")' % _OVERRIDE)

        self.assertEqual(cmds.getAttr(_ATTR), _OVERRIDE)
        self.assertEqual(str(getRenderSettingsPrim().GetPath()), "/Render/OverrideRender")

    def test_NonExistentPath(self):
        cmds.file(_SCENE, open=True, force=True)
        with self.assertRaisesRegex(RuntimeError, "No render description prim found at path"):
            setActiveRenderDescription(
                "|renderSettings|renderSettingsShape,/Render/DoesNotExist")
        self.assertEqual(cmds.getAttr(_ATTR), _MAIN)

    def test_NotARenderDescriptionPrim(self):
        cmds.file(_SCENE, open=True, force=True)
        # MainProduct exists, but is a RenderProduct, not a RenderSettings or RenderPass.
        with self.assertRaisesRegex(RuntimeError, "No render description prim found at path"):
            setActiveRenderDescription(
                "|renderSettings|renderSettingsShape,/Render/MainProduct")
        self.assertEqual(cmds.getAttr(_ATTR), _MAIN)

    def test_NoDefaultRenderDescriptionNode(self):
        cmds.file(new=True, force=True)
        if cmds.objExists(_NODE):
            cmds.lockNode(_NODE, lock=False)
            cmds.delete(_NODE)
        self.assertFalse(cmds.objExists(_NODE))

        with self.assertRaises(RuntimeError):
            setActiveRenderDescription(_OVERRIDE)


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
