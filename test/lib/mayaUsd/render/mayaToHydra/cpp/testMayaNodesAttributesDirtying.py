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


class TestMayaNodesAttributesDirtying(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']

    # Create a minimal scene with mesh/camera/light/material and a shared extension attribute.
    # This scene is used by the C++ tests to assert primvar dirtying behavior.
    def setupScene(self):
        cmds.file(new=True, force=True)
        mesh_transform, mesh_shape = cmds.polyCube(name="dirtyMesh")
        cam_transform, cam_shape = cmds.camera(name="dirtyCamera")
        light_shape = cmds.pointLight(name="dirtyLight")

        material = cmds.shadingNode("lambert", asShader=True, name="dirtyMaterial")
        material_sg = cmds.sets(
            renderable=True,
            noSurfaceShader=True,
            empty=True,
            name="dirtyMaterialSG")
        cmds.connectAttr(material + ".outColor", material_sg + ".surfaceShader", force=True)
        cmds.sets(mesh_transform, edit=True, forceElement=material_sg)

        mesh_transform = cmds.ls(mesh_transform, long=True)[0]
        mesh_shape = cmds.listRelatives(mesh_transform, shapes=True, fullPath=True)[0]
        cam_shape = cmds.listRelatives(cam_transform, shapes=True, fullPath=True)[0]
        light_shape = cmds.ls(light_shape, long=True)[0]
        material_sg = cmds.ls(material_sg, long=True)[0]

        def _ensure_ext_dirty(node):
            if not cmds.attributeQuery("extDirty", node=node, exists=True):
                cmds.addAttr(node, longName="extDirty", attributeType="double", defaultValue=0.0)
            try:
                cmds.setAttr(node + ".extDirty", edit=True, keyable=True)
            except RuntimeError:
                cmds.setAttr(node + ".extDirty", edit=True, channelBox=True)

        for node in (mesh_shape, cam_shape, light_shape, material_sg):
            _ensure_ext_dirty(node)

        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhDirtyMeshShape", mesh_shape))
        cmds.optionVar(stringValue=("mhDirtyCameraShape", cam_shape))
        cmds.optionVar(stringValue=("mhDirtyLightShape", light_shape))
        cmds.optionVar(stringValue=("mhDirtyMaterialSg", material_sg))

        cmds.refresh()

    # What: extension attribute changes on mesh/camera/light/material should dirty primvars.
    # How: setup the scene, then run the C++ test that drives extDirty and checks notices.
    # Expect: each node produces a primvar dirty entry containing the new value.
    def test_extDirtyPrimvars(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaNodesAttributesDirtying.testDirtyPrimvars")

    # What: camera aiUScale (Arnold extension) should dirty primvars only.
    # How: setup the scene, run the C++ test that sets aiUScale and inspects notices.
    # Expect: exactly one primvar notice for the camera prim.
    def test_cameraAiUScalePrimvarsNotices(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaNodesAttributesDirtying.testCameraAiUScaleDirtying")

    # What: mesh aiUseSubFrame should dirty primvars + extComputationPrimvars.
    # How: setup the scene, run the C++ test that sets aiUseSubFrame and inspects notices.
    # Expect: exactly one notice containing both primvars and extComputationPrimvars.
    def test_meshAiUseSubFramePrimvarsNotices(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaNodesAttributesDirtying.testMeshAiUseSubFrameDirtying")

    # What: light aiShadowDensity (Arnold extension) should dirty primvars only.
    # How: setup the scene, run the C++ test that sets aiShadowDensity and inspects notices.
    # Expect: exactly one primvar notice for the light prim.
    def test_lightAiShadowDensityPrimvarsNotices(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaNodesAttributesDirtying.testLightAiShadowDensityDirtying")

    # What: built-in light params (color, intensity) should dirty light params.
    # How: setup the scene, run the C++ test that changes intensity and color.
    # Expect: exactly one notice per change; light schema with USD's extra locators
    #         (primvars/visibility/collections) and no extComputationPrimvars.
    def test_lightParamsDirtying(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaNodesAttributesDirtying.testLightParamDirtying")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
