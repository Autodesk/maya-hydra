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


class TestDataSourceNames(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Build a minimal scene exercising the light and camera leaf params that
    # HYDRA-2028 newly advertises through GetNames(). The persp camera always
    # exists; we add a directional light with a known color so the C++ side can
    # check both that the params are advertised and that Get() serves them.
    def setupScene(self):
        cmds.file(new=True, force=True)

        # Directional light -> translated to a simpleLight prim under Storm.
        # Default shape name is "directionalLightShape1" (matched on the C++ side).
        cmds.directionalLight()
        light_shape = cmds.ls(type='directionalLight', long=True)[0]
        # Known non-default color so the C++ value check is deterministic.
        cmds.setAttr(light_shape + ".color", 0.25, 0.5, 0.75, type="double3")

        # Enable depth of field on the persp camera so fStop/focusDistance are
        # meaningful (fStop is reported as 0 when DOF is disabled).
        cmds.setAttr("perspShape.depthOfField", 1)
        cmds.setAttr("perspShape.fStop", 5.6)
        cmds.setAttr("perspShape.focusDistance", 12.0)

        self.setTestRenderer()
        cmds.refresh()

    # What: light container advertises and serves its leaf params.
    # How: build the scene, then run the C++ traversal/value test.
    # Expect: every new leaf token is advertised and Get() serves a typed value.
    def test_lightContainerAdvertisesAndServesLeafParams(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="DataSourceNames.LightContainerAdvertisesAndServesLeafParams")

    # What: camera container advertises and serves the additional params.
    # How: build the scene, then run the C++ traversal/value test.
    # Expect: new names advertised; shutterOpen/shutterClose served as doubles.
    def test_cameraContainerAdvertisesAndServesParams(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="DataSourceNames.CameraContainerAdvertisesAndServesParams")

    # What: the capture's probe reports a name served but not listed.
    # How: run the C++ test on a hand-made container.
    # Expect: the unlisted name is reported, an empty value is not.
    def test_captureFindsUnlistedNames(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="DataSourceNames.CaptureFindsUnlistedNames")

    # What: no MayaHydra prim serves a name it does not list.
    # How: add a mesh with a material and more light types to the scene, then probe
    #      every prim of MayaHydraSceneIndex.
    # Expect: nothing reported.
    def test_mayaHydraPrimsListEveryName(self):
        self.setupScene()
        cube = cmds.polyCube()[0]
        material = cmds.shadingNode("lambert", asShader=True)
        cmds.select(cube)
        cmds.hyperShade(assign=material)
        cmds.select(clear=True)
        cmds.pointLight()
        cmds.spotLight()
        cmds.shadingNode("areaLight", asLight=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="DataSourceNames.MayaHydraPrimsListEveryName")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
