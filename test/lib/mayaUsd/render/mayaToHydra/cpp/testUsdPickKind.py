# Copyright 2024 Autodesk
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
import usdUtils

import testUtils
from testUtils import PluginLoaded

class TestUsdPickKind(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    PICK_PATH = "|kindHierarchy|kindHierarchyShape,/RootAssembly/ParentGroup"

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testUsdPickKind', 'kindHierarchy.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestUsdPickKind, self).setUp()
        self.loadUsdScene()
        cmds.refresh()

    def test_pickKinds(self):
        with PluginLoaded('mayaHydraCppTests'):
            kinds = ["", "model", "group", "assembly", "component", "subcomponent"]
            selectedItems = [
                # Kind is none: pick the most descendant prim.
                "/ChildAssembly/LeafModel/ImportantSubtree/Cube",
                # Kind is model: subcomponent is not part of the model kind
                # hierarchy, so we iterate up the parent hierarchy twice to
                # reach the component prim.
                "/ChildAssembly/LeafModel",
                # Kind is group: an assembly is a group, so ChildAssembly is
                # picked.
                "/ChildAssembly",
                # Kind is assembly.
                "/ChildAssembly",
                # Kind is component
                "/ChildAssembly/LeafModel",
                # Kind is subcomponent
                "/ChildAssembly/LeafModel/ImportantSubtree"
            ]

            # Read the current USD selection kind.
            kindOptionVar = "mayaUsd_SelectionKind"
            previousKind = cmds.optionVar(q=kindOptionVar)

            for (kind, selectedItem) in zip(kinds, selectedItems):
                cmds.optionVar(sv=(kindOptionVar, kind))
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + selectedItem,
                    f="TestUsdPicking.pickPrim")

            # Restore the USD selection kind back to its original value.
            cmds.optionVar(sv=(kindOptionVar, previousKind))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
