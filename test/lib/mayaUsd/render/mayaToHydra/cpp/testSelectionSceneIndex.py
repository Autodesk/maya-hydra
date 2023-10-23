import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded

class TestSelectionSceneIndex(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupScene(self):
        self.setHdStormRenderer()
        cmds.polySphere(name="aSphere")
        cmds.refresh()

    def test_selectionSceneIndex(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="FlowViewport.selectionSceneIndex")
            cmds.mayaHydraCppTest(f="FlowViewport.selectionSceneIndexDirty")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
