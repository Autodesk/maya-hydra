#
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
import unittest

import maya.cmds as cmds
import fixturesUtils
import mtohUtils

# The order mayaHydraTesting -renderThreadStats returns its values in.
RENDER_THREAD_STAT_NAMES = [
    "overlapEnabled",
    "started",
    "contextArenaAcquired",
    "ownsSurface",
    "framesRenderedOffThread",
    "framesComposited",
    "framesRenderedSynchronously",
    "fenceSignalFailures",
    "fenceWaitTimeouts",
    "drains",
    "quiescenceWaits",
    "drainTimeouts",
    "settles",
    "settleRetries",
    "settlesAbandoned",
    "presentsWithoutDepth",
    "presentsSkipped",
    "aovCopyFailures",
    "copyTargetWaits",
    "copyTargetWaitFailures",
    "compositeFenceSignalFailures",
    "compositeReusedLastGood",
    "idleWaitTimeouts",
    "sharedBufferFenceFailures",
    "idleWaitMicros",
    "aovCopyMicros",
    "compositeMicros",
    "compositeHandoffMicros",
    "compositeFenceMicros",
    "compositeBlitMicros",
    "interlockBypassed",
    "aovCopyBypassed",
]


def overlapRequested():
    """Whether MAYAHYDRA_PIPELINE_OVERLAP was set for this process.

    The flag is read once and cached, so a single process can only see one
    value. The test is registered twice, with the flag on and off, and adapts
    its assertions to which registration is running.
    """
    return os.environ.get("MAYAHYDRA_PIPELINE_OVERLAP", "0") not in ("", "0")


class TestRenderThreadOverlap(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def stats(self):
        values = cmds.mayaHydraTesting(renderThreadStats=True) or []
        self.assertEqual(
            len(values), len(RENDER_THREAD_STAT_NAMES),
            "mayaHydraTesting -renderThreadStats returned %d values but the "
            "test expects %d. The C++ append order and "
            "RENDER_THREAD_STAT_NAMES have drifted apart."
            % (len(values), len(RENDER_THREAD_STAT_NAMES)))
        return dict(zip(RENDER_THREAD_STAT_NAMES, values))

    def buildScene(self):
        cmds.file(new=True, force=True)
        self.setHdStormRenderer()
        cube = cmds.polyCube()[0]
        self.setBasicCam()
        cmds.refresh(force=True)
        return cube

    def assertRenderThreadIsLive(self, stats):
        """Fails unless the render thread is genuinely running.

        Every overlap-gated site is guarded by "flag on AND thread started",
        and the fallback renders correctly, so a working viewport says nothing
        about whether the render thread did the work. These are the assertions
        that tell a real overlap run from a silent fallback.
        """
        self.assertEqual(
            stats["overlapEnabled"], 1,
            "MAYAHYDRA_PIPELINE_OVERLAP is set for this process but the "
            "render override never saw it, so nothing under test ran.")
        self.assertEqual(
            stats["started"], 1,
            "Overlap was enabled but the render thread did not start, so "
            "every gated site fell back to synchronous rendering. This is "
            "usually the shared GL context failing to come current. "
            "Stats: %s" % dict(stats))
        self.assertGreater(
            stats["framesRenderedOffThread"], 0,
            "The render thread started but drew nothing, so the frames on "
            "screen all came from the synchronous fallback. Stats: %s"
            % dict(stats))

    def test_inertWhenTheFlagIsOff(self):
        """With the flag off, none of this exists.

        The point of the default path is that it costs nothing, so the thread
        must not merely be unused: it must never have been created.
        """
        if overlapRequested():
            raise unittest.SkipTest("this registration runs with the flag on")

        self.buildScene()
        cmds.refresh(force=True)

        stats = self.stats()
        self.assertEqual(stats["overlapEnabled"], 0)
        self.assertEqual(stats["started"], 0)
        self.assertEqual(stats["framesRenderedOffThread"], 0)
        self.assertEqual(stats["framesComposited"], 0)
        self.assertGreater(
            stats["framesRenderedSynchronously"], 0,
            "With overlap off every frame should be drawn synchronously, but "
            "none were recorded. Stats: %s" % dict(stats))

    def test_framesAreDrawnOffTheMainThread(self):
        """The render thread draws, and what it draws reaches the screen."""
        if not overlapRequested():
            raise unittest.SkipTest("this registration runs with the flag off")

        self.buildScene()

        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        for _ in range(6):
            cmds.refresh(force=True)
        cmds.mayaHydraTesting(drainRenderThread=True)
        cmds.refresh(force=True)

        stats = self.stats()
        self.assertRenderThreadIsLive(stats)

        # Drawing off-thread is only half of it; the frame has to be composited
        # into Maya's framebuffer or the viewport would sit frozen on the
        # first-frame prime while the counters climbed.
        self.assertGreater(
            stats["framesComposited"], 0,
            "Frames were rendered off-thread but none were composited, so "
            "nothing the render thread drew reached the screen. Stats: %s"
            % dict(stats))

    def test_theFirstFrameIsDrawnSynchronously(self):
        """The prime, which is why a render op always leaves an image behind.

        The render thread has produced nothing when the first render op runs,
        so there is nothing to composite; drawing that one frame on the main
        thread is what keeps the viewport from coming up blank. Stats are reset
        before the scene is built, because building it is what re-initializes
        Hydra and so triggers the prime -- resetting afterwards would throw
        away the evidence.
        """
        if not overlapRequested():
            raise unittest.SkipTest("this registration runs with the flag off")

        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        self.buildScene()

        stats = self.stats()
        self.assertGreater(
            stats["framesRenderedSynchronously"], 0,
            "No frame was drawn synchronously after Hydra was re-initialized, "
            "so the first render op had neither a completed frame to composite "
            "nor one of its own. Stats: %s" % dict(stats))

    def test_nothingSilentlyFails(self):
        """The failures that still produce a believable image.

        Each of these leaves the viewport looking plausible, which is why they
        are counted rather than left to a pixel comparison that cannot see
        them. A missing depth AOV, for instance, blits colour perfectly and
        only breaks once something depth-composes over the result.
        """
        if not overlapRequested():
            raise unittest.SkipTest("this registration runs with the flag off")

        self.buildScene()
        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        for _ in range(6):
            cmds.refresh(force=True)
        cmds.mayaHydraTesting(drainRenderThread=True)

        stats = self.stats()
        self.assertRenderThreadIsLive(stats)

        for name in ("fenceSignalFailures",
                     "fenceWaitTimeouts",
                     "drainTimeouts",
                     "presentsWithoutDepth",
                     "presentsSkipped",
                     "aovCopyFailures",
                     "compositeFenceSignalFailures",
                     "sharedBufferFenceFailures"):
            self.assertEqual(
                stats[name], 0,
                "%s is %d, expected 0. Stats: %s"
                % (name, stats[name], dict(stats)))

    def test_aViewportPickDiscardsInFlightWork(self):
        """The pick path, which is the reason the discard exists.

        A pick runs synchronously on the main thread, so a frame rendered
        before it must not be composited after it -- the viewport would show
        the scene without the new selection. cmds.select does not reach
        MtohRenderOverride::select at all, so this uses the viewportPick hook,
        which drives Maya's real selection machinery.
        """
        if not overlapRequested():
            raise unittest.SkipTest("this registration runs with the flag off")

        cube = self.buildScene()
        cmds.select(clear=True)

        width, height = cmds.mayaHydraTesting(renderTargetSize=True)
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)

        cmds.mayaHydraTesting(resetRenderThreadStats=True)

        # Put a frame in flight first, or the discard has nothing to discard
        # and this covers the call rather than the hazard. A forced refresh
        # submits and returns without waiting, so the render thread is still
        # working when the pick lands. Deliberately not draining here: that
        # would leave nothing in flight, which is the vacuous version.
        cmds.refresh(force=True)

        # A box over the middle of the viewport rather than a single pixel,
        # which would be a hostage to rasterisation. setBasicCam aims at the
        # origin and the cube is there, so the centre is a hit.
        half = 40
        cmds.mayaHydraTesting(viewportPick=(
            width // 2 - half, height // 2 - half,
            width // 2 + half, height // 2 + half))

        selection = cmds.ls(selection=True) or []
        self.assertIn(
            cube, selection,
            "The pick over the centre of a %dx%d viewport did not select the "
            "cube (selection is %r). Either the marquee missed -- note "
            "selectFromScreen takes viewport coordinates with the origin at "
            "the bottom left -- or the pick found no hit, which is a pick "
            "failure rather than a threading one." % (width, height, selection))

        stats = self.stats()
        self.assertRenderThreadIsLive(stats)

        # The actual point. If this is zero the pick never reached
        # MtohRenderOverride::select, and the discard is still untested no
        # matter what the assertions above say.
        self.assertGreater(
            stats["drains"], 0,
            "The pick selected the cube, but no discard was recorded, so it "
            "did not reach MtohRenderOverride::select and this test does not "
            "cover the pick path. Maya may have satisfied the selection "
            "through OGS instead. Stats: %s" % dict(stats))

        # DiscardInFlight waits out a render already running before clearing
        # the handoff. If that wait gives up, the render publishes afterwards
        # and its pre-pick frame is composited after the pick, which is exactly
        # the stale frame the discard exists to prevent.
        self.assertEqual(
            stats["drainTimeouts"], 0,
            "The pick's discard timed out with the render thread still busy, "
            "so a frame rendered before the pick can still publish and be "
            "composited after it. Stats: %s" % dict(stats))


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
