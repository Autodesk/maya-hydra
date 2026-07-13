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
# Python test for Arnold custom attribute translation on native Maya nodes
# (mesh, camera, directionalLight, areaLight) via mtoaSIP primvar remapping and
# maya-hydra light schema mapping.
#
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded

HD_ARNOLD = "HdArnoldRendererPlugin"
HD_ARNOLD_OVERRIDE = "mayaHydraRenderOverride_" + HD_ARNOLD


class TestArnoldCustomAttributesMayaNodes(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['mtoa']
    _setHdStormRenderer = False

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

    def _setupMesh(self):
        with PluginLoaded('mtoa'):
            mesh_xform = cmds.polyCube(name='arnoldAttrMesh', width=2, height=2, depth=2)[0]
            mesh_shape = cmds.listRelatives(mesh_xform, shapes=True)[0]
            self._meshShape = cmds.ls(mesh_shape, long=True)[0]
            cmds.setAttr(self._meshShape + ".aiSubdivType", 1)
            cmds.setAttr(self._meshShape + ".aiDispHeight", 0.25)
            cmds.setAttr(self._meshShape + ".aiOpaque", 0)
            cmds.setAttr(self._meshShape + ".aiMatte", 1)
            cmds.setAttr(self._meshShape + ".aiSelfShadows", 0)
        cmds.refresh()

    def _setupCamera(self):
        with PluginLoaded('mtoa'):
            _, cam_shape = cmds.camera(name='arnoldAttrCam')
            self._cameraShape = cmds.ls(cam_shape, long=True)[0]
            cmds.setAttr(self._cameraShape + ".aiExposure", 1.0)
            cmds.setAttr(self._cameraShape + ".aiFocusDistance", 12.5)
        cmds.refresh()

    def _setupDirectionalLight(self):
        with PluginLoaded('mtoa'):
            dir_shape = cmds.directionalLight(name='arnoldAttrDirLight', intensity=1.0)
            self._dirLightShape = cmds.ls(dir_shape, long=True)[0]
            cmds.setAttr(self._dirLightShape + ".aiAngle", 2.5)
            cmds.setAttr(self._dirLightShape + ".aiExposure", 1.5)
            cmds.setAttr(self._dirLightShape + ".aiCastVolumetricShadows", 0)
            cmds.setAttr(self._dirLightShape + ".aiUseColorTemperature", 1)
            cmds.setAttr(self._dirLightShape + ".aiColorTemperature", 3200.0)
        cmds.refresh()

    def _setupAreaLight(self):
        with PluginLoaded('mtoa'):
            shape = cmds.shadingNode(
                "areaLight", asLight=True, name="arnoldAttrAreaLightShape")
            cmds.setAttr(shape + ".intensity", 1.0)
            self._areaLightShape = cmds.ls(shape, long=True)[0]
            cmds.setAttr(self._areaLightShape + ".aiResolution", 256)
            cmds.setAttr(self._areaLightShape + ".aiSpread", 0.5)
            cmds.setAttr(self._areaLightShape + ".aiRoundness", 0.25)
        cmds.refresh()

    def test_meshArnoldPrimvars(self):
        self._setArnoldRenderer()
        self._setupMesh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._meshShape,
                f="ArnoldCustomAttributesMayaNodes.meshArnoldPrimvars")

    def test_cameraArnoldPrimvars(self):
        self._setArnoldRenderer()
        self._setupCamera()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._cameraShape,
                f="ArnoldCustomAttributesMayaNodes.cameraArnoldPrimvars")

    def test_directionalLightAttributes(self):
        self._setArnoldRenderer()
        self._setupDirectionalLight()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._dirLightShape,
                f="ArnoldCustomAttributesMayaNodes.directionalLightAttributes")

    def test_areaLightArnoldPrimvars(self):
        self._setArnoldRenderer()
        self._setupAreaLight()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._areaLightShape,
                f="ArnoldCustomAttributesMayaNodes.areaLightArnoldPrimvars")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
