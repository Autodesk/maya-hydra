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
import ctypes

import maya.api.OpenMaya as om
import maya.api.OpenMayaUI as omui
import maya.cmds as cmds
import fixturesUtils
import mtohUtils


class TestSkinnedMeshCachedPlayback(mtohUtils.MayaHydraBaseTestCase):
    """A skinned mesh, deformed on the GPU and cached, drawn on the main thread.

    The same scene as testRenderThreadOverlap's skinned mesh test, without the
    render thread, so a failure there can be told apart from one here.
    """
    _file = __file__

    def readProbes(self, view, probes):
        image = om.MImage()
        view.readColorBuffer(image, True)
        width, height = image.getSize()
        # MImage.pixels() is the address of width * height RGBA bytes.
        data = (ctypes.c_ubyte * (width * height * 4)).from_address(image.pixels())
        return [tuple(data[(y * width + x) * 4:(y * width + x) * 4 + 3]) for x, y in probes]

    def describeMayaPlacement(self, node, frames):
        def rounded(values):
            return tuple(round(v, 2) for v in values)

        parts = []
        for frame in frames:
            bboxMin = cmds.getAttr(node + ".boundingBoxMin", time=frame)[0]
            bboxMax = cmds.getAttr(node + ".boundingBoxMax", time=frame)[0]
            parts.append("frame %d %s-%s" % (frame, rounded(bboxMin), rounded(bboxMax)))
        shapes = cmds.listRelatives(node, shapes=True, noIntermediate=True) or [node]
        current = cmds.exactWorldBoundingBox(shapes[0])
        parts.append("now (frame %g) %s-%s" % (
            cmds.currentTime(query=True), rounded(current[:3]), rounded(current[3:])))
        return "Maya has the %s at %s." % (node, ", ".join(parts))

    # What: a skinned mesh under the deformer and cache evaluators is drawn where
    #       its skeleton puts it, after a jump in time.
    # How: a cylinder skinned to a joint keyed from x=-2.5 at frame 1 to x=2.5 at
    #      frame 10, the cache filled, then the viewport read at both positions
    #      at frame 1 and at frame 10.
    # Expect: the cylinder leaves x=-2.5 and arrives at x=2.5.
    def test_skinnedMeshFollowsTime(self):
        cmds.file(new=True, force=True)
        self.setTestRenderer()
        self.setBasicCam()
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
        for frame in range(1, 11):
            cmds.currentTime(frame)

        view = omui.M3dView.active3dView()

        def project(xWorld):
            x, y, _ = view.worldToView(om.MPoint(xWorld, 0.3, 0.3))
            return x, y

        probes = [project(-2.5), project(2.5)]

        cmds.currentTime(1)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        frame1Old, frame1New = self.readProbes(view, probes)

        cmds.currentTime(10)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        frame10Old, frame10New = self.readProbes(view, probes)

        def differs(a, b):
            return max(abs(int(p) - int(q)) for p, q in zip(a, b)) > 8

        if not (differs(frame1Old, frame10Old) and differs(frame1New, frame10New)):
            self.fail(
                "The skinned cylinder does not move from %s to %s between frames 1 "
                "and 10: at %s the frames show %s and %s, at %s they show %s and "
                "%s. %s"
                % (probes[0], probes[1], probes[0], frame1Old, frame10Old,
                   probes[1], frame1New, frame10New,
                   self.describeMayaPlacement(cylinder, (1, 10))))


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
