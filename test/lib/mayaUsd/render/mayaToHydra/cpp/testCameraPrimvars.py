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

        self.setTestRenderer()
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

    # What: depth-of-field attributes must reach the camera prim's schema fields.
    # How: build the scene, then run the C++ test that toggles depthOfField and
    # edits focusDistance and fStop.
    # Expect: focusDistance and fStop round-trip while DOF is on, and fStop
    # reports 0 once DOF is turned off.
    def test_depthOfFieldSchemaFieldsRoundTrip(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.DepthOfFieldSchemaFieldsRoundTrip")

    # What: the default exposure must not scale the image.
    # How: build the scene, then run the C++ test that reads linearExposureScale.
    # Expect: linearExposureScale is 1.0.
    def test_exposureDefaultIsNoScale(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ExposureDefaultIsNoScale")

    # What: setting exposure must reach the camera prim and notify Hydra.
    # How: build the scene, then run the C++ test that sets exposure.
    # Expect: linearExposureScale is 2^exposure, and its locator is dirtied.
    def test_exposureSetAttrUpdatesLinearExposureScale(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ExposureSetAttrUpdatesLinearExposureScale")

    # What: keyed exposure must follow the current frame.
    # How: build the scene, then run the C++ test that keys exposure and changes frames.
    # Expect: linearExposureScale matches the keyed value at each frame.
    def test_exposureKeyedFollowsCurrentFrame(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ExposureKeyedFollowsCurrentFrame")

    # What: exposure on one camera must not affect another camera.
    # How: add a second camera, then run the C++ test that sets exposure on the first.
    # Expect: only the first camera's linearExposureScale changes.
    def test_exposureIsPerCamera(self):
        self.setupScene()
        _, secondCameraShape = cmds.camera()
        secondCameraShape = cmds.ls(secondCameraShape, long=True)[0]
        cmds.optionVar(stringValue=("mhSecondCameraShape", secondCameraShape))
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ExposureIsPerCamera")

    # What: a Maya camera must publish the same linearExposureScale as a USD camera
    # with the same exposure.
    # How: add a USD camera with exposure 1, then run the C++ test that sets exposure 1
    # on the Maya camera.
    # Expect: both camera prims report 2.0.
    def test_exposureMatchesUsdCamera(self):
        import mayaUsd.ufe
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom

        self.setupScene()
        usdCameraName = "usdExposureCam"
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.ufe.getStage(stagePath)
        usdCamera = UsdGeom.Camera.Define(stage, "/" + usdCameraName)
        usdCamera.CreateExposureAttr(1.0)
        cmds.optionVar(stringValue=("mhUsdCameraPrimName", usdCameraName))
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CameraPrimvars.ExposureMatchesUsdCamera")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
