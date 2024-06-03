import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import mayaUtils
import testUtils
from testUtils import PluginLoaded

class TestPointInstancingWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testPointInstancingWireframeHighlight', 'nestedAndComposedPointInstancers.usda')
        usdUtils.createStageFromFile(usdScenePath)
        #self.setHdStormRenderer()
        #cmds.refresh()

    def setUp(self):
        super(TestPointInstancingWireframeHighlight, self).setUp()
        self.loadUsdScene()
        #cmds.refresh()

    def test_PointInstancerSelection(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="PointInstancingWireframeHighlight.pointInstancer")
            
    def test_InstanceSelection(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="PointInstancingWireframeHighlight.instance")

    def test_PrototypeSelection(self):
        pass

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
