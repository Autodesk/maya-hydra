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
# Python test for the custom node translation functionality of the
# HdArnoldMtoaSceneIndexPlugin (mtoaSIP).
#
# Custom node translation: mayaCustomDagNode prims emitted by maya-hydra for
#    Arnold plugin nodes are re-typed and have their attributes mapped to
#    renderer-specific Hydra schemas.
#    Tested: aiPhotometricLight -> sphereLight
#            aiStandIn          -> ArnoldProcedural
#            aiVolume           -> ArnoldVolume
#    NOT tested here (have existing maya-hydra adapters that bypass mayaCustomDagNode):
#            aiSkyDomeLight, aiAreaLight
#
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded

HD_ARNOLD = "HdArnoldRendererPlugin"
HD_ARNOLD_OVERRIDE = "mayaHydraRenderOverride_" + HD_ARNOLD


class TestArnoldCustomNodes(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['mtoa']
    _setHdStormRenderer = False     # Need to use Arnold renderer for the tests

    def tearDown(self):
        """Test will hang if Arnold renderer is not reset before Maya exits."""
        self.setViewport2Renderer()

    def _setArnoldRenderer(self):
        """Activate the Arnold Hydra renderer so mtoaSIP is in the scene index chain."""
        self.activeEditor = cmds.playblast(activeEditor=1)
        cmds.modelEditor(
            self.activeEditor, e=1,
            rendererOverrideName=HD_ARNOLD_OVERRIDE)
        cmds.refresh(f=1)
        activeOverride = cmds.modelEditor(
            self.activeEditor, q=1, rendererOverrideName=True)
        self.assertEqual(
            activeOverride,
            HD_ARNOLD_OVERRIDE,
            "Arnold Hydra renderer override not available. "
            "Ensure HdArnoldRendererPlugin is on PXR_PLUGINPATH_NAME.")

    def _setupPhotometricLight(self):
        with PluginLoaded('mtoa'):
            xform = cmds.createNode('transform', name='photometric1')
            shape = cmds.createNode(
                'aiPhotometricLight',
                name='photometricShape1',
                parent=xform,
            )
            self._photometricShape = cmds.ls(shape, long=True)[0]
            cmds.setAttr(self._photometricShape + ".intensity", 2.5)
            cmds.setAttr(self._photometricShape + ".aiExposure", 3.0)
            cmds.setAttr(self._photometricShape + ".aiFilename",
                         "/path/to/test.ies", type="string")
            cmds.setAttr(self._photometricShape + ".aiSamples", 5)
        cmds.refresh()

    def _setupStandIn(self):
        with PluginLoaded('mtoa'):
            xform = cmds.createNode('transform', name='standIn1')
            shape = cmds.createNode(
                'aiStandIn',
                name='standInShape1',
                parent=xform,
            )
            self._standInShape = cmds.ls(shape, long=True)[0]
            cmds.setAttr(self._standInShape + ".dso",
                         "/path/to/test.ass", type="string")
            # Prevent MtoA from loading the non-existent .ass file while
            # HdArnold is active; the C++ test only verifies scene-index translation.
            cmds.setAttr(self._standInShape + ".standInDrawOverride", 4)
        cmds.refresh()

    def _setupVolume(self):
        with PluginLoaded('mtoa'):
            xform = cmds.createNode('transform', name='volume1')
            shape = cmds.createNode(
                'aiVolume',
                name='volumeShape1',
                parent=xform,
            )
            self._volumeShape = cmds.ls(shape, long=True)[0]
            cmds.setAttr(self._volumeShape + ".filename",
                         "/path/to/test.vdb", type="string")
        cmds.refresh()

    def test_photometricLightTranslation(self):
        self._setArnoldRenderer()
        self._setupPhotometricLight()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._photometricShape,
                f="MtoaSIP.photometricLightTranslation")

    def test_standInTranslation(self):
        self._setArnoldRenderer()
        self._setupStandIn()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._standInShape,
                f="MtoaSIP.standInTranslation")

    def test_volumeTranslation(self):
        self._setArnoldRenderer()
        self._setupVolume()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._volumeShape,
                f="MtoaSIP.volumeTranslation")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
