import maya.cmds as cmds

import fixturesUtils
import mtohUtils

import unittest

class TestStage(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_newFileWithUsdStage(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib

        self.setHdStormRenderer()
        # create an empty USD Stage
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        
        # create a new Maya scene.
        # See https://jira.autodesk.com/browse/HYDRA-496
        # prior to this fix, Maya would crash
        cmds.file( f=True, new=True )     
        
        # sanity check
        self.setHdStormRenderer()
        # start with a maya sphere
        sphere = cmds.polySphere()
        cmds.refresh()
        rprims = self.getIndex()
        # we expect non-zero rprims
        rprimsCount = len(rprims)
        self.assertGreater(rprimsCount, 0)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
