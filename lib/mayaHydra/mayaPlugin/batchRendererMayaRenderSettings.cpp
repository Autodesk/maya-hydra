//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#include "batchRendererMayaRenderSettings.h"

#include "pluginDebugCodes.h"
#include "renderVarUtils.h"

#include <mayaHydraLib/sceneIndex/registration.h>
#include <mayaHydraLib/mixedUtils.h>

#include <flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.h>
#include <flowViewport/API/renderViewData/fvpRenderViewDataManager.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>
#include <flowViewport/imageWriter/fvpRenderBufferWriter.h>
#include <flowViewport/imageWriter/fvpTextureBufferWriter.h>

#include <ufe/pathString.h>

#include <ufeExtensions/Global.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/tokens.h>

#include <maya/MFnCamera.h>
#include <maya/MStatus.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

using namespace MayaHydra;
PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MStatus BatchRendererMayaRenderSettings::Render(
    BatchRenderer& renderer,
    const BatchRenderer::InputParams& inputParams)
{
    // Maya render settings path: render directly from Maya scene state and input params.
    // It would be good to clear the resources of the overrides that are
    // not in active use, but I'm not sure if we have a better way than
    // the idle time we use currently. The approach below would break if
    // two render overrides were used at the same time.
    // for (auto* override: _allInstances) {
    //     if (override != this) {
    //         override->ClearHydraResources();
    //     }
    // }
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER)
        .Msg("BatchRenderer::RenderFromMayaRenderSettings()\n");

    // Collect tasks, update scene indices, and execute one frame.
    auto renderFrame = [&]() {
        HdTaskSharedPtrVector tasks = renderer._taskController->GetRenderingTasks();

        if (renderer._mayaHydraSceneIndex) {
            if (!TF_VERIFY(
                    renderer._mayaHydraSceneIndex->useMeshAdapter(),
                    "The environment variable MAYA_HYDRA_USE_MESH_ADAPTER is turned off explicitly. Please either remove that environment variable or turn it on to use production rendering."))
            {
                return;
            }
        }

        // Update shadow collection for lights
        if (renderer._mayaHydraSceneIndex) {
            renderer._mayaHydraSceneIndex->UpdateLightsShadowCollection();
        }

        // Update plugin data producers
        for (auto& viewData : Fvp::RenderViewDataManager::Get().GetAllViewData()) {
            for (auto& dataProducer : viewData.GetDataProducerSceneIndicesData()) {
                dataProducer->UpdateVisibility();
                dataProducer->UpdateTransform();
            }
        }

        // Update plugin filtering scene indices
        std::string rendererNamesToUpdate;
        for (auto& sceneFilteringSceneIndexData : Fvp::FilteringSceneIndexInterfaceImp::get()
                 .getSceneFilteringSceneIndicesData()) {
            if (sceneFilteringSceneIndexData->UpdateVisibility()) {
                rendererNamesToUpdate += sceneFilteringSceneIndexData->GetClient()->getRendererNames();
            }
        }
        for (auto& selectionHighlightFilteringSceneIndexData :
             Fvp::FilteringSceneIndexInterfaceImp::get().getSelectionHighlightFilteringSceneIndicesData()) {
            if (selectionHighlightFilteringSceneIndexData->UpdateVisibility()) {
                rendererNamesToUpdate += selectionHighlightFilteringSceneIndexData->GetClient()->getRendererNames();
            }
        }
        if (!rendererNamesToUpdate.empty()) {
            Fvp::FilteringSceneIndicesChainManager::get()
                .updateFilteringSceneIndicesChain(rendererNamesToUpdate);
        }

        renderer._engine.Execute(renderer._renderIndex, &tasks);
    }; // End of renderFrame lambda.

    if (renderer._initializationAttempted && !renderer._initializationSucceeded) {
        // Initialization must have failed already, stop trying.
        return MStatus::kFailure;
    }

    if (renderer._needsClear.exchange(false)) {
        constexpr bool fullReset = false;
        renderer.ClearHydraResources(fullReset);
    }

    if (!renderer._initializationAttempted) {
        renderer._InitHydraResources();

        if (!renderer._initializationSucceeded) {
            return MStatus::kFailure;
        }
    }

    // TODO_BATCH_RENDER  Remove this viewport architecture dependency.
    const std::string panelName{BatchRenderer::kBatchRenderDummyPanelName};
    auto& manager = Fvp::RenderViewDataManager::Get();
    if (!manager.ViewIsAlreadyRegistered(panelName)) {
        const Fvp::InformationInterface::RenderViewDesc renderViewDesc(panelName, false);
        // The following returns true only if there are non-Maya data producers
        // added.
        manager.AddRenderViewData(
            renderViewDesc,
            renderer.renderIndex(),
            renderer._dataProducerMergingSceneIndexProxy,
            renderer._lastFilteringSceneIndexBeforeCustomFiltering);
    }

    if (renderer._needToReplaceSelection) {
        renderer._needToReplaceSelection = false;
    }

    MayaHydraParams delegateParams = renderer._globals.delegateParams;
    delegateParams.displaySmoothMeshes = true; // This is the default.

    if (renderer._mayaHydraSceneIndex) {
        renderer._mayaHydraSceneIndex->SetParams(delegateParams);
        // TODO_BATCH_RENDER
        // Maya Hydra scene index does way too much, viewport render aware.
        // How do we fix this?  PPT, 3-Mar-2025.
        // _mayaHydraSceneIndex->PreFrame(drawContext);
    }

    HdxRenderTaskParams params;
    params.enableLighting = true;
#if PXR_VERSION <= 2508
    params.enableSceneMaterials = true;
#endif

    // Do not set params.wireframeColor, as this implies reading the
    // FvpColorPreferencesTokens->wireframeSelection from the
    // Fvp::ColorPreferences instance, which is unavailable in batch mode.

    params.cullStyle = HdCullStyleBackUnlessDoubleSided;

    const int width = static_cast<int>(inputParams.width);
    const int height = static_cast<int>(inputParams.height);
    renderer._viewport = GfVec4d(0, 0, width, height);
    renderer._taskController->SetRenderViewport(renderer._viewport);

    // Convert any aov named "Ci" to "color", "z" to "depth"
    auto normalizeAovToken = [](const TfToken& token) {
        if (token == TfToken("Ci")) {
            return HdAovTokens->color;
        }
        if (token == TfToken("z")) {
            return HdAovTokens->depth;
        }
        return token;
    };

    // Apply AOVs (RenderVars)
    // TODO
    TfTokenVector renderOutputs = inputParams.renderVarsInfo.renderVars;
    if (renderOutputs.empty()) {
        renderOutputs = { HdAovTokens->color };
    }
    TfTokenVector normalizedOutputs;
    normalizedOutputs.reserve(renderOutputs.size());
    for (const TfToken& token : renderOutputs) {
        const TfToken normalized = normalizeAovToken(token);
        if (std::find(normalizedOutputs.begin(), normalizedOutputs.end(), normalized)
            == normalizedOutputs.end()) {
            normalizedOutputs.push_back(normalized);
        }
    }
    renderOutputs = normalizedOutputs;
    renderer._taskController->SetRenderOutputs(renderOutputs);

    // Check if the AOVs include color or depth
    const auto hasToken = [&renderOutputs](const TfToken& token) {
        return std::find(renderOutputs.begin(), renderOutputs.end(), token) != renderOutputs.end();
    };
    const bool hasColorAov = hasToken(HdAovTokens->color);
    const bool hasDepthAov = hasToken(HdAovTokens->depth);

    // Set Purpose tags
    renderer.SetRenderPurposeTags(delegateParams);

    // Set MSAA as per Maya AntiAliasing settings
    if (renderer._isUsingHdSt) {
        // Maya's MSAA toggle settings
        constexpr bool isMultiSampled = true;

        // Set MSAA on Color Buffer
        if (hasColorAov) {
            HdAovDescriptor colorAovDesc
                = renderer._taskController->GetRenderOutputSettings(HdAovTokens->color);
            colorAovDesc.multiSampled = isMultiSampled;
            renderer._taskController->SetRenderOutputSettings(HdAovTokens->color, colorAovDesc);
        }

        // Set MSAA of Depth buffer
        if (hasDepthAov) {
            HdAovDescriptor depthAovDesc
                = renderer._taskController->GetRenderOutputSettings(HdAovTokens->depth);
            depthAovDesc.multiSampled = isMultiSampled;
            renderer._taskController->SetRenderOutputSettings(HdAovTokens->depth, depthAovDesc);
        }
    }

    MDagPath camPath = UfeExtensions::ufeToDagPath(inputParams.ufeCameraPath);
    if (!camPath.isValid()) {
        TF_WARN(
            "Invalid Maya camera UFE path: %s",
            Ufe::PathString::string(inputParams.ufeCameraPath).c_str());
        return MStatus::kFailure;
    }

    if (!camPath.hasFn(MFn::kCamera)) {
        if (camPath.extendToShape() != MS::kSuccess || !camPath.hasFn(MFn::kCamera)) {
            TF_WARN(
                "Failed to resolve camera shape for UFE path: %s",
                Ufe::PathString::string(inputParams.ufeCameraPath).c_str());
            return MStatus::kFailure;
        }
    }

    MStatus camStatus;
    MFnCamera cameraFn(camPath, &camStatus);
    if (camStatus != MS::kSuccess) {
        TF_WARN(
            "Failed to create MFnCamera for UFE path: %s",
            Ufe::PathString::string(inputParams.ufeCameraPath).c_str());
        return MStatus::kFailure;
    }

    GfMatrix4d viewMatrix = GetGfMatrixFromMaya(camPath.inclusiveMatrixInverse());
    GfMatrix4d projectionMatrix = GetGfMatrixFromMaya(cameraFn.projectionMatrix());
    // As per MFnCamera::projectionMatrix() documentation:
    // Maya uses a left hand coordinate system, so the entries [2][2] and [3][2] are negated.
    projectionMatrix[2][2] = -projectionMatrix[2][2];
    projectionMatrix[3][2] = -projectionMatrix[3][2];

    renderer._taskController->SetFreeCameraMatrices(viewMatrix, projectionMatrix);

    renderer._taskController->SetRenderParams(params);
    if (!params.camera.IsEmpty()) {
        renderer._taskController->SetCameraPath(params.camera);
    }

    // Default color in usdview.
    renderer._taskController->SetSelectionColor(renderer._globals.colorSelectionHighlightColor);
    renderer._taskController->SetEnableSelection(renderer._globals.colorSelectionHighlight);

    if (renderer._globals.outlineSelectionWidth != 0.f) {
        renderer._taskController->SetSelectionOutlineRadius(renderer._globals.outlineSelectionWidth);
        renderer._taskController->SetSelectionEnableOutline(true);
    } else {
        renderer._taskController->SetSelectionEnableOutline(false);
    }

    renderer._taskController->SetCollection(renderer._renderCollection);

    // Update all registered plugin before render.
    for (auto& entry : renderer._sceneIndexRegistry->GetRegistrations()) {
        entry.second->Update();
    }

    if (renderer._isUsingHdSt) {
        constexpr auto enableShadows = true;
        HdxShadowTaskParams shadowParams;
        shadowParams.cullStyle = HdCullStyleNothing;

        // The light & shadow parameters currently (19.11-20.08) are only used for tasks specific to
        // Storm
        renderer._taskController->SetEnableShadows(enableShadows);
        renderer._taskController->SetShadowParams(shadowParams);
    }

    // The renderFrame() lambda does too much to be called in a loop:
    // all we want is to call it once, then call _Execute() repeatedly.
    renderFrame();

    // For Arnold the following always returns false.
    // _isConverged = _taskController->IsConverged();
    auto isConverged = [&renderer, &renderOutputs]() {
        if (renderOutputs.empty()) {
            TF_WARN("RenderOutputs list is empty; assuming converged.");
            return true;
        }

        // All AOVs must be converged
        for (const TfToken& aovToken : renderOutputs) {
            auto renderBuffer = renderer._taskController->GetRenderOutput(aovToken);
            if (!renderBuffer) {
                TF_WARN("Render output '%s' not found; ignoring.", aovToken.GetText());
                continue;
            }
            if (!renderBuffer->IsConverged()) {
                return false;
            }
        }
        return true;
    };
    renderer._isConverged = isConverged();

    // Render to convergence.
    constexpr auto wait100ms = std::chrono::duration<float, std::milli>(100);
    while (!renderer._isConverged) {
        std::this_thread::sleep_for(wait100ms);

        // See renderFrame() lambda comments.
        HdTaskSharedPtrVector tasks = renderer._taskController->GetRenderingTasks();
        
        renderer._engine.Execute(renderer._renderIndex, &tasks);
        renderer._isConverged = isConverged();
    }

    const auto fileName = Fvp::ImageBufferWriter::GetFileName();
    if (!fileName.empty()) {
        using Writer = Fvp::ImageBufferWriter;
        const bool multipleAovs = renderOutputs.size() > 1;

        for (const TfToken& aovToken : renderOutputs) {
            std::string outputFileName = fileName;
            if (multipleAovs) {
                const std::string aovSuffix = SanitizeAovNameForFileName(aovToken.GetString());
                outputFileName = AppendAovSuffixToFileName(outputFileName, aovSuffix);
            }

            // For non-color AOVs (e.g. depth/alpha), prefer render buffers to preserve
            // single-channel formats and avoid GPU texture-only paths.
            const bool useTextureWriter = renderer._isUsingHdSt;
            Writer::Ptr writer = useTextureWriter
                ? Writer::Ptr(std::make_shared<Fvp::TextureBufferWriter>(
                      &renderer._engine, renderer._hgi.get(), aovToken))
                : Writer::Ptr(std::make_shared<Fvp::RenderBufferWriter>(
                      renderer._taskController.get(), aovToken));

            if (!Writer::Write(writer, outputFileName)) {
                TF_RUNTIME_ERROR("Failed to write image to %s", outputFileName.c_str());
            }
        }
        return MStatus::kSuccess;
    }

    return MStatus::kFailure;
}

} // namespace MAYAHYDRA_NS_DEF
