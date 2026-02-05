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

#include "pluginDebugCodes.h"
#include "renderSettingsUtils.h"

#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/sceneIndex/registration.h>
#include <mayaHydraLib/tokens.h>

#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif
#include <flowViewport/tokens.h>
#include <flowViewport/API/renderViewData/fvpRenderViewDataManager.h>
#include <flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>
#include <pxr/pxr.h>

#include <ufe/pathString.h>
#include <ufe/sceneItemList.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/selectionTracker.h>
#include <pxr/imaging/hdx/shadowTask.h>
#include <pxr/imaging/hdx/tokens.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>

#include <mayaUsdAPI/utils.h>

#include <maya/MMessage.h>
#include <maya/MProfiler.h>
#include <maya/MSceneMessage.h>
#include <maya/MFileIO.h>
#include <maya/MStatus.h>

#include <atomic>
#include <string>
#include <iostream>

int _batchRendererProfilerCategory = MProfiler::addCategory(
    "BatchRenderer (mayaHydra)",
    "Events from mayaHydra render override");

using namespace MayaHydra;
PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// TODO_BATCH_RENDER
// Fvp::RenderViewDataManager::AddRenderViewData
// connects a custom data producer scene index chain with the Hydra Flow
// Viewport Toolkit merging scene index, depending on the Hydra renderer.
// This should be changed for batch rendering.  For now, use a dummy panel
// to connect the scene index chain.

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

    std::cout << "PPT: _isUsingHdSt is " << (_isUsingHdSt ? "true" : "false")
              << std::endl;

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

    constexpr bool fullReset = true;
    ClearHydraResources(fullReset);

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

bool BatchRenderer::_PrepareHydraBatchRender(
    int width,
    int height,
    HdxRenderTaskParams* outParams)
{
    if (_initializationAttempted && !_initializationSucceeded) {
        // Initialization must have failed already, stop trying.
        return false;
    }

    if (_needsClear.exchange(false)) {
        constexpr bool fullReset = false;
        ClearHydraResources(fullReset);
    }

    if (!_initializationAttempted) {
        _InitHydraResources();
        if (!_initializationSucceeded) {
            return false;
        }
    }

    // TODO_BATCH_RENDER  Remove this viewport architecture dependency.
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

    if (_needToReplaceSelection) {
        _needToReplaceSelection = false;
    }

    MayaHydraParams delegateParams = _globals.delegateParams;
    delegateParams.displaySmoothMeshes = true; // This is the default.

    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->SetParams(delegateParams);
        // TODO_BATCH_RENDER
        // Maya Hydra scene index does way too much, viewport render aware.
        // How do we fix this?  PPT, 3-Mar-2025.
        // _mayaHydraSceneIndex->PreFrame(drawContext);
    }

    _viewport = GfVec4d(0, 0, width, height);
    _taskController->SetRenderViewport(_viewport);

    // Set Purpose tags.
    SetRenderPurposeTags(delegateParams);

    HdxRenderTaskParams params;
    params.enableLighting = true;
#if PXR_VERSION <= 2508
    params.enableSceneMaterials = true;
#endif
    params.cullStyle = HdCullStyleBackUnlessDoubleSided;

    if (outParams) {
        *outParams = params;
    }

    return true;
}

void BatchRenderer::_FinalizeHydraBatchRender(const HdxRenderTaskParams& params)
{
    _taskController->SetRenderParams(params);
    if (!params.camera.IsEmpty()) {
        _taskController->SetCameraPath(params.camera);
    }

    // Default color in usdview.
    _taskController->SetSelectionColor(_globals.colorSelectionHighlightColor);
    _taskController->SetEnableSelection(_globals.colorSelectionHighlight);

    if (_globals.outlineSelectionWidth != 0.f) {
        _taskController->SetSelectionOutlineRadius(_globals.outlineSelectionWidth);
        _taskController->SetSelectionEnableOutline(true);
    } else {
        _taskController->SetSelectionEnableOutline(false);
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

    // Set the initial selection onto the selection scene index later. 
    _needToReplaceSelection = true;

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

    // Support a USD stage providing the render settings through prims in the
    // Hydra scene.  At time of writing (5-Sep-2025) only Hydra Prman does, if
    // the  HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS=true environment
    // variable is used.  This is done through USD stage-level metadata that
    // points to a render settings prim in the USD scene.
    _SetActiveRenderSettingsPrimFromStageMetadata();

    _initializationSucceeded = true;
}

//When fullReset is true, we remove the data producer scene indices that apply to all views.
//It means you are doing a full reset of hydra such as when doing "File New".
//Use fullReset = false when you still want to see the previously registered data producer scene indices when using an hydra viewport.
void BatchRenderer::ClearHydraResources(bool fullReset)
{
    if (!_initializationAttempted) {
        return;
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("BatchRenderer::ClearHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    // TODO_BATCH_RENDER  Do not call 
    // Fvp::RenderViewDataManager::Get().RemoveAllViewportsInformation()
    // as this will affect interactive viewports.  Only remove
    // information for our dummy batch render viewport.  Must remove this
    // viewport architecture dependence.
    Fvp::RenderViewDataManager::Get().RemoveRenderViewData(kBatchRenderDummyPanelName);
    
    if (fullReset){
        //Remove the data producer scene indices that apply to all views
        Fvp::DataProducerSceneIndexInterfaceImp::get().ClearDataProducerSceneIndicesThatApplyToAllViews();
    }

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
        // Leak the render index, as its destructor crashes under Windows clang
        // code coverage build.
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
    constexpr bool fullReset = true;
    instance->ClearHydraResources(fullReset);
}

HdRenderIndex* BatchRenderer::renderIndex() const
{
    return _renderIndex;
}

void BatchRenderer::_SetActiveRenderSettingsPrimPath(const SdfPath& path)
{
    if (!TF_VERIFY(_sceneGlobalsSceneIndex, "Scene globals scene index not yet initialized")) {
        return;
    }
    _sceneGlobalsSceneIndex->SetActiveRenderSettingsPrimPath(path);
}

void BatchRenderer::_SetActiveRenderSettingsPrimFromStageMetadata()
{
    // Render settings can be selected via stage metadata. If not present,
    // traverse the stage to find a render settings prim.
    const Ufe::SceneItemList proxyShapes = GetAllMayaUsdProxyShapes();

    TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                 "Found %zu MayaUsdProxyShape nodes in scene.\n",
                 proxyShapes.size());

    // Get the USD stage from the first USD proxy shape node.
    if (!proxyShapes.empty()) {
        const auto ps = proxyShapes.front();
        const auto psPath = ps->path();
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                     Ufe::PathString::string(psPath) + 
                     " chosen to provide render settings.\n");

        const auto stage = MayaUsdAPI::getStage(psPath);
        if (TF_VERIFY(stage, "No stage found for proxy shape %s", Ufe::PathString::string(psPath).c_str())) {
            UsdRenderSettings usdRenderSettings;
            if (FindUsdRenderSettingsOnStage(stage, usdRenderSettings)) {
                const SdfPath rsPath = usdRenderSettings.GetPrim().GetPath();

                // Add the delegateId prefix since the scene globals scene index is
                // inserted into the merging scene index.
                // The proper way to do this is to call
                // MayaHydra::sceneIndexPathPrefix(), which correctly deals
                // with possible USD proxy shape name duplication in the Maya
                // Dag.  Unfortunately, at this point, the Hydra scene is not
                // fully populated.  This level of correctness is beyond the
                // scope of the production rendering POC.
                const auto hydraRsPath = rsPath.ReplacePrefix(
                    SdfPath::AbsoluteRootPath(),
                    SdfPath("/MayaUsdProxyShape_PluginNode").AppendElementString(ps->nodeName()));

                TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                             "Active render settings set to " +
                             hydraRsPath.GetAsString() + "\n");

                _SetActiveRenderSettingsPrimPath(hydraRsPath);
            } else {
                TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_RENDER_SETTINGS,
                             "No render settings prim found on stage.\n");
            }
        }
    }
}

}
