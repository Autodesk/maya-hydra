import maya.cmds as cmds

import fixturesUtils
import mtohUtils
import unittest

from testUtils import PluginLoaded

class TestMayaUsdUfeItems(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupUsdStage(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, UsdLux

        self.setHdStormRenderer()
        cmds.refresh()
        usdProxyShapeUfePathString = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdStage = mayaUsd.lib.GetPrim(usdProxyShapeUfePathString).GetStage()
        UsdGeom.Cylinder.Define(usdStage, "/USDCylinder")
        UsdLux.RectLight.Define(usdStage, "/USDRectLight")
        cmds.refresh()

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_SkipMayaUsdUfeLights(self):
        self.setupUsdStage()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaUsdUfeItems.skipUsdUfeLights")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
