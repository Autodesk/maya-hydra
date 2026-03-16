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
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded


class TestLightPrimvars(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']

    # Create a minimal Arnold light scene for the C++ primvars tests.
    # Sets known default/non-default attributes and records the light shape path.
    def setupScene(self):
        cmds.file(new=True, force=True)
        with PluginLoaded('mtoa'):
            # Create aiSkyDomeLight (Arnold light) via createNode
            light_transform = cmds.createNode('transform', name='aiSkyDomeLight1')
            light_shape = cmds.createNode(
                'aiSkyDomeLight',
                name='aiSkyDomeLightShape1',
                parent=light_transform,
            )
            light_shape = cmds.ls(light_shape, long=True)[0]

            # Set non-default Arnold attrs so they get translated to primvars
            # aiExposure default is 0; set to 2.0
            cmds.setAttr(light_shape + ".aiExposure", 2.0)
            # aiDiffuse default is 1.0; set to 0.5
            cmds.setAttr(light_shape + ".aiDiffuse", 0.5)
            # aiSpecular stays at default 1.0 -> should NOT appear as primvar
            # aiShadowDensity (not in param attr list) - for NonParamAttrTriggersOnlyPrimvarDirty
            cmds.setAttr(light_shape + ".aiShadowDensity", 0)

        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhLightShape", light_shape))
        cmds.refresh()

    # What: translation and dirtying for non-default light primvars.
    # How: build the scene, then run the C++ test that inspects primvars and updates aiExposure.
    # Expect: non-default primvars exist; default primvars are absent; dirty notice carries new value.
    def test_lightPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="LightPrimvars.testTranslationAndDirtying")

    # What: non-param light attribute changes should dirty primvars only.
    # How: build the scene, then run the C++ test that edits aiShadowDensity.
    # Expect: primvars dirty without light schema dirty.
    def test_nonParamAttrTriggersOnlyPrimvarDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="LightPrimvars.NonParamAttrTriggersOnlyPrimvarDirty")

    # What: param attribute updates should not duplicate primvars notices.
    # How: build the scene, then run the C++ test that edits aiExposure.
    # Expect: exactly one primvars dirty entry for the light.
    def test_intensityUpdateNoDuplicatePrimvarsDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="LightPrimvars.IntensityUpdateNoDuplicatePrimvarsDirty")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
