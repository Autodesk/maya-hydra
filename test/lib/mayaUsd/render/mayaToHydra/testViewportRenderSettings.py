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
# Where a viewport render setting comes from. The settings are attributes of
# defaultRenderGlobals: they are saved with the scene, and an edit is also
# stored as the user's default. A scene that has the setting wins over the
# user's default, and a scene that does not (a new scene) starts from it.
#
# Checked on the refine level, a setting shared by all renderers.
import maya.cmds as cmds

import fixturesUtils
import mayaUtils
import mtohUtils


class TestViewportRenderSettings(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    REFINE_LEVEL = "mayaHydraRefinementLevel"
    REFINE_LEVEL_PLUG = "defaultRenderGlobals." + REFINE_LEVEL

    # A cube, saved with the refine level at 2.
    SCENE = ("testViewportRenderSettings", "cubeRefineLevel2.ma")

    def forgetUserDefault(self):
        # The user's default is an optionVar, which outlives the scene and, once Maya
        # saves its preferences, the session.
        cmds.optionVar(remove=self.REFINE_LEVEL)

    def setUp(self):
        self.forgetUserDefault()
        super(TestViewportRenderSettings, self).setUp()

    def tearDown(self):
        self.forgetUserDefault()
        super(TestViewportRenderSettings, self).tearDown()

    def refineLevel(self):
        # The attribute is created when the renderer initializes, on the first draw
        # of a scene that does not have it.
        self.setTestRenderer()
        cmds.refresh(force=True)
        return cmds.getAttr(self.REFINE_LEVEL_PLUG)

    def setRefineLevel(self, level):
        # As the Attribute Editor and the option box do: set the attribute, then
        # report it, which also stores it as the user's default.
        cmds.setAttr(self.REFINE_LEVEL_PLUG, level)
        cmds.mayaHydra(updateRenderGlobals=self.REFINE_LEVEL)

    def test_refineLevelFollowsSceneThenUserDefault(self):
        # A new scene, with no user default: the built-in default.
        self.assertEqual(self.refineLevel(), 0)

        # The saved scene's level is used.
        mayaUtils.openTestScene(*self.SCENE)
        self.assertEqual(self.refineLevel(), 2)

        # A scene's level is not an edit: it does not follow into a new scene.
        mayaUtils.openNewScene()
        self.assertEqual(self.refineLevel(), 0)
        mayaUtils.openTestScene(*self.SCENE)
        self.assertEqual(self.refineLevel(), 2)

        # An edit becomes the user's default, which a new scene starts from.
        self.setRefineLevel(3)
        self.assertEqual(self.refineLevel(), 3)
        mayaUtils.openNewScene()
        self.assertEqual(self.refineLevel(), 3)

        # The saved scene still wins over the user's default.
        mayaUtils.openTestScene(*self.SCENE)
        self.assertEqual(self.refineLevel(), 2)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
