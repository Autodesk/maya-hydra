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


class TestMaterialDirtying(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Create a sphere with a lambert material carrying a dynamic attribute. The names are used by
    # the C++ tests.
    def setupScene(self):
        cmds.file(new=True, force=True)
        sphere = cmds.polySphere(name="dirtySphere")[0]
        material = cmds.shadingNode("lambert", asShader=True, name="dirtyMaterial")
        cmds.addAttr(material, longName="extDirty", attributeType="double", defaultValue=0.0)
        material_sg = cmds.sets(
            renderable=True,
            noSurfaceShader=True,
            empty=True,
            name="dirtyMaterialSG")
        cmds.connectAttr(material + ".outColor", material_sg + ".surfaceShader", force=True)
        cmds.sets(sphere, edit=True, forceElement=material_sg)

        self.setHdStormRenderer()
        cmds.refresh()

    # What: a node-dirty on the shader color, without an attribute-changed message, should
    #       dirty the material (Attribute Editor slider drag).
    # How: setup the scene, then run the C++ test that changes a node driving the color and checks notices.
    # Expect: the shading group material prim gets a material-locator dirty.
    def test_shaderNodeDirtyDirtiesMaterial(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MaterialDirtying.testShaderNodeDirtyDirtiesMaterial")

    # What: setting the shader color should update the color in the Hydra material network.
    # How: setup the scene, then run the C++ test that sets the color to red and reads it back.
    # Expect: the material network diffuseColor is red.
    def test_shaderColorUpdatesMaterialNetwork(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MaterialDirtying.testShaderColorUpdatesMaterialNetwork")

    # What: a node-dirty on a dynamic attribute of the shader, without an attribute-changed
    #       message, should dirty primvars only (Attribute Editor slider drag).
    # How: setup the scene, then run the C++ test that dgdirties the attribute and checks notices.
    # Expect: the shading group material prim gets a primvars dirty and no material dirty.
    def test_shaderDynamicAttrNodeDirtyDirtiesPrimvarsOnly(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="MaterialDirtying.testShaderDynamicAttrNodeDirtyDirtiesPrimvarsOnly")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
