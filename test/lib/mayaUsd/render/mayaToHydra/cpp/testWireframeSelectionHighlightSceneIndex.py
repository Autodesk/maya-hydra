import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import mayaUtils
from testUtils import PluginLoaded

class TestWireframeSelectionHighlightSceneIndex(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_wireframeSelectionHighlightSceneIndex(self):
        self.setHdStormRenderer()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.refresh()
            cmds.mayaHydraCppTest(
                f="FlowViewport.wireframeSelectionHighlightSceneIndex")

            testFile = mayaUtils.openTestScene(
                "testWireframeSelectionHighlight",
                "testSelectionHighlightHierarchy.ma")
            cmds.refresh()
            
            cmds.mayaHydraCppTest(
                f="FlowViewport.wireframeSelectionHighlightSceneIndexDirty")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
