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

#include "batchRendererHydraV1RenderSettings.h"

#include "pluginDebugCodes.h"
#include "renderVarUtils.h"

#include <mayaHydraLib/sceneIndex/registration.h>
#include <mayaHydraLib/mixedUtils.h>

#include <flowViewport/imageWriter/fvpRenderBufferWriter.h>
#include <flowViewport/imageWriter/fvpTextureBufferWriter.h>

#include <mayaUsdAPI/utils.h>

#include <ufe/pathString.h>

#include <ufeExtensions/Global.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/enum.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/tokens.h>
#include <pxr/usd/usdGeom/camera.h>

#include <maya/MDagPath.h>
#include <maya/MFnCamera.h>
#include <maya/MStatus.h>

#include <algorithm>
#include <chrono>
#include <string>
#include <thread>

using namespace MayaHydra;
PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool GetUsdCameraMatricesFromPrim(
    const UsdPrim& cameraPrim,
    const Ufe::Path& ufeCameraPath,
    GfMatrix4d* outViewMatrix,
    GfMatrix4d* outProjectionMatrix)
{
    if (!outViewMatrix || !outProjectionMatrix) {
        TF_WARN("GetUsdCameraMatricesFromPrim: output matrix pointers are null.");
        return false;
    }
    if (!cameraPrim.IsValid() || !cameraPrim.IsA<UsdGeomCamera>()) {
        TF_WARN(
            "GetUsdCameraMatricesFromPrim: USD camera prim invalid or not a camera: %s",
            Ufe::PathString::string(ufeCameraPath).c_str());
        return false;
    }

    const UsdGeomCamera usdCamera(cameraPrim);
    const UsdTimeCode   timeCode = UsdTimeCode::Default();

    // Compute camera frustum to get view and projection matrices.
    const GfFrustum frustum = usdCamera.GetCamera(timeCode).GetFrustum();
    *outViewMatrix = frustum.ComputeViewMatrix();
    *outProjectionMatrix = frustum.ComputeProjectionMatrix();
    return true;
}

bool GetMayaCameraMatricesFromUfePath(
    const Ufe::Path& ufeCameraPath,
    GfMatrix4d* outViewMatrix,
    GfMatrix4d* outProjectionMatrix)
{
    if (!outViewMatrix || !outProjectionMatrix) {
        TF_WARN("GetMayaCameraMatricesFromUfePath: output matrix pointers are null.");
        return false;
    }

    MDagPath camPath = UfeExtensions::ufeToDagPath(ufeCameraPath);
    if (!camPath.isValid()) {
        TF_WARN(
            "GetMayaCameraMatricesFromUfePath: Invalid Maya camera UFE path: %s",
            Ufe::PathString::string(ufeCameraPath).c_str());
        return false;
    }

    if (!camPath.hasFn(MFn::kCamera)) {
        if (camPath.extendToShape() != MS::kSuccess || !camPath.hasFn(MFn::kCamera)) {
            TF_WARN(
                "GetMayaCameraMatricesFromUfePath: Failed to resolve camera shape for UFE path: "
                "%s",
                Ufe::PathString::string(ufeCameraPath).c_str());
            return false;
        }
    }

    MStatus   camStatus;
    MFnCamera cameraFn(camPath, &camStatus);
    if (camStatus != MS::kSuccess) {
        TF_WARN(
            "GetMayaCameraMatricesFromUfePath: Failed to create MFnCamera for UFE path: %s",
            Ufe::PathString::string(ufeCameraPath).c_str());
        return false;
    }

    *outViewMatrix = GetGfMatrixFromMaya(camPath.inclusiveMatrixInverse());
    *outProjectionMatrix = GetGfMatrixFromMaya(cameraFn.projectionMatrix());
    // As per MFnCamera::projectionMatrix() documentation:
    // Maya uses a left hand coordinate system, so the entries [2][2] and [3][2] are negated.
    (*outProjectionMatrix)[2][2] = -(*outProjectionMatrix)[2][2];
    (*outProjectionMatrix)[3][2] = -(*outProjectionMatrix)[3][2];
    return true;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

MStatus BatchRendererHydraV1RenderSettings::Render(
    BatchRenderer& renderer,
    const BatchRenderer::InputParams& inputParams)
{
    // Hydra V1 render settings path: uses USD render settings and UFE camera.
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

    HdxRenderTaskParams params;
    if (!renderer._PrepareHydraBatchRender(
            static_cast<int>(inputParams.width),
            static_cast<int>(inputParams.height),
            &params)) {
        return MStatus::kFailure;
    }

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

    HdRenderDelegate* renderDelegate = renderer._GetRenderDelegate();
    for (const TfToken& aovToken : renderOutputs) {
        HdFormat desiredFormat = HdFormatInvalid;
        if (renderDelegate) {
            const HdAovDescriptor delegateDesc = renderDelegate->GetDefaultAovDescriptor(aovToken);
            desiredFormat = delegateDesc.format;
        }
        if (desiredFormat == HdFormatInvalid) {
            const auto it = inputParams.renderVarsInfo.dataTypes.find(aovToken);
            if (it != inputParams.renderVarsInfo.dataTypes.end()) {
                desiredFormat = GetHdFormatFromRenderVarDataType(it->second);
            }
        }
        if (desiredFormat == HdFormatInvalid) {
            continue;
        }

        HdAovDescriptor aovDesc = renderer._taskController->GetRenderOutputSettings(aovToken);
        if (aovDesc.format != desiredFormat) {
            aovDesc.format = desiredFormat;
            renderer._taskController->SetRenderOutputSettings(aovToken, aovDesc);
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                "RenderVar format for %s set to %s\n",
                aovToken.GetText(),
                TfEnum::GetName(desiredFormat).c_str());
        }
    }

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

    GfMatrix4d viewMatrix(1.0);
    GfMatrix4d projectionMatrix(1.0);
    bool        hasCameraMatrices = false;

    // Resolve view/projection matrices from a camera.
    const Ufe::Path& ufeCameraPath = inputParams.ufeCameraPath;
    if (ufeCameraPath.empty()) {
        TF_WARN("Missing camera UFE path.");
        return MStatus::kFailure;
    }

    if (ufeCameraPath.runTimeId() == UfeExtensions::getUsdRunTimeId()) {
        // USD Camera
        const UsdPrim cameraPrim = MayaUsdAPI::ufePathToPrim(ufeCameraPath);
        if (!GetUsdCameraMatricesFromPrim(
                cameraPrim,
                ufeCameraPath,
                &viewMatrix,
                &projectionMatrix)) {
            return MStatus::kFailure;
        }
        hasCameraMatrices = true;
    } else {
        // Maya Camera
        if (!GetMayaCameraMatricesFromUfePath(
                ufeCameraPath,
                &viewMatrix,
                &projectionMatrix)) {
            return MStatus::kFailure;
        }
        hasCameraMatrices = true;
    }
        

        
    if (!hasCameraMatrices) {
        TF_WARN(
            "Unable to compute camera matrices for UFE path: %s",
            Ufe::PathString::string(ufeCameraPath).c_str());
        return MStatus::kFailure;
    }

    renderer._taskController->SetFreeCameraMatrices(viewMatrix, projectionMatrix);
    renderer._FinalizeHydraBatchRender(params);

    // The common render frame does too much to be called in a loop:
    // all we want is to call it once, then call _Execute() repeatedly.
    renderer._ExecuteHydraBatchRenderFrame();

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

        // See _ExecuteHydraBatchRenderFrame() comment.
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
