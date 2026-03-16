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

    # Create a minimal camera scene for the C++ primvars tests.
    # Adds a non-param custom attribute and stores the camera shape for C++ lookup.
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

    # What: non-param custom attribute changes should dirty primvars only.
    # How: build the scene, then run the C++ test that edits testCustomAttr.
    # Expect: a primvars-only dirty notice (no camera schema).
    def test_nonParamAttrTriggersOnlyPrimvarDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.NonParamAttrTriggersOnlyPrimvarDirty")

    # What: focalLength updates should not produce duplicate primvars notices.
    # How: build the scene, then run the C++ test that edits focalLength.
    # Expect: exactly one primvars dirty entry for the camera.
    def test_focalLengthUpdateNoDuplicatePrimvarsDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.FocalLengthUpdateNoDuplicatePrimvarsDirty")

    # What: param attribute list must match the attributes read by GetCameraParamValue.
    # How: build the scene, then run the C++ consistency test.
    # Expect: all attributes used by GetCameraParamValue are in the param list.
    def test_paramAttributesMatchGetLogic(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ParamAttributesMatchGetLogic")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
