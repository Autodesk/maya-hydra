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
import os

import maya.cmds as cmds
import fixturesUtils
import mtohUtils


class TestFramePublication(mtohUtils.MayaHydraBaseTestCase):
    """Frame publication boundary.

    MAYAHYDRA_SNAPSHOT is read once per process, so this file is registered
    twice with opposite values of it: once to check the boundary is inert by
    default, and once to check it advances and publishes a frame per flush.
    """

    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    ENABLED, CURRENT_FRAME, PUBLISHED_FRAME, PUBLISH_COUNT = range(4)

    def snapshotEnabled(self):
        return os.environ.get('MAYAHYDRA_SNAPSHOT', '0') not in ('0', '', 'false', 'False')

    def stats(self):
        return cmds.mayaHydraTesting(snapshotStats=True)

    def setupScene(self):
        cmds.file(new=True, force=True)
        cmds.polySphere()
        self.setHdStormRenderer()
        cmds.refresh()

    # What: the boundary reports the state the env flag selects.
    # How: read the stats after a refresh.
    # Expect: disabled and stuck at frame 0 when off; enabled and past frame 0 when on.
    def test_enablement(self):
        self.setupScene()
        enabled, current, published, publishCount = self.stats()

        if not self.snapshotEnabled():
            self.assertEqual(enabled, 0)
            # Nothing is named or published, so a consumer sees "no frame yet".
            self.assertEqual(current, 0)
            self.assertEqual(published, 0)
            self.assertEqual(publishCount, 0)
        else:
            self.assertEqual(enabled, 1)
            self.assertGreater(current, 0)
            self.assertGreater(publishCount, 0)

    # What: each flush names a new frame and publishes it.
    # How: refresh repeatedly and compare the frame ids across refreshes.
    # Expect: the current frame id advances, and the published id catches up to it.
    def test_framesAdvanceAndPublish(self):
        self.setupScene()
        if not self.snapshotEnabled():
            # With the flag off there is nothing to advance; test_enablement
            # already covers that.
            self.assertEqual(self.stats()[self.ENABLED], 0)
            return

        before = self.stats()
        # A translated change, so the flush has work to do rather than only
        # proving that the counter increments on an idle redraw.
        cmds.move(1, 0, 0, relative=True)
        cmds.refresh()
        after = self.stats()

        self.assertGreater(after[self.CURRENT_FRAME], before[self.CURRENT_FRAME])
        self.assertGreater(after[self.PUBLISH_COUNT], before[self.PUBLISH_COUNT])

        # The barrier runs after the flush, so by the time the refresh has
        # returned the frame it named is published, not one frame behind.
        self.assertEqual(after[self.PUBLISHED_FRAME], after[self.CURRENT_FRAME])

    # What: a published frame id is never ahead of the frame being produced.
    # How: sample the pair across several refreshes.
    # Expect: published <= current at every sample.
    def test_publishedNeverLeadsCurrent(self):
        self.setupScene()
        if not self.snapshotEnabled():
            return

        for i in range(4):
            cmds.move(1, 0, 0, relative=True)
            cmds.refresh()
            stats = self.stats()
            self.assertLessEqual(
                stats[self.PUBLISHED_FRAME],
                stats[self.CURRENT_FRAME],
                'published frame id ran ahead of the current frame id on iteration %d' % i)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
