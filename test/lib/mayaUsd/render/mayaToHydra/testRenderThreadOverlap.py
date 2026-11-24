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
# Runs only with MAYAHYDRA_PIPELINE_OVERLAP=1; see the _overlap registration.
import maya.cmds as cmds
import fixturesUtils
import mtohUtils

# The order mayaHydraTesting -renderThreadStats returns its values in.
RENDER_THREAD_STAT_NAMES = [
    "overlapEnabled",
    "started",
    "supported",
    "framesRenderedOffThread",
    "framesComposited",
    "framesRenderedSynchronously",
    "ownFramesComposited",
    "discards",
    "idleWaitTimeouts",
    "fenceSignalFailures",
    "unfencedFramesSkipped",
    "quiescenceFenceTimeouts",
    "quiescenceWaitTimeouts",
    "presentsWithoutDepth",
    "presentsSkipped",
    "aovCopyFailures",
    "compositeFenceSignalFailures",
    "sharedBufferFenceFailures",
    "primsCaptured",
    "mayaReadsOnRenderThread",
    "captureHolds",
    "captureHoldMicros",
    "captureReleaseMicros",
]

# Each of these still leaves a believable image on screen, so they are counted
# rather than left to a pixel comparison that cannot see them.
FAILURE_STAT_NAMES = [
    "fenceSignalFailures",
    "unfencedFramesSkipped",
    "quiescenceFenceTimeouts",
    "presentsWithoutDepth",
    "presentsSkipped",
    "aovCopyFailures",
    "compositeFenceSignalFailures",
    "sharedBufferFenceFailures",
    # A read of live Maya data the capture scene index should have served.
    "mayaReadsOnRenderThread",
]


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

    def buildScene(self, useTestRenderer=False):
        cmds.file(new=True, force=True)
        if useTestRenderer:
            self.setTestRenderer()
        else:
            self.setHdStormRenderer()
        cube = cmds.polyCube()[0]
        self.setBasicCam()
        cmds.refresh(force=True)
        return cube

    def assertRenderThreadIsLive(self, stats):
        """Fails unless the render thread is genuinely running.

        The synchronous fallback renders correctly, so a working viewport says
        nothing about whether the render thread did the work.
        """
        self.assertEqual(
            stats["overlapEnabled"], 1,
            "MAYAHYDRA_PIPELINE_OVERLAP is set for this process but the "
            "render override never saw it, so nothing under test ran.")
        if not stats["supported"]:
            # Such as Metal against Pixar's USD, Vulkan without threaded
            # submission, or OpenGL on Linux: MayaHydra draws on the main thread
            # by design.
            self.skipTest("The render thread cannot run on the current Hgi here.")
        self.assertEqual(
            stats["started"], 1,
            "Overlap was enabled but the render thread did not start, so "
            "every frame fell back to synchronous rendering. This is "
            "usually the shared GL context failing to come current. "
            "Stats: %s" % dict(stats))
        self.assertGreater(
            stats["framesRenderedOffThread"], 0,
            "The render thread started but drew nothing, so the frames on "
            "screen all came from the synchronous fallback. Stats: %s"
            % dict(stats))

    def test_overlapRuns(self):
        """The first frame leaves an image behind, the rest are drawn on the
        render thread, and what it draws reaches the screen without any of
        the failures that would still leave a believable image."""
        # Reset before building, because building the scene re-initializes
        # Hydra and so triggers the first frame.
        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        self.buildScene()
        for _ in range(6):
            cmds.refresh(force=True)
        cmds.mayaHydraTesting(drainRenderThread=True)

        stats = self.stats()
        self.assertRenderThreadIsLive(stats)
        # Under OpenGL the first frame is drawn on the render thread and
        # composited as its own frame instead.
        self.assertGreater(
            stats["framesRenderedSynchronously"] + stats["ownFramesComposited"], 0,
            "The first frame after Hydra was re-initialized was neither drawn "
            "synchronously nor composited as its own frame, so the first "
            "render op had no image to leave behind. Stats: %s" % dict(stats))
        self.assertGreater(
            stats["framesComposited"], 0,
            "Frames were rendered off-thread but none were composited, so "
            "nothing the render thread drew reached the screen. Stats: %s"
            % dict(stats))
        for name in FAILURE_STAT_NAMES:
            self.assertEqual(
                stats[name], 0,
                "%s is %d, expected 0. Stats: %s"
                % (name, stats[name], dict(stats)))

    def test_renderThreadReadsOnlyCapturedData(self):
        """Lights, cameras, materials and DAG meshes read Maya when Hydra pulls
        their data. With the render thread running, those pulls must reach
        the capture scene index's copy, not Maya."""
        cube = self.buildScene()
        cmds.setAttr("defaultRenderGlobals.enableDefaultLight", 0)
        pointLight = cmds.pointLight(intensity=1.0)
        spotLight = cmds.spotLight(intensity=1.0)
        cmds.xform(cmds.listRelatives(spotLight, parent=True)[0], translation=(0, 4, 0))
        material = cmds.shadingNode("lambert", asShader=True)
        cmds.select(cube)
        cmds.hyperShade(assign=material)
        cmds.select(clear=True)
        camera = cmds.listRelatives(cmds.lookThru(query=True), shapes=True) or [
            cmds.lookThru(query=True)]
        cmds.refresh(force=True)

        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        for i in range(6):
            # Each edit dirties a prim whose data source reads Maya when
            # pulled, and each refresh submits a frame that syncs it.
            cmds.setAttr(pointLight + ".intensity", 1.0 + 0.1 * i)
            cmds.setAttr(spotLight + ".coneAngle", 30 + i)
            cmds.setAttr(material + ".color", 0.2 + 0.1 * i, 0.3, 0.4, type="double3")
            cmds.setAttr(camera[0] + ".focalLength", 35 + i)
            cmds.move(0, 0.05 * i, 0, cube + ".vtx[0]", relative=True)
            cmds.refresh(force=True)
        cmds.mayaHydraTesting(drainRenderThread=True)

        stats = self.stats()
        self.assertRenderThreadIsLive(stats)
        self.assertGreater(
            stats["primsCaptured"], 0,
            "Frames were rendered off-thread but the capture scene index "
            "copied nothing, so it is not in the render index's chain or was "
            "never activated, and the render thread read the live scene. "
            "Stats: %s" % dict(stats))
        self.assertEqual(
            stats["mayaReadsOnRenderThread"], 0,
            "MayaHydra read Maya %d times while a frame rendered off the main "
            "thread. Stats: %s" % (stats["mayaReadsOnRenderThread"], dict(stats)))

    def assertJumpLeavesNoGhost(self, what, fromFrame=1, toFrame=10, beforeJump=None):
        """Jump between frames 1 and 10, where `what` is at x=-2.5 and x=2.5.

        Maya follows a time change outside playback with exactly one redraw,
        which has to composite its own frame rather than the previous one, and
        nothing else. Reads Maya's colour buffer straight after that redraw,
        rather than through a playblast, which would refresh again and hide
        the state a user sees. `beforeJump` runs after the view has settled on
        `fromFrame` and must leave the time there.
        """
        import ctypes
        import maya.api.OpenMaya as om
        import maya.api.OpenMayaUI as omui

        xAtFrame = {1: -2.5, 10: 2.5}

        cmds.currentTime(fromFrame)
        cmds.refresh(force=True)
        cmds.mayaHydraTesting(drainRenderThread=True)
        cmds.refresh(force=True)

        view = omui.M3dView.active3dView()

        def pixelAt(image, x, y):
            width, height = image.getSize()
            # MImage.pixels() is the address of width * height RGBA bytes.
            data = (ctypes.c_ubyte * (width * height * 4)).from_address(image.pixels())
            offset = (y * width + x) * 4
            return tuple(data[offset:offset + 3])

        def project(xWorld):
            # Above the ground plane, off the grid's axis line.
            x, y, _ = view.worldToView(om.MPoint(xWorld, 0.3, 0.3))
            return x, y

        oldX, oldY = project(xAtFrame[fromFrame])
        newX, newY = project(xAtFrame[toFrame])

        def readViewport():
            image = om.MImage()
            view.readColorBuffer(image, True)
            return pixelAt(image, oldX, oldY), pixelAt(image, newX, newY)

        # What the starting frame looks like at both positions once settled.
        frame1Old, frame1New = readViewport()

        if beforeJump:
            beforeJump()

        cmds.mayaHydraTesting(resetRenderThreadStats=True)
        cmds.currentTime(toFrame)
        atOld, atNew = readViewport()
        stats = self.stats()

        # And the target frame, settled, as the reference for the single refresh.
        cmds.mayaHydraTesting(drainRenderThread=True)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        settledOld, settledNew = readViewport()

        self.assertRenderThreadIsLive(stats)
        self.assertEqual(
            stats["ownFramesComposited"], 1,
            "The time changed while playback was stopped, but the render op "
            "that applied it did not composite its own frame. Stats: %s"
            % dict(stats))

        def differs(a, b):
            return max(abs(int(p) - int(q)) for p, q in zip(a, b)) > 8

        self.assertTrue(
            differs(frame1Old, settledOld),
            "The probe at %s does not see the %s move away (frame %d %s, "
            "frame %d %s), so this test cannot detect a ghost."
            % ((oldX, oldY), what, fromFrame, frame1Old, toFrame, settledOld))
        self.assertFalse(
            differs(atNew, settledNew),
            "After one refresh the frame-%d position %s shows %s, but the "
            "settled frame %d shows %s."
            % (toFrame, (newX, newY), atNew, toFrame, settledNew))
        self.assertFalse(
            differs(atOld, settledOld),
            "After one refresh the frame-%d position %s shows %s, but the "
            "settled frame %d shows %s: the %s is drawn at its old "
            "position too."
            % (fromFrame, (oldX, oldY), atOld, toFrame, settledOld, what))

    def test_aTimeChangeWhileStoppedLeavesNoGhost(self):
        """A transform-animated cube must appear only at the new time."""
        cube = self.buildScene(useTestRenderer=True)
        cmds.playbackOptions(minTime=1, maxTime=10)
        cmds.setKeyframe(cube, attribute="translateX", time=1, value=-2.5)
        cmds.setKeyframe(cube, attribute="translateX", time=10, value=2.5)
        self.assertJumpLeavesNoGhost("cube")

    def test_aJumpRightAfterPlaybackLeavesNoGhost(self):
        """Play, stop, then jump: the last frame playback submitted must not
        be composited under the jump's own frame."""
        cube = self.buildScene(useTestRenderer=True)
        cmds.playbackOptions(minTime=1, maxTime=10, loop="once")
        cmds.setKeyframe(cube, attribute="translateX", time=1, value=-2.5)
        cmds.setKeyframe(cube, attribute="translateX", time=10, value=2.5)

        def playThenStop():
            cmds.currentTime(1)
            cmds.play(forward=True, wait=True)
            self.assertEqual(cmds.currentTime(query=True), 10)
            cmds.refresh(force=True)

        self.assertJumpLeavesNoGhost(
            "cube", fromFrame=10, toFrame=1, beforeJump=playThenStop)

    def test_aSkinnedMeshJumpUnderCachedPlaybackLeavesNoGhost(self):
        """The same for a skinned mesh, deformed on the GPU and cached.

        Each of these changes how the deformed points reach Hydra: the GPU
        deformer shares them as device buffers, and cached playback serves
        them from the cache instead of evaluating.
        """
        cube = self.buildScene(useTestRenderer=True)
        cmds.delete(cube)
        cmds.select(clear=True)
        root = cmds.joint(position=(0, -0.5, 0))
        cmds.joint(position=(0, 0.5, 0))
        cylinder = cmds.polyCylinder(radius=0.6, height=1.0, subdivisionsHeight=4)[0]
        cmds.skinCluster(root, cylinder, toSelectedBones=True)
        cmds.playbackOptions(minTime=1, maxTime=10)
        cmds.setKeyframe(root, attribute="translateX", time=1, value=-2.5)
        cmds.setKeyframe(root, attribute="translateX", time=10, value=2.5)

        previousMode = cmds.evaluationManager(query=True, mode=True)[0]
        previousDeformer = cmds.evaluator(name="deformer", query=True, enable=True)
        previousCache = cmds.evaluator(name="cache", query=True, enable=True)
        previousFillMode = cmds.cacheEvaluator(query=True, cacheFillMode=True)
        self.addCleanup(cmds.evaluationManager, mode=previousMode)
        self.addCleanup(cmds.evaluator, name="deformer", enable=bool(previousDeformer))
        self.addCleanup(cmds.evaluator, name="cache", enable=bool(previousCache))
        self.addCleanup(cmds.cacheEvaluator, cacheFillMode=previousFillMode)
        cmds.evaluationManager(mode="parallel")
        cmds.evaluator(name="deformer", enable=True)
        cmds.evaluator(name="cache", enable=True)
        cmds.cacheEvaluator(cacheFillMode="syncOnly")
        cmds.select(clear=True)
        # Fill the cache by evaluating every frame once.
        for frame in range(1, 11):
            cmds.currentTime(frame)
        self.assertJumpLeavesNoGhost("skinned cylinder")

    def test_aViewportPickDiscardsInFlightWork(self):
        """A pick made while a frame is in flight selects what is under it and
        discards the frame in flight.

        That discard is what keeps a frame rendered before the pick from being
        composited after it, showing the scene without the new selection. This
        checks that the pick reaches it, not the pixels.

        cmds.select does not reach MtohRenderOverride::select at all, so this
        uses the viewportPick hook, which drives Maya's real selection
        machinery.
        """
        cube = self.buildScene()
        cmds.select(clear=True)

        width, height = cmds.mayaHydraTesting(renderTargetSize=True)
        self.assertGreater(width, 0)
        self.assertGreater(height, 0)

        cmds.mayaHydraTesting(resetRenderThreadStats=True)

        # Put a frame in flight first, or the discard has nothing to discard.
        # A forced refresh submits and returns without waiting, so the render
        # thread is still working when the pick lands.
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
        self.assertGreater(
            stats["discards"], 0,
            "The pick selected the cube, but no discard was recorded, so it "
            "did not reach MtohRenderOverride::select and this test does not "
            "cover the pick path. Maya may have satisfied the selection "
            "through OGS instead. Stats: %s" % dict(stats))


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
