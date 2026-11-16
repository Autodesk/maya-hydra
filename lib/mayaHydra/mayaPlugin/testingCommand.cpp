//
// Copyright 2025 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "testingCommand.h"

#include "renderOverride.h"
#if MAYA_API_VERSION >= 20270000
#include "batchRenderer.h"
#endif

#include <mayaHydraLib/renderThread/renderThreadStats.h>
#include <mayaHydraLib/snapshot/snapshotPublishBarrier.h>

#include <maya/MArgList.h>
#include <maya/MArgParser.h>
#include <maya/MGlobal.h>
#include <maya/MIntArray.h>
#include <maya/MSelectionList.h>
#include <maya/MSyntax.h>

#include <pxr/pxr.h>

/**
 * mayaHydraTesting command
 * 
 * This command is used to contain functionality dedicated for unit testing. It can for example
 * be used to interface with the main mayaHydra code base to query the state of things.
 * 
 * Current functionality:
 * - Checking for convergence
 * - Releasing the batch renderer retained for unit tests
 */

/* Examples
    // Query convergence for a given renderer
    MEL : mayaHydraTesting -cv -rn "HdStormRendererPlugin"
    Python : maya.cmds.mayaHydraTesting(converged=True, rendererName="HdStormRendererPlugin")

    // Release the batch renderer retained for unit tests
    MEL : mayaHydraTesting -rbr
    Python : maya.cmds.mayaHydraTesting(releaseBatchRenderer=True)

    // Frame publication boundary state. Inert unless MAYAHYDRA_SNAPSHOT=1.
    MEL : mayaHydraTesting -sst
    Python : maya.cmds.mayaHydraTesting(snapshotStats=True)
    # -> [enabled, currentFrameId, publishedFrameId, publishCount]

    // Render thread state. Inert unless MAYAHYDRA_PIPELINE_OVERLAP=1.
    Python : maya.cmds.mayaHydraTesting(renderThreadStats=True)
    Python : maya.cmds.mayaHydraTesting(resetRenderThreadStats=True)

    // Wait for every render thread to publish its frame. Call before capturing an image.
    Python : maya.cmds.mayaHydraTesting(drainRenderThread=True)

    // The viewport's render target size, as [width, height].
    Python : maya.cmds.mayaHydraTesting(renderTargetSize=True)

    // A viewport pick over a screen rectangle (x1, y1, x2, y2). Returns objects selected.
    Python : maya.cmds.mayaHydraTesting(viewportPick=(100, 100, 200, 200))
*/

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraTestingCommand::commandName("mayaHydraTesting");

namespace {

// Flags
constexpr auto kConverged = "-cv";
constexpr auto kConvergedLong = "-converged";
constexpr auto kRendererName = "-rn";
constexpr auto kRendererNameLong = "-rendererName";
#if MAYA_API_VERSION >= 20270000
// Maya rejects short flag names longer than three characters.
constexpr auto kReleaseBatchRenderer = "-rbr";
constexpr auto kReleaseBatchRendererLong = "-releaseBatchRenderer";
#endif
constexpr auto kSnapshotStats = "-sst";
constexpr auto kSnapshotStatsLong = "-snapshotStats";
constexpr auto kDrainRenderThread = "-drt";
constexpr auto kDrainRenderThreadLong = "-drainRenderThread";
constexpr auto kRenderThreadStats = "-rts";
constexpr auto kRenderThreadStatsLong = "-renderThreadStats";
constexpr auto kResetRenderThreadStats = "-rrt";
constexpr auto kResetRenderThreadStatsLong = "-resetRenderThreadStats";
constexpr auto kRenderTargetSize = "-rtz";
constexpr auto kRenderTargetSizeLong = "-renderTargetSize";
constexpr auto kViewportPick = "-vpk";
constexpr auto kViewportPickLong = "-viewportPick";

} // namespace

MSyntax MayaHydraTestingCommand::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);

    syntax.addFlag(kConverged, kConvergedLong);

    syntax.addFlag(kRendererName, kRendererNameLong, MSyntax::kString);

#if MAYA_API_VERSION >= 20270000
    syntax.addFlag(kReleaseBatchRenderer, kReleaseBatchRendererLong);
#endif

    syntax.addFlag(kSnapshotStats, kSnapshotStatsLong);

    syntax.addFlag(kDrainRenderThread, kDrainRenderThreadLong);
    syntax.addFlag(kRenderThreadStats, kRenderThreadStatsLong);
    syntax.addFlag(kResetRenderThreadStats, kResetRenderThreadStatsLong);
    syntax.addFlag(kRenderTargetSize, kRenderTargetSizeLong);
    syntax.addFlag(
        kViewportPick,
        kViewportPickLong,
        MSyntax::kLong,
        MSyntax::kLong,
        MSyntax::kLong,
        MSyntax::kLong);

    return syntax;
}

MStatus MayaHydraTestingCommand::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st) {
        return st;
    }

    if (argData.isFlagSet(kConverged)) {
        std::vector<MString> activeRendererNames = MtohRenderOverride::AllActiveRendererNames();
        if (activeRendererNames.empty()) {
            displayError("There are no active Hydra renderers.", true);
            return MS::kFailure;
        }

        MString rendererName;
        if (argData.isFlagSet(kRendererName, &st)) {
            if (!st) { return st; }
            rendererName = argData.flagArgumentString(kRendererName, 0, &st);
            if (!st) { return st; }
        }

        if (!rendererName.isEmpty()) {
            TfToken rendererNameToken(rendererName.asChar());
            setResult(MtohRenderOverride::HasConverged(rendererNameToken));
        } else {
            // Check all renderers
            bool haveConverged = true;
            for (const auto& activeRendererName : activeRendererNames) {
                TfToken rendererNameToken(activeRendererName.asChar());
                haveConverged = haveConverged && MtohRenderOverride::HasConverged(rendererNameToken);
            }
            setResult(haveConverged);
        }
    }

#if MAYA_API_VERSION >= 20270000
    if (argData.isFlagSet(kReleaseBatchRenderer)) {
        BatchRenderer::ReleaseRetainedForTest();
    }
#endif

    if (argData.isFlagSet(kSnapshotStats)) {
        const auto stats = SnapshotManager::instance().GetStats();

        // One array rather than separate flags, so a test reads a single frame's state.
        MIntArray result;
        result.append(stats.enabled ? 1 : 0);
        result.append(static_cast<int>(stats.currentFrameId));
        result.append(static_cast<int>(stats.publishedFrameId));
        result.append(static_cast<int>(stats.publishCount));
        setResult(result);
    }

    if (argData.isFlagSet(kResetRenderThreadStats)) {
        MAYAHYDRA_NS_DEF::RenderThreadStats::instance().Reset();
    }

    if (argData.isFlagSet(kDrainRenderThread)) {
        setResult(MtohRenderOverride::DrainAllRenderThreads());
    }

    if (argData.isFlagSet(kRenderTargetSize)) {
        const auto size = MtohRenderOverride::LastRenderTargetSize();
        MIntArray  result;
        result.append(size.first);
        result.append(size.second);
        setResult(result);
    }

    if (argData.isFlagSet(kViewportPick)) {
        // The only route to MtohRenderOverride::select. There is no scripted equivalent:
        // cmds.select bypasses the viewport, and selectFromScreen is C++ only.
        int coords[4] = { 0, 0, 0, 0 };
        for (unsigned int i = 0; i < 4; ++i) {
            coords[i] = argData.flagArgumentInt(kViewportPick, i, &st);
            if (!st) { return st; }
        }

        // Deliberately kReplaceList: a test that adds to the selection cannot
        // assert what it picked.
        st = MGlobal::selectFromScreen(
            static_cast<short>(coords[0]), static_cast<short>(coords[1]),
            static_cast<short>(coords[2]), static_cast<short>(coords[3]),
            MGlobal::kReplaceList,
            MGlobal::kSurfaceSelectMethod);
        if (!st) {
            displayError(
                "mayaHydraTesting -viewportPick: MGlobal::selectFromScreen "
                "failed. It acts on the active 3d view, so this usually means "
                "no model panel is active.",
                true);
            return st;
        }

        MSelectionList picked;
        MGlobal::getActiveSelectionList(picked);
        setResult(static_cast<int>(picked.length()));
    }

    if (argData.isFlagSet(kRenderThreadStats)) {
        // Fixed order, mirrored by RENDER_THREAD_STAT_NAMES in the test. The first four
        // exist so a passing overlap test cannot be a silent fallback: the fallback
        // renders correctly, so a working viewport proves nothing on its own.
        const auto stats = MAYAHYDRA_NS_DEF::RenderThreadStats::instance().Get();
        MIntArray  result;
        result.append(stats.overlapEnabled ? 1 : 0);
        result.append(stats.started ? 1 : 0);
        result.append(stats.contextArenaAcquired ? 1 : 0);
        result.append(stats.ownsSurface ? 1 : 0);
        result.append(static_cast<int>(stats.framesRenderedOffThread));
        result.append(static_cast<int>(stats.framesComposited));
        result.append(static_cast<int>(stats.framesRenderedSynchronously));
        result.append(static_cast<int>(stats.fenceSignalFailures));
        result.append(static_cast<int>(stats.fenceWaitTimeouts));
        result.append(static_cast<int>(stats.drains));
        result.append(static_cast<int>(stats.quiescenceWaits));
        result.append(static_cast<int>(stats.drainTimeouts));
        result.append(static_cast<int>(stats.settles));
        result.append(static_cast<int>(stats.settleRetries));
        result.append(static_cast<int>(stats.settlesAbandoned));
        result.append(static_cast<int>(stats.presentsWithoutDepth));
        result.append(static_cast<int>(stats.presentsSkipped));
        result.append(static_cast<int>(stats.aovCopyFailures));
        result.append(static_cast<int>(stats.copyTargetWaits));
        result.append(static_cast<int>(stats.copyTargetWaitFailures));
        result.append(static_cast<int>(stats.compositeFenceSignalFailures));
        result.append(static_cast<int>(stats.compositeReusedLastGood));
        result.append(static_cast<int>(stats.idleWaitTimeouts));
        result.append(static_cast<int>(stats.sharedBufferFenceFailures));
        // Microseconds, not nanoseconds: a 32-bit nanosecond count wraps after 2.1
        // seconds, to a plausible small number rather than an obvious error.
        result.append(static_cast<int>(stats.idleWaitNanos / 1000));
        result.append(static_cast<int>(stats.aovCopyNanos / 1000));
        result.append(static_cast<int>(stats.compositeNanos / 1000));
        result.append(static_cast<int>(stats.compositeHandoffNanos / 1000));
        result.append(static_cast<int>(stats.compositeFenceNanos / 1000));
        result.append(static_cast<int>(stats.compositeBlitNanos / 1000));
        result.append(stats.interlockBypassed ? 1 : 0);
        result.append(stats.aovCopyBypassed ? 1 : 0);
        setResult(result);
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
