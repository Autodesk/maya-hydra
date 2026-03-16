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


class TestCameraPrimvars(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def setupScene(self):
        cmds.file(new=True, force=True)
        # Create a camera and get its shape
        camera_transform, camera_shape = cmds.camera()
        camera_shape = cmds.ls(camera_shape, long=True)[0]

        # Add a custom attribute (non-param) for NonParamAttrTriggersOnlyPrimvarDirty test
        cmds.addAttr(camera_shape, longName="testCustomAttr", attributeType="float", defaultValue=0.0)

        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhCameraShape", camera_shape))
        cmds.refresh()

    def test_nonParamAttrTriggersOnlyPrimvarDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.NonParamAttrTriggersOnlyPrimvarDirty")

    def test_focalLengthUpdateNoDuplicatePrimvarsDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.FocalLengthUpdateNoDuplicatePrimvarsDirty")

    def test_paramAttributesMatchGetLogic(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ParamAttributesMatchGetLogic")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
