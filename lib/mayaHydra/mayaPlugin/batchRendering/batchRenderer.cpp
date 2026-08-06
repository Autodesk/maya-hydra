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

#include "batchRenderer.h"
#include "batchRendererHydraV1RenderSettings.h"
#include "batchRendererHydraV2RenderSettings.h"
#include "batchRendererMayaRenderSettings.h"
#include "tokens.h"

#include "pluginDebugCodes.h"
#include "renderSettingsUtils.h"

#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/sceneIndex/registration.h>

#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif
#include <flowViewport/tokens.h>
#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>
#include <flowViewport/API/renderViewData/fvpRenderViewDataManager.h>
#include <flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>
#include <pxr/pxr.h>

#include <pxr/base/tf/getenv.h>
#include <pxr/base/gf/range2f.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>

#include <ufeExtensions/Global.h>
#include <ufe/colorManagementHandler.h>

#include <maya/MMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/MStatus.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

using namespace MayaHydra;
PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Look at defaultRenderGlobals.hydraSceneDumpPath attribute and
// dump Hydra scene to that file path.  Hydra scene will be taken from
// the output of the defaultRenderGlobals.hydraSceneDumpSceneIndex
// scene index (default to the terminal scene index).
void dumpHydraScene(const HdRenderIndex* renderIndex)
{
    MObject nodeObj;
    MString dumpPath;
    std::string sceneIndexName;
    constexpr const char* kDefaultRenderGlobalsNodeName = "defaultRenderGlobals";    
    if (GetDependNodeFromNodeName(kDefaultRenderGlobalsNodeName, nodeObj)) {
        MFnDependencyNode depNode(nodeObj);
        MPlug plug = depNode.findPlug("hydraSceneDumpPath", true);
        if (!plug.isNull()) {
            dumpPath = plug.asString();
        }
        MPlug siPlug = depNode.findPlug("hydraSceneDumpSceneIndex", true);
        if (!siPlug.isNull()) {
            sceneIndexName = siPlug.asString().asChar();
        }
    }

    if (dumpPath.length() > 0) {
        std::ofstream dumpFile(dumpPath.asChar());
        if (dumpFile.is_open()) {
            constexpr const char* kTerminalSceneIndexMsg = "terminal scene index";
            auto si = renderIndex->GetTerminalSceneIndex();
            if (!sceneIndexName.empty()) {
                auto siHasName = Fvp::SceneIndexDisplayNamePred(sceneIndexName);
                auto found = Fvp::findSceneIndexInTree(si, siHasName);
                if (!found) {
                    TF_WARN("Scene index %s not found for scene dump, using %s", sceneIndexName.c_str(), kTerminalSceneIndexMsg);
                    sceneIndexName = kTerminalSceneIndexMsg;
                } else {
                    si = found;
                }
            } else {
                sceneIndexName = kTerminalSceneIndexMsg;
            }
            Fvp::SceneIndexInspector inspector(si);
            inspector.WriteHierarchy(dumpFile);
            TF_STATUS("Hydra scene from scene index %s written to %s", sceneIndexName.c_str(), dumpPath.asChar());
        }
    }
}

std::string getOCIOConfigFilePath()
{
    const auto colorManagement
        = Ufe::ColorManagementHandler::colorManagementHandler(UfeExtensions::getMayaRunTimeId());
    if (!colorManagement || !colorManagement->isColorManagementEnabled()) {
        return {};
    }

    return colorManagement->getConfigFilePath();
}

// Fvp::RenderViewDataManager::AddRenderViewData connects a custom data
// producer scene index chain with the Hydra Flow Viewport Toolkit merging
// scene index, depending on the Hydra renderer.  A dummy panel name is
// used to connect the scene index chain for batch rendering.

const SdfPath MAYA_NATIVE_ROOT = SdfPath("/MayaData");

} // namespace

namespace MAYAHYDRA_NS_DEF {

BatchRenderer::BatchRenderer(const MtohRendererDescription& desc)
    : _rendererDesc(desc)
    , _sceneIndexRegistry(nullptr)
    , _globals(MtohRenderGlobals::GetInstance())
    , _hgi(Hgi::CreatePlatformDefaultHgi())
    , _hgiDriver { HgiTokens->renderDriver, VtValue(_hgi.get()) }
    , _fvpSelectionTracker(new Fvp::SelectionTracker)
    , _isUsingHdSt(desc.rendererName == MtohTokens->HdStormRendererPlugin)
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "BatchRenderer created (%s - %s - %s)\n",
            _rendererDesc.rendererName.GetText(),
            _rendererDesc.overrideName.GetText(),
            _rendererDesc.displayName.GetText());
    _ID = MAYA_NATIVE_ROOT.AppendChild(
                  TfToken(TfStringPrintf("_MayaHydra_%s_%p", desc.rendererName.GetText(), this)));

    MStatus status;
    auto    id
        = MSceneMessage::addCallback(MSceneMessage::kBeforeNew, _ClearHydraCallback, this, &status);
    if (status) {
        _callbacks.append(id);
    }
    id = MSceneMessage::addCallback(MSceneMessage::kBeforeOpen, _ClearHydraCallback, this, &status);
    if (status) {
        _callbacks.append(id);
    }
}

BatchRenderer::~BatchRenderer()
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "BatchRenderer destroyed (%s - %s - %s)\n",
            _rendererDesc.rendererName.GetText(),
            _rendererDesc.overrideName.GetText(),
            _rendererDesc.displayName.GetText());

    _ClearHydraResources();

    MMessage::removeCallbacks(_callbacks);
    _callbacks.clear();
}

HdRenderDelegate* BatchRenderer::_GetRenderDelegate()
{
    return _renderIndex ? _renderIndex->GetRenderDelegate() : nullptr;
}

MStatus BatchRenderer::RenderFromMayaRenderSettings(
    const InputParams& inputParams)
{
    // Delegate the Maya render-settings path to the dedicated implementation.
    return BatchRendererMayaRenderSettings::Render(*this, inputParams);
}
  
MStatus BatchRenderer::RenderFromHydraV1RenderSettings(
    const InputParams& inputParams)
{
    // Delegate the Hydra V1 render-settings path to the dedicated implementation.
    return BatchRendererHydraV1RenderSettings::Render(*this, inputParams);
}

MStatus BatchRenderer::RenderFromHydraV2RenderSettings()
{
    // Delegate the Hydra V2 render-settings path to the dedicated implementation.
    return BatchRendererHydraV2RenderSettings::Render(*this);
}

void BatchRenderer::_SetRenderPurposeTags(const MayaHydraParams& delegateParams)
{
    TfTokenVector mhRenderTags = {HdRenderTagTokens->geometry};
    if (delegateParams.renderPurpose)
        mhRenderTags.push_back(HdRenderTagTokens->render);
    if (delegateParams.proxyPurpose)
        mhRenderTags.push_back(HdRenderTagTokens->proxy);
    if (delegateParams.guidePurpose)
        mhRenderTags.push_back(HdRenderTagTokens->guide);
    _taskController->SetRenderTags(mhRenderTags);
}

bool BatchRenderer::Initialize()
{
    if (_initializationAttempted && !_initializationSucceeded) {
        // Initialization must have failed already, stop trying.
        return false;
    }

    if (!_initializationAttempted) {
        _InitHydraResources();
        if (!_initializationSucceeded) {
            return false;
        }
    }

    const std::string panelName { kBatchRenderDummyPanelName };
    auto&             manager = Fvp::RenderViewDataManager::Get();
    if (!manager.ViewIsAlreadyRegistered(panelName)) {
        const Fvp::InformationInterface::RenderViewDesc renderViewDesc(panelName, false);
        // The following returns true only if there are non-Maya data producers
        // added.
        manager.AddRenderViewData(
            renderViewDesc,
            renderIndex(),
            _dataProducerMergingSceneIndexProxy,
            _lastFilteringSceneIndexBeforeCustomFiltering);
    }

    MayaHydraParams delegateParams = _globals.delegateParams;
    delegateParams.displaySmoothMeshes = true; // This is the default.

    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->SetParams(delegateParams);
    }

    // Set Purpose tags.
    _SetRenderPurposeTags(delegateParams);

    return true;
}

bool BatchRenderer::_PrepareRender(
    unsigned int         width,
    unsigned int         height,
    HdxRenderTaskParams& outParams,
    const std::optional<GfRect2i>& dataWindow)
{
    _viewport = GfVec4d(0, 0, width, height);

    HdxRenderTaskParams params;
    params.enableLighting = true;
#if PXR_VERSION <= 2508
    params.enableSceneMaterials = true;
#endif
    params.cullStyle = HdCullStyleBackUnlessDoubleSided;

    // Crop region (UsdRenderProduct.dataWindowNDC -> CameraUtilFraming):
    // keep the AOV buffer at full image size and only restrict the data
    // window so the render delegate writes the AOV clear value outside the
    // crop.  HdxTaskController's framing/buffer-size APIs are separate from
    // SetRenderViewport; calling SetFraming + SetRenderBufferSize disables
    // the legacy viewport path while keeping the AOV buffer at full
    // (width x height).
    if (dataWindow.has_value()) {
        // Sanitize the rect to image bounds; some delegates (e.g. HdPrman 26)
        // crash on out-of-range data windows.
        // Guard against zero resolution: imgMaxX/Y would be -1, making
        // std::clamp(val, 0, -1) undefined behaviour (lo > hi).
        if (width == 0 || height == 0) {
            TF_WARN(
                "BatchRenderer::_PrepareRender: zero image dimension (%ux%u); "
                "ignoring crop region.",
                width, height);
            _taskController->SetRenderViewport(_viewport);
            outParams = params;
            return true;
        }
        const int imgMaxX = static_cast<int>(width)  - 1;
        const int imgMaxY = static_cast<int>(height) - 1;
        const int minX = std::clamp(dataWindow->GetMinX(), 0, imgMaxX);
        const int minY = std::clamp(dataWindow->GetMinY(), 0, imgMaxY);
        const int maxX = std::clamp(dataWindow->GetMaxX(), minX, imgMaxX);
        const int maxY = std::clamp(dataWindow->GetMaxY(), minY, imgMaxY);
        const GfRect2i  sanitized(GfVec2i(minX, minY), GfVec2i(maxX, maxY));
        const GfRange2f displayWindow(
            GfVec2f(0.0f, 0.0f),
            GfVec2f(static_cast<float>(width), static_cast<float>(height)));
        const CameraUtilFraming framing(displayWindow, sanitized);

        _taskController->SetRenderBufferSize(GfVec2i(
            static_cast<int>(width), static_cast<int>(height)));
        _taskController->SetFraming(framing);
        _taskController->SetOverrideWindowPolicy(
            std::optional<CameraUtilConformWindowPolicy>(CameraUtilFit));
    } else {
        _taskController->SetRenderViewport(_viewport);
    }

    outParams = params;

    return true;
}

void BatchRenderer::_FinalizeHydraBatchRender(const HdxRenderTaskParams& params)
{
    _taskController->SetRenderParams(params);
    if (!params.camera.IsEmpty()) {
        _taskController->SetCameraPath(params.camera);
    }

    _taskController->SetCollection(_renderCollection);

    // Update all registered plugin before render.
    for (auto& entry : _sceneIndexRegistry->GetRegistrations()) {
        entry.second->Update();
    }

    if (_isUsingHdSt) {
        constexpr auto      enableShadows = true;
        HdxShadowTaskParams shadowParams;
        shadowParams.cullStyle = HdCullStyleNothing;

        // The light & shadow parameters currently (19.11-20.08) are only used for tasks specific
        // to Storm.
        _taskController->SetEnableShadows(enableShadows);
        _taskController->SetShadowParams(shadowParams);
    }
}

void BatchRenderer::_ExecuteHydraBatchRenderFrame()
{
    HdTaskSharedPtrVector tasks = _taskController->GetRenderingTasks();

    if (_mayaHydraSceneIndex) {
        if (!TF_VERIFY(
                _mayaHydraSceneIndex->useMeshAdapter(),
                "The environment variable MAYA_HYDRA_USE_MESH_ADAPTER is turned off "
                "explicitly. Please either remove that environment variable or turn it on to "
                "use production rendering.")) {
            return;
        }
    }

    // Update shadow collection for lights
    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->UpdateLightsShadowCollection();
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
    for (auto& sceneFilteringSceneIndexData :
         Fvp::FilteringSceneIndexInterfaceImp::get().getSceneFilteringSceneIndicesData()) {
        if (sceneFilteringSceneIndexData->UpdateVisibility()) {
            rendererNamesToUpdate += sceneFilteringSceneIndexData->GetClient()->getRendererNames();
        }
    }
    for (auto& selectionHighlightFilteringSceneIndexData :
         Fvp::FilteringSceneIndexInterfaceImp::get()
             .getSelectionHighlightFilteringSceneIndicesData()) {
        if (selectionHighlightFilteringSceneIndexData->UpdateVisibility()) {
            rendererNamesToUpdate
                += selectionHighlightFilteringSceneIndexData->GetClient()->getRendererNames();
        }
    }
    if (!rendererNamesToUpdate.empty()) {
        Fvp::FilteringSceneIndicesChainManager::get().updateFilteringSceneIndicesChain(
            rendererNamesToUpdate);
    }

    _engine.Execute(_renderIndex, &tasks);

    dumpHydraScene(_renderIndex);
}

void BatchRenderer::_ClearMayaHydraSceneIndex()
{
#ifdef CODE_COVERAGE_WORKAROUND
    // Leak the Maya scene index for code coverage, as its base class
    // HdRetainedSceneIndex dtor crashes in Windows clang code coverage build.
    _mayaHydraSceneIndex->_Destroy();
#else
    if (_dataProducerMergingSceneIndexProxy && _mayaHydraSceneIndex) {
        _dataProducerMergingSceneIndexProxy->RemoveSceneIndex(_mayaHydraSceneIndex);
    }
#endif
    _mayaHydraSceneIndex.Reset();
}

void BatchRenderer::_InitHydraResources()
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("BatchRenderer::_InitHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    _initializationAttempted = true;

    GlfContextCaps::InitInstance();
    _rendererPlugin
        = HdRendererPluginRegistry::GetInstance().GetRendererPlugin(_rendererDesc.rendererName);
    if (!_rendererPlugin)
        return;

    _renderDelegate = HdRendererPluginRegistry::GetInstance().CreateRenderDelegate(_rendererDesc.rendererName);
    if (!_renderDelegate)
        return;

    _renderIndex = HdRenderIndex::New(_renderDelegate.Get(), {&_hgiDriver});
    if (!_renderIndex)
        return;
    GetMayaHydraLibInterface().RegisterTerminalSceneIndex(_renderIndex->GetTerminalSceneIndex());

    _taskController = std::make_unique<HdxTaskController>(
        _renderIndex,
        _ID.AppendChild(TfToken(TfStringPrintf(
            "_UsdImaging_%s_%p",
            TfMakeValidIdentifier(_rendererDesc.rendererName.GetText()).c_str(),
            this)))
    );
    _taskController->SetEnableShadows(true);
    
    MayaHydraInitData mhInitData(
        TfToken("MayaHydraSceneIndex"),
        *renderIndex(),
        MAYA_NATIVE_ROOT,
        _isUsingHdSt
    );

    // Data producer merging scene index sets up the Flow Viewport merging scene index, must
    // be created first, as it is required for:
    // - Selection scene index, which uses the Flow Viewport merging scene
    //   index as input.
    // - Maya scene producer, which needs the render index proxy to insert
    //   itself.

    _dataProducerMergingSceneIndexProxy
        = std::make_shared<Fvp::DataProducerMergingSceneIndexProxy>();

    constexpr bool interactive = false;
    _mayaHydraSceneIndex = MayaHydraSceneIndex::New(mhInitData, interactive);
    TF_VERIFY(_mayaHydraSceneIndex, "Maya Hydra scene index not found, check mayaHydra plugin installation.");
    
    VtValue fvpSelectionTrackerValue(_fvpSelectionTracker);
    _engine.SetTaskContextData(FvpTokens->fvpSelectionState, fvpSelectionTrackerValue);
    // Keep Hdx selection tasks satisfied even when selection replacement is disabled.
    const HdxSelectionTrackerSharedPtr hdxSelectionTracker
        = std::make_shared<HdxSelectionTracker>();
    _engine.SetTaskContextData(HdxTokens->selectionState, VtValue(hdxSelectionTracker));

    _mayaHydraSceneIndex->Populate();
    //Add the scene index as an input scene index of the merging scene index
    _dataProducerMergingSceneIndexProxy->InsertSceneIndex(
        _mayaHydraSceneIndex, SdfPath::AbsoluteRootPath());
    
    if (!_sceneIndexRegistry) {
        constexpr bool interactive = false;
        _sceneIndexRegistry.reset(new MayaHydraSceneIndexRegistry(
            _dataProducerMergingSceneIndexProxy->GetMergingSceneIndex(), interactive));
    }
    
    //Create internal scene indices chain
    _inputSceneIndexOfFilteringSceneIndicesChain
        = _dataProducerMergingSceneIndexProxy->GetMergingSceneIndex();

    //Put BlockPrimRemovalPropagationSceneIndex first as it can block/unblock the prim removal propagation on the whole scene indices chain
    _blockPrimRemovalPropagationSceneIndex = Fvp::BlockPrimRemovalPropagationSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _pruningSceneIndex = Fvp::PruningSceneIndex::New(_blockPrimRemovalPropagationSceneIndex);
    _pruningSceneIndex->AddExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya filtering is handled by VP2/OGS.
    _inputSceneIndexOfFilteringSceneIndicesChain = _pruningSceneIndex;

    _CreateSceneIndicesChainAfterMergingSceneIndex();
    
    if (auto* renderDelegate = _GetRenderDelegate()) {
        // Pull in any options that may have changed due file-open.
        // If the currentScene has defaultRenderGlobals we'll absorb those new settings,
        // but if not, fallback to user-defaults (current state) .
        const bool filterRenderer = true;
        const bool fallbackToUserDefaults = true;
        _globals.GlobalChanged(
            { _rendererDesc.rendererName, filterRenderer, fallbackToUserDefaults });
        _globals.ApplySettings(renderDelegate, _rendererDesc.rendererName);
    }

    // Batch rendering initialization, from 
    // UsdAppUtilsFrameRecorder::UsdAppUtilsFrameRecorder().
    _taskController->SetEnablePresentation(false);
    _renderDelegate->SetRenderSetting(
        HdRenderSettingsTokens->enableInteractive, VtValue(false));

    const std::string ocioConfigFilePath = getOCIOConfigFilePath();

    if (!ocioConfigFilePath.empty()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
            "Render setting " + BatchRenderTokens->ocioConfigPath.GetString() + " set to "
                + ocioConfigFilePath + "\n");

        _renderDelegate->SetRenderSetting(BatchRenderTokens->ocioConfigPath, VtValue(ocioConfigFilePath));
    }

    // Tell render delegate to read render settings from the Hydra
    // scene (Hydra v2 render settings), rather than from the render
    // settings map (Hydra v1 render settings).  This is an
    // Autodesk-specific convention which render delegate providers
    // can use.
    
    // At time of writing (2026-06-02) only Hydra Arnold understands this
    // token, requires further testing.
    if (TfGetenvBool("MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS", false)) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                     "Render setting " + BatchRenderTokens->renderSettingsSrc.GetString() + " set to " + BatchRenderTokens->hydraSceneRenderSettingsSrc.GetString() + "\n");

        _renderDelegate->SetRenderSetting(BatchRenderTokens->renderSettingsSrc, VtValue(BatchRenderTokens->hydraSceneRenderSettingsSrc));
    }

    // Support a USD stage providing the render settings through prims in the
    // Hydra scene.  Hydra Prman supports this when the
    // HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS=true environment variable
    // is set.
    _SetActiveRenderSettingsPrimFromScene();

    _initializationSucceeded = true;
}

// Perform a full reset of Hydra and remove the data producer scene indices.
void BatchRenderer::_ClearHydraResources()
{
    if (!_initializationAttempted) {
        return;
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("BatchRenderer::_ClearHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    // Only remove information for our dummy batch render viewport, to avoid
    // affecting interactive viewports.
    Fvp::RenderViewDataManager::Get().RemoveRenderViewData(kBatchRenderDummyPanelName);
    
    //Remove the data producer scene indices that apply to all views
    Fvp::DataProducerSceneIndexInterfaceImp::get().ClearDataProducerSceneIndicesThatApplyToAllViews();

    // Remove the scene index registry
    _sceneIndexRegistry.reset();

    _ClearMayaHydraSceneIndex();

    // Cleanup internal context data that keep references to data that is now
    // invalid.
    _engine.ClearTaskContextData();

    _taskController.reset();

    if (_renderIndex != nullptr) {
        GetMayaHydraLibInterface().UnregisterTerminalSceneIndex(_renderIndex->GetTerminalSceneIndex());
#ifndef CODE_COVERAGE_WORKAROUND
        // The render index destructor crashes under Windows clang code
        // coverage builds, so deletion is skipped in that configuration.
        delete _renderIndex;
#endif
        _renderIndex = nullptr;
    }

    if (_rendererPlugin != nullptr) {
        _renderDelegate = nullptr;
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(_rendererPlugin);
        _rendererPlugin = nullptr;
    }

    // Decrease ref count on the render index proxy which owns the merging scene index at the end of
    // this function as some previous calls may likely use it to remove some scene indices
    _dataProducerMergingSceneIndexProxy.reset();

    _viewport = GfVec4d(0, 0, 0, 0);
    _initializationSucceeded = false;
    _initializationAttempted = false;
}

void BatchRenderer::_CreateSceneIndicesChainAfterMergingSceneIndex()
{
    //This function is where happens the ordering of filtering scene indices that are after the merging scene index
    //We use as its input scene index : _inputSceneIndexOfFilteringSceneIndicesChain
    _lastFilteringSceneIndexBeforeCustomFiltering = _inputSceneIndexOfFilteringSceneIndicesChain;

    _lastFilteringSceneIndexBeforeCustomFiltering = _sceneGlobalsSceneIndex = HdsiSceneGlobalsSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering);
    TF_AXIOM(_mayaHydraSceneIndex);

#ifdef CODE_COVERAGE_WORKAROUND
    Fvp::leakSceneIndex(_lastFilteringSceneIndexBeforeCustomFiltering);
#endif
}

void BatchRenderer::_ClearHydraCallback(void* data)
{
    auto* instance = reinterpret_cast<BatchRenderer*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }
    instance->_ClearHydraResources();
}

HdRenderIndex* BatchRenderer::renderIndex() const
{
    return _renderIndex;
}

void BatchRenderer::_SetActiveRenderSettingsPrimFromScene()
{
    if (!TF_VERIFY(_sceneGlobalsSceneIndex, "Scene globals scene index not yet initialized")) {
        return;
    }

    const auto hydraRsPath = GetActiveRenderSettingsHydraPath();
    if (!TF_VERIFY(!hydraRsPath.IsEmpty(), 
                   "Invalid Hydra active render settings prim path.")) {
        return;
    }

    TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                 "Active render settings set to " +
                 hydraRsPath.GetAsString() + "\n");

    _sceneGlobalsSceneIndex->SetActiveRenderSettingsPrimPath(hydraRsPath);
}

}
