//
// Copyright 2019 Luma Pictures
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
// Copyright 2024 Autodesk, Inc. All rights reserved.
//


// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include "renderOverride.h"
#include "renderRegionCommand.h"
#include "setVisibleFramePassesCommand.h"

#include "mayaColorPreferencesTranslator.h"
#include "pluginDebugCodes.h"
#include "renderOverrideUtils.h"
#include "renderSettingsUtils.h"

#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/sceneIndex/registration.h>
#include <mayaHydraLib/sceneIndex/mhGenerativeProceduralResolvingSceneIndex.h>
#include <mayaHydraLib/pick/mhPickHit.h>
#include <mayaHydraLib/pick/mhPickHandler.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>

#include <mayaHydraLib/profilingUtils.h>
#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/tokens.h>

#include <flowViewport/fvpDirtyNotifier.h>
#include <flowViewport/tokens.h>
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <flowViewport/debugCodes.h>
#include <flowViewport/selection/fvpSelection.h>
#include <flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.h>
#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
#include <flowViewport/API/renderViewData/fvpIsolateSelectManager.h>
#include <flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h>
#include <flowViewport/fvpInstruments.h>
#endif
#include <flowViewport/API/renderViewData/fvpRenderViewDataManager.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>
#include <flowViewport/sceneIndex/fvpBBoxSceneIndex.h>
#include <flowViewport/sceneIndex/fvpReprSelectorSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPassFilteringSceneIndex.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/imageWriter/fvpImageBufferWriter.h>
#include <flowViewport/fvpPurposeRenderTagsForPasses.h>

#include <hvt/engine/framePass.h>
#include <hvt/engine/framePassUtils.h>
#include <hvt/engine/renderIndexProxy.h>
#include <hvt/engine/taskCreationHelpers.h>
#include <hvt/engine/viewportEngine.h>
#include <hvt/tasks/resources.h>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>

#include <ufe/camera.h>
#include <ufe/hierarchy.h>
#include <ufe/selection.h>
#include <ufe/namedSelection.h>
#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/observableSelection.h>
#include <ufe/globalSelection.h>
#include <ufe/selectionNotification.h>
#include <ufe/observer.h>

#include <ufeExtensions/Global.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/instantiateSingleton.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hd/renderBuffer.h>
#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/mesh.h>
#include <pxr/imaging/hd/basisCurves.h>
#include <pxr/imaging/hd/points.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/tokens.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/prim.h>

#include <pxr/usd/usd/modelAPI.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdLux/lightAPI.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

#include <mayaUsdAPI/proxyStage.h>

#include <maya/MDagPath.h>
#include <maya/M3dView.h>
#include <maya/MConditionMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/MEventMessage.h>
#include <maya/MGlobal.h>
#include <maya/MNodeMessage.h>
#include <maya/MAnimControl.h>
#include <maya/MObjectHandle.h>
#include <maya/MSceneMessage.h>
#include <maya/MSelectionList.h>
#include <maya/MTimerMessage.h>
#include <maya/MUiMessage.h>
#include <maya/MFnCamera.h>
#include <maya/MFileIO.h>
#include <maya/MTypes.h>

#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <limits>

#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/hashset.h>
#include "envSettings.h"

using namespace MayaHydra;

namespace {

PXR_NAMESPACE_USING_DIRECTIVE

const SdfPath MAYA_NATIVE_ROOT = SdfPath("/MayaHydraViewportRenderer");

inline bool isInComponentsPickingMode(const MHWRender::MSelectionInfo& selectInfo)
{
    return selectInfo.selectable(MSelectionMask::kSelectMeshVerts)
        || selectInfo.selectable(MSelectionMask::kSelectMeshEdges)
        || selectInfo.selectable(MSelectionMask::kSelectMeshFreeEdges)
        || selectInfo.selectable(MSelectionMask::kSelectMeshFaces)
        || selectInfo.selectable(MSelectionMask::kSelectVertices)
        || selectInfo.selectable(MSelectionMask::kSelectEdges)
        || selectInfo.selectable(MSelectionMask::kSelectFacets);
}

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
std::string getRenderingDestination(
    const MHWRender::MFrameContext* frameContext
)
{
    TF_AXIOM(frameContext);
    MString viewportId;
    frameContext->renderingDestination(viewportId);
    return std::string(viewportId.asChar());
}
#endif

inline Fvp::LightsManagementSceneIndex::LightingMode convertFromMayaLightingModeToFlowViewportLightMode(MFrameContext::LightingMode mayaLightingMode)
{
    switch (mayaLightingMode) {
        case MFrameContext::kLightDefault: return Fvp::LightsManagementSceneIndex::LightingMode::kDefaultLighting;
        case MFrameContext::kAmbientLight:
            TF_WARN("Ambient/Flat lighting mode is not supported");//Fall into next switch/case as we want to return kSceneLighting
        case MFrameContext::kSceneLights: return Fvp::LightsManagementSceneIndex::LightingMode::kSceneLighting;
        case MFrameContext::kSelectedLights:
            return Fvp::LightsManagementSceneIndex::LightingMode::kSelectedLightsOnly;
        case MFrameContext::kNoLighting: return Fvp::LightsManagementSceneIndex::LightingMode::kNoLighting;
        default: return Fvp::LightsManagementSceneIndex::LightingMode::kSceneLighting;
    }
}

}

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

namespace {

// Not sure if we actually need a mutex guarding _allInstances, but
// everywhere that uses it isn't a "frequent" operation, so the
// extra speed loss should be fine, and I'd rather be safe.
std::mutex                       _allInstancesMutex;
std::vector<MtohRenderOverride*> _allInstances;

//! \brief  Get the index of the hit nearest to a given cursor point.
int GetNearestHitIndex(
    const MHWRender::MFrameContext& frameContext,
    const PickHitVector&            hits,
    int                             cursor_x,
    int                             cursor_y)
{
    int nearestHitIndex = -1;

    double dist2_min = std::numeric_limits<double>::max();
    float  depth_min = std::numeric_limits<float>::max();

    for (unsigned int i = 0; i < hits.size(); i++) {
        const PickHit& hit = hits[i];
        const MPoint      worldSpaceHitPoint(
            hit.hdxPickHit.worldSpaceHitPoint[0], hit.hdxPickHit.worldSpaceHitPoint[1], hit.hdxPickHit.worldSpaceHitPoint[2]);

        // Calculate the (x, y) coordinate relative to the lower left corner of the viewport.
        double hit_x, hit_y;
        frameContext.worldToViewport(worldSpaceHitPoint, hit_x, hit_y);

        // Calculate the 2D distance between the hit and the cursor
        double dist_x = hit_x - (double)cursor_x;
        double dist_y = hit_y - (double)cursor_y;
        double dist2 = dist_x * dist_x + dist_y * dist_y;

        // Find the hit nearest to the cursor.
        if ((dist2 < dist2_min) || (dist2 == dist2_min && hit.hdxPickHit.normalizedDepth < depth_min)) {
            dist2_min = dist2;
            depth_min = hit.hdxPickHit.normalizedDepth;
            nearestHitIndex = (int)i;
        }
    }

    return nearestHitIndex;
}
} // namespace

class MtohRenderOverride::SelectionObserver : public Ufe::Observer
{
public:
    SelectionObserver(MtohRenderOverride& renderOverride)
        : Ufe::Observer(), _renderOverride(renderOverride)
    {}

    void operator()(const Ufe::Notification& notification) override
    {
        // During Maya file read, each node will be selected in turn, so we get
        // notified for each node in the scene.  Prune this out.
        if (MFileIO::isOpeningFile()) {
            return;
        }

        _renderOverride.SelectionChanged(
            dynamic_cast<const Ufe::SelectionChanged&>(notification));
    }

private:
    MtohRenderOverride& _renderOverride;
};

// MtohRenderOverride is a rendering override class for the viewport to use Hydra instead of VP2.0.
MtohRenderOverride::MtohRenderOverride(const MtohRendererDescription& desc)
    : MHWRender::MRenderOverride(desc.overrideName.GetText())
    , _rendererDesc(desc)
    , _sceneIndexRegistry(nullptr)
    , _globals(MtohRenderGlobals::GetInstance())
    , _hgi(Hgi::CreatePlatformDefaultHgi())
    , _hgiDriver { HgiTokens->renderDriver, VtValue(_hgi.get()) }
    , _fvpSelectionTracker(new Fvp::SelectionTracker)
    , _ufeSn(Ufe::NamedSelection::get("MayaSelectTool"))
    , _mayaSelectionObserver(std::make_shared<SelectionObserver>(*this))
    , _isUsingHdSt(desc.rendererName == MtohTokens->HdStormRendererPlugin)
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "MtohRenderOverride created (%s - %s - %s)\n",
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

    // Observe the UFE selection.
    auto sn = Ufe::GlobalSelection::get();
    TF_AXIOM(sn);
    sn->addObserver(_mayaSelectionObserver);

    // Setup the playblast watch.
    // _playBlasting is forced to true here so we can just use _PlayblastingChanged below
    //
    _playBlasting = true;
    MConditionMessage::addConditionCallback(
        "playblasting", &MtohRenderOverride::_PlayblastingChanged, this, &status);
    MtohRenderOverride::_PlayblastingChanged(false, this);

    _defaultLight.SetSpecular(GfVec4f(0.0f));
    _defaultLight.SetAmbient(GfVec4f(0.0f));

    {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        _allInstances.push_back(this);
    }

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    Fvp::Instruments::instance().set(kNbViewSelectedChangedCalls, VtValue(_nbViewSelectedChangedCalls));
#endif

    // Tell the Viewport Toolbox where to find its resources.
    std::filesystem::path pluginPath = MtohGetMayaHydraPluginLocation();
    // Go up 2 folders from plugin path and add /include/hvt/resources
    if (!pluginPath.empty()) {
        const std::filesystem::path resourcePath = pluginPath.parent_path().parent_path() / "include" / "hvt" / "resources";
        
        // Check if the resource path exists and warn if it doesn't
        if (!std::filesystem::exists(resourcePath)) {
            TF_WARN(
                "MayaHydra: Viewport Toolbox resource directory does not exist: %s",
                resourcePath.string().c_str());
        } else {
            hvt::SetResourceDirectory(resourcePath);
        }
    }
}

MtohRenderOverride::~MtohRenderOverride()
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "MtohRenderOverride destroyed (%s - %s - %s)\n",
            _rendererDesc.rendererName.GetText(),
            _rendererDesc.overrideName.GetText(),
            _rendererDesc.displayName.GetText());

    if (_mayaSelectionObserver) {
        if (auto sn = Ufe::GlobalSelection::get()) {
            sn->removeObserver(_mayaSelectionObserver);
        }
    }

    if (_timerCallback) {
        MMessage::removeCallback(_timerCallback);
    }

    if (_timeChangeCallback) {
        MMessage::removeCallback(_timeChangeCallback);
        _timeChangeCallback = 0;
    }

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    if (_viewSelectedChangedCb) {
        MMessage::removeCallback(_viewSelectedChangedCb);
    }
#endif

    constexpr bool fullReset = true;
    ClearHydraResources(fullReset);

    _operations.clear();

    MMessage::removeCallbacks(_callbacks);
    _callbacks.clear();
    for (auto& panelAndCallbacks : _renderPanelCallbacks) {
        MMessage::removeCallbacks(panelAndCallbacks.second);
    }

    if (!_allInstances.empty()) {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        _allInstances.erase(
            std::remove(_allInstances.begin(), _allInstances.end(), this), _allInstances.end());
    }
}

HdRenderDelegate* MtohRenderOverride::_GetRenderDelegate(int framePassIndex /*= 0*/)
{
    if (framePassIndex < 0 || framePassIndex >= static_cast<int> (_framePassesData.size())) {
        TF_CODING_ERROR("Invalid pass index: %d", framePassIndex);
        return nullptr;
    }
    
    const auto& renderIndexProxy = (_framePassesData[framePassIndex] && _framePassesData[framePassIndex]->HasRenderIndexProxy())
        ? _framePassesData[framePassIndex]->GetRenderIndexProxy()
        : nullptr;

    return renderIndexProxy ? renderIndexProxy->RenderIndex()->GetRenderDelegate() : nullptr;
}

HdRenderDelegate* MtohRenderOverride::_GetRenderDelegate(int framePassIndex /*= 0*/) const
{
    if (framePassIndex < 0 || framePassIndex >= static_cast<int> (_framePassesData.size())) {
        TF_CODING_ERROR("Invalid pass index: %d", framePassIndex);
        return nullptr;
    }

    const auto& renderIndexProxy = (_framePassesData[framePassIndex] && _framePassesData[framePassIndex]->HasRenderIndexProxy())
        ? _framePassesData[framePassIndex]->GetRenderIndexProxy()
        : nullptr;

    return renderIndexProxy ? renderIndexProxy->RenderIndex()->GetRenderDelegate()
        : nullptr;
}

void MtohRenderOverride::UpdateRenderGlobals(
    const MtohRenderGlobals& globals,
    const TfToken&           attrName)
{
    // If no attribute or attribute starts with 'mayaHydra', these setting wil be applied on the
    // next call to MtohRenderOverride::Render, so just force an invalidation
    // XXX: This will need to change if mayaHydra settings should ever make it to the delegate
    // itself.

    // If the attribute name does not start with mayaHydra, or mayaHydra is
    // not found.
    if (attrName.GetString().find("mayaHydra") != 0) {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        for (auto* instance : _allInstances) {
            const auto& rendererName = instance->_rendererDesc.rendererName;

            // If no attrName or the attrName is the renderer, then update everything
            const size_t attrFilter = (attrName.IsEmpty() || attrName == rendererName) ? 0 : 1;
            if (attrFilter && !instance->_globals.AffectsRenderer(attrName, rendererName)) {
                continue;
            }

            // Will be applied in _InitHydraResources later anyway
            if (auto* renderDelegate = instance->_GetRenderDelegate()) {
                instance->_globals.ApplySettings(
                    renderDelegate,
                    instance->_rendererDesc.rendererName,
                    TfTokenVector(attrFilter, attrName));
                if (attrFilter) {
                    break;
                }
            }
        }
    } 
    else {
        if (attrName.GetString().find("Purpose") != 0) {
            //One of the render purpose attributes just changed
            //Get purpose render tag from attribute name
            const PXR_NS::TfToken purposeRenderTag
                = RenderGlobalsUtils::GetPurposeRenderTagFromAttrName(attrName);

            if (!purposeRenderTag.IsEmpty()) {
                std::lock_guard<std::mutex> lock(_allInstancesMutex);
                for (auto* instance : _allInstances) {
                    Fvp::FramePassDataPtrVector& framePassDataArray
                        = instance->_framePassesData;
                    for (auto& framePassData : framePassDataArray) {
                        if (framePassData && framePassData->IsValid()) {
                            auto params = instance->_globals.delegateParams;
                            framePassData->_renderTagsUpdateFn(params.renderPurpose, params.proxyPurpose, params.guidePurpose);
                            framePassData->DirtyPrimsFromPurposeRenderTag(purposeRenderTag);
                        }
                    }
                }
            }
        }
    }

    // Less than ideal still
    MGlobal::executeCommandOnIdle("refresh -f");
}

VtValue MtohRenderOverride::_GetUsedGPUMemory() const
{
    // Currently, only Storm is the known/tested renderer that provides GPU stats
    // via the Render Delegate.
    HdRenderDelegate* renderDelegate = _GetRenderDelegate();
    if (_isUsingHdSt && renderDelegate)
    {
        VtDictionary hdStRenderStat = renderDelegate->GetRenderStats();
        return hdStRenderStat[HdPerfTokens->gpuMemoryUsed.GetString()];
    }
    return VtValue();
}

int MtohRenderOverride::GetUsedGPUMemory()
{
    size_t totalGPUMemory = 0;
    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        const VtValue usedGpuMemory = instance->_GetUsedGPUMemory();
        if (usedGpuMemory.IsHolding<size_t>()) {
            totalGPUMemory += usedGpuMemory.UncheckedGet<size_t>();
        } else if (usedGpuMemory.IsHolding<int>()) {
            totalGPUMemory += usedGpuMemory.UncheckedGet<int>();
        }
    }
    return static_cast<int>(totalGPUMemory / (1024u * 1024u));
}

std::map<std::string, int> MtohRenderOverride::GetSceneStatistics()
{
    std::map<std::string, int> stats = {
        {"primitives", 0},
        {"mesh", 0},
        {"mesh.points", 0},
        {"mesh.faces", 0},
        {"curve", 0},
        {"curve.points", 0},
        {"point", 0},
    };

    MtohRenderOverride* instance = nullptr;
    {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        for (auto* inst : _allInstances) {
            if (inst->_initializationSucceeded && inst->renderIndex()) {
                instance = inst;
                break;
            }
        }
    }

    if (!instance || !instance->renderIndex()) {
        return stats;
    }

    // Get stats for all passes, avoiding double-counting when passes share the same render index
    const int numFramePasses = instance->_GetNumFramePasses();
    
    // Track which prims we've already counted to avoid double-counting
    std::set<SdfPath> seenPrims;
    
    // We are going to get the prims from all passes, but avoid double counting
    // the geometry (topology/verts) is defined in only one of the passes
    for (int i = 0; i < numFramePasses; ++i) { 
        auto* renderIndex = instance->renderIndex(i);
        if (!renderIndex) {
            continue;
        }
        
        auto primIds = renderIndex->GetRprimIds();
        
        for (const auto& primId : primIds) {
            auto* rprim = renderIndex->GetRprim(primId);
            if (!rprim) {
                continue;
            }

            // Check if we've already counted this prim
            bool isNewPrim = (seenPrims.find(primId) == seenPrims.end());
            if (isNewPrim) {
                seenPrims.insert(primId);
                stats["primitives"]++;
            }

            auto* mesh = dynamic_cast<const HdMesh*>(rprim);
            if (mesh) {
                if (isNewPrim) {
                    stats["mesh"]++;
                }
                auto sceneIndexPrim = renderIndex->GetTerminalSceneIndex()->GetPrim(primId);
                auto meshSchema = HdMeshSchema::GetFromParent(sceneIndexPrim.dataSource);
                if (meshSchema.IsDefined()) {
                    auto meshTopology = meshSchema.GetTopology();
                    if (meshTopology.IsDefined()) {
                        auto faceVertexCounts = meshTopology.GetFaceVertexCounts();
                        auto faceVertexIndices = meshTopology.GetFaceVertexIndices();
                        if (faceVertexCounts && faceVertexIndices) {
                            auto counts = faceVertexCounts->GetTypedValue(0.0f);
                            auto indices = faceVertexIndices->GetTypedValue(0.0f);
                            stats["mesh.faces"] += counts.size();
                            if (!indices.empty()) {
                                int maxIndex = *std::max_element(indices.begin(), indices.end());
                                stats["mesh.points"] += maxIndex + 1;
                            }
                        }
                    }

                }
                continue;
            }

            auto* curves = dynamic_cast<const HdBasisCurves*>(rprim);
            if (curves) {
                if (isNewPrim) {
                    stats["curve"]++;
                }
                auto sceneIndexPrim = renderIndex->GetTerminalSceneIndex()->GetPrim(primId);
                auto curvesSchema = HdBasisCurvesSchema::GetFromParent(sceneIndexPrim.dataSource);
                if (curvesSchema.IsDefined()) {
                    auto curvesTopology = curvesSchema.GetTopology();
                    if (curvesTopology.IsDefined()) {
                        auto curveIndices = curvesTopology.GetCurveIndices();
                        if (curveIndices) {
                            auto indices = curveIndices->GetTypedValue(0.0f);
                            if (!indices.empty()) {
                                int maxIndex = *std::max_element(indices.begin(), indices.end());
                                stats["curve.points"] += maxIndex + 1;
                            }
                        }
                    }
                }
                continue;
            }

            auto* points = dynamic_cast<const HdPoints*>(rprim);
            if (points) {
                if (isNewPrim) {
                    stats["point"]++;
                }
                continue;
            }
        }
    }
    return stats;
}

std::vector<MString> MtohRenderOverride::AllActiveRendererNames()
{
    std::vector<MString> renderers;

    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        if (instance->_initializationSucceeded) {
            renderers.push_back(instance->_rendererDesc.rendererName.GetText());
        }
    }
    return renderers;
}

TfTokenVector MtohRenderOverride::GetAvailableFramePassAovs(int passIndex)
{
    TfTokenVector aovs;

    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        if (instance->_initializationSucceeded
            && passIndex < static_cast<int>(instance->_framePassesData.size())) {
            // Can't rely on UsdImagingGLEngine::GetRenderAovs() as creating a temp UsdImagingGLEngine with same hgi 
            // may interfere with the current renderer, just copy the same implementation here
            TfTokenVector currAovs;
            const auto    renderIndex = instance->renderIndex(passIndex);
            HdRenderDelegate* renderDelegate
                = renderIndex ? renderIndex->GetRenderDelegate() : nullptr;
            if (renderIndex && renderDelegate
                && renderIndex->IsBprimTypeSupported(HdPrimTypeTokens->renderBuffer)) {

                static const TfToken candidates[] = { HdAovTokens->primId,
                                                      HdAovTokens->depth,
                                                      HdAovTokens->normal,
#if PXR_VERSION > 2411
                                                      HdAovTokens->Neye,
#endif
                                                      HdAovTokensMakePrimvar(TfToken("st")) };

                currAovs = { HdAovTokens->color };
                for (auto const& aov : candidates) {
                    if (renderDelegate->GetDefaultAovDescriptor(aov).format
                        != HdFormatInvalid) {
                        currAovs.push_back(aov);
                    }
                }
            }
            aovs.insert(aovs.end(), currAovs.begin(), currAovs.end());
        }
    }
    return aovs;
}

SdfPathVector MtohRenderOverride::RendererRprims(TfToken rendererName, bool visibleOnly)
{
    MtohRenderOverride* instance = GetByName(rendererName);
    if (!instance) {
        return SdfPathVector();
    }

    // We need to find the right render index from a framePassData and get its RPrims.
    SdfPathVector primIds;
    const int     numFramePassesData = static_cast<int>(instance->_framePassesData.size());
    for (int i = 0; i < numFramePassesData; ++i) {
        const auto& framePassData = instance->_framePassesData[i];
        if (!framePassData) {
            continue;
        }
        
        const std::string& rendererNameFromPass = framePassData->_rendererName.GetString();
        if (rendererName != rendererNameFromPass){
            continue;
        }

        auto* renderIndex = (framePassData->_renderIndexProxy)
            ? framePassData->_renderIndexProxy->RenderIndex()
            : nullptr;
        if (!renderIndex) {
            continue;
        }
        
        //Do a copy as we may remove some of them
        SdfPathVector tempPrimIds = renderIndex->GetRprimIds();
        if (visibleOnly) {
            tempPrimIds.erase(
                std::remove_if(
                    tempPrimIds.begin(),
                    tempPrimIds.end(),
                    [renderIndex](const SdfPath& primId) {
                        auto* rprim = renderIndex->GetRprim(primId);
                        if (!rprim)
                            return true;
                        return !rprim->IsVisible();
                    }),
                tempPrimIds.end());
        }

        // Concatenate results
        if (tempPrimIds.size()) { 
            primIds.reserve(
                primIds.size() + tempPrimIds.size()); // Reserve space to avoid reallocations
            primIds.insert(
                primIds.end(), tempPrimIds.begin(), tempPrimIds.end()); // Insert all elements
        }
    }

    // Sort them by lexicographically order
    std::sort(primIds.begin(), primIds.end(), std::less<SdfPath>());
    return primIds;
}

SdfPath MtohRenderOverride::RendererSceneDelegateId(TfToken rendererName, TfToken sceneDelegateName)
{
    MtohRenderOverride* instance = GetByName(rendererName);
    if (!instance) {
        return SdfPath();
    }

    if (instance->_mayaHydraSceneIndex) {
        return instance->_mayaHydraSceneIndex->GetDelegateID(sceneDelegateName);
    }
    return SdfPath();
}

bool MtohRenderOverride::HasConverged(TfToken rendererName)
{
    MtohRenderOverride* instance = GetByName(rendererName);
    if (!instance) {
        return false;
    }
    return instance->_isConverged;
}

void MtohRenderOverride::_DetectMayaDefaultLighting(const MHWRender::MDrawContext& drawContext)
{
    constexpr auto considerAllSceneLights = MHWRender::MDrawContext::kFilteredIgnoreLightLimit;

    const auto numLights = drawContext.numberOfActiveLights(considerAllSceneLights);
    auto       foundMayaDefaultLight = false;
    if (numLights == 1) {
        auto* lightParam = drawContext.getLightParameterInformation(0, considerAllSceneLights);
        if (lightParam != nullptr && !lightParam->lightPath().isValid()) {
            // This light does not exist so it must be the
            // default maya light
            MFloatPointArray positions;
            MFloatVector     direction;
            auto             intensity = 0.0f;
            MColor           color;
            auto             hasDirection = false;
            auto             hasPosition = false;

            // Maya default light has no position, only direction
            drawContext.getLightInformation(
                0,
                positions,
                direction,
                intensity,
                color,
                hasDirection,
                hasPosition,
                considerAllSceneLights);

            if (hasDirection && !hasPosition) {

#if defined(HD_API_VERSION) && HD_API_VERSION >= 74 // For USD 24.11+
                intensity /= M_PI;//Is a HdPrimTypeTokens->simpleLight
#endif

                // Note for devs : if you update more parameters in the default light, don't forget
                // to update MtohDefaultLightDelegate::SetDefaultLight and MayaViewportSceneIndex::SetDefaultLight, currently there are only 3 :
                // position, diffuse, specular
                GfVec3f position;
                GetDirectionalLightPositionFromDirectionVector(position, {direction.x, direction.y, direction.z});
                _defaultLight.SetPosition({ position.data()[0], position.data()[1], position.data()[2], 0.0f });
                _defaultLight.SetDiffuse(
                    { intensity * color.r, intensity * color.g, intensity * color.b, 1.0f });
                _defaultLight.SetSpecular(
                    { intensity * color.r, intensity * color.g, intensity * color.b, 1.0f });
                foundMayaDefaultLight = true;
            }
        }
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_DEFAULT_LIGHTING)
        .Msg(
            "MtohRenderOverride::"
            "_DetectMayaDefaultLighting() "
            "foundMayaDefaultLight=%i\n",
            foundMayaDefaultLight);

    if (foundMayaDefaultLight != _hasDefaultLighting) {
        _hasDefaultLighting = foundMayaDefaultLight;
        TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_DEFAULT_LIGHTING)
            .Msg(
                "MtohRenderOverride::"
                "_DetectMayaDefaultLighting() clearing! "
                "_hasDefaultLighting=%i\n",
                _hasDefaultLighting);
    }
}

MStatus MtohRenderOverride::Render(
    const MHWRender::MDrawContext&                         drawContext,
    const MHWRender::MDataServerOperation::MViewportScene& scene)
{
    // It would be good to clear the resources of the overrides that are
    // not in active use, but I'm not sure if we have a better way than
    // the idle time we use currently. The approach below would break if
    // two render overrides were used at the same time.
    // for (auto* override: _allInstances) {
    //     if (override != this) {
    //         override->ClearHydraResources();
    //     }
    // }
    MH_PROFILE_FUNCTION();
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER).Msg("MtohRenderOverride::Render()\n");
    // We can use the mayaHydraSetVisibleFramePasses command to set the visible passes

    auto renderFrame = [&](bool markTime = false) {
        MH_PROFILE_SCOPE("MtohRenderOverride::Render renderFrame lambda");
        if (scene.changed()) {
            if (_mayaHydraSceneIndex) {
                _mayaHydraSceneIndex->UpdateRenderItems(scene);
            }
        }

        if (_mayaViewportSceneIndex) {
            _mayaViewportSceneIndex->Update(drawContext);
        }

        // Update shadow collection for lights
        if (_mayaHydraSceneIndex) {
            _mayaHydraSceneIndex->UpdateLightsShadowCollection();
        }

        // Update plugin data producers
        for (auto& viewportData : Fvp::RenderViewDataManager::Get().GetAllViewData()) {
            for (auto& dataProducer : viewportData.GetDataProducerSceneIndicesData()) {
                dataProducer->UpdateVisibility();
                dataProducer->UpdateTransform();
            }
        }

        //Apply any pending update from MayaUsd proxy shape nodes
        _sceneIndexRegistry->ApplyPendingUpdates();

        // Update plugin filtering scene indices
        std::string rendererNamesToUpdate;
        for (auto& sceneFilteringSceneIndexData :
             Fvp::FilteringSceneIndexInterfaceImp::get().getSceneFilteringSceneIndicesData()) {
            if (sceneFilteringSceneIndexData->UpdateVisibility()) {
                rendererNamesToUpdate
                    += sceneFilteringSceneIndexData->GetClient()->getRendererNames();
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

        const MIntArray& framePassesVisible    = 
            MayaHydraSetVisibleFramePasses::getVisibleFramePasses();
        int                 numVisibleFramePasses = framePassesVisible.length();
        const MString visibleAOVName = MayaHydraSetVisibleFramePasses::getAovName();
        const int           numFramePasses     = _GetNumFramePasses();
        if (numVisibleFramePasses > numFramePasses) {
            numVisibleFramePasses
                = numFramePasses;
        }

        // Iterate over visible passes
        for (int visibleIdx = 0; visibleIdx < numVisibleFramePasses; ++visibleIdx) {
            MH_PROFILE_SCOPE("MayaHydra frame pass");
            const int actualPassIndex = framePassesVisible[visibleIdx]; // Get the actual pass index
            const hvt::FramePassPtr& currentPass = _GetFramePass(actualPassIndex);
            if (!currentPass) {
                continue;
            }

            const bool isPass0              = (actualPassIndex == 0);
            const bool isFirstVisiblePass   = (visibleIdx == 0);
            const bool isLastVisiblePass    = (visibleIdx == numVisibleFramePasses - 1);//Put me back later

            // Clear background for the first visible pass only
            currentPass->params().clearBackgroundColor = isFirstVisiblePass;
            currentPass->params().clearBackgroundDepth = isFirstVisiblePass;

            // Enable presentation for the last visible pass only
            currentPass->params().enablePresentation = isLastVisiblePass;
            // Set the AOV to visualize
            // Per HVT, the AOV to visualize must be set on the last visible pass, and HdAovTokens->color must be set on other passes
            TfToken visualizeAOV = HdAovTokens->color;
            if (isLastVisiblePass) {
                const TfToken aovName = TfToken(visibleAOVName.asChar());
                // Can't rely on GetRenderBuffer(aovName) here as the AOV may not have been created yet
                const auto renderDelegate = _GetRenderDelegate(actualPassIndex);
                const bool aovNameExists = renderDelegate
                    ? renderDelegate->GetDefaultAovDescriptor(aovName).format != HdFormatInvalid
                    : false;
                visualizeAOV = (aovNameExists) ? aovName : HdAovTokens->color;
            }
            currentPass->params().visualizeAOV = visualizeAOV;

            if (visibleIdx > 0) {
                currentPass->params().renderParams.depthBiasEnable = true;
                currentPass->params().renderParams.depthBiasUseDefault = false;
                currentPass->params().renderParams.depthBiasConstantFactor = -1.0f;
                currentPass->params().renderParams.depthBiasSlopeFactor = -1.0f;
            }

            if (isPass0) {
                // Do not share the AOVs, for the first pass only
                HdTaskSharedPtrVector passTasks = currentPass->GetRenderTasks();
#if PXR_VERSION >= 2605
                if (_sceneGlobalsSceneIndex) {
                    const SdfPath& cameraPath = currentPass->params().renderParams.camera;
                    if (!cameraPath.IsEmpty()) {
                        _sceneGlobalsSceneIndex->SetPrimaryCameraPrimPath(cameraPath);
                    }
                }
#endif
                
                /*Debug code left here if needed later
                hvt::FramePass& framePassToDebug = *currentPass;
                std::ostringstream    content;
                content << framePassToDebug;
                std::string framePassParameters = content.str();
                OutputDebugStringA("Main Frame Pass parameters:");
                OutputDebugStringA(framePassParameters.c_str());
                */

                currentPass->Render(passTasks);
            } else {
                // Share AOVs from the previous visible pass or pass0
                const int previousPassIndex = (visibleIdx > 0) ?framePassesVisible[visibleIdx - 1] : 0;
                hvt::FramePassPtr& previousPass = _GetFramePass(previousPassIndex);
                if (previousPass) {
                    const hvt::RenderBufferBindings inputAOVs = previousPass->GetRenderBufferBindingsForNextPass(
                            { PXR_NS::HdAovTokens->color, PXR_NS::HdAovTokens->depth }
                    );
                    HdTaskSharedPtrVector passTasks = currentPass->GetRenderTasks(inputAOVs);
#if PXR_VERSION >= 2605
                    if (_sceneGlobalsSceneIndex) {
                        const SdfPath& cameraPath = currentPass->params().renderParams.camera;
                        if (!cameraPath.IsEmpty()) {
                            _sceneGlobalsSceneIndex->SetPrimaryCameraPrimPath(cameraPath);
                        }
                    }
#endif

                    /*Debug code left here if needed later
                    hvt::FramePass& framePassToDebug = *currentPass;
                    std::ostringstream content;
                    content << framePassToDebug;
                    std::string framePassParameters = content.str();
                    OutputDebugStringA("Second Frame Pass parameters:");
                    OutputDebugStringA(framePassParameters.c_str());
                    */
                    currentPass->Render(passTasks);
                }
            }
        }

        const auto fileName = Fvp::ImageBufferWriter::GetFileName();
        if (!fileName.empty()) {
            if (!Fvp::ImageBufferWriter::Write(_fileWriterArgs, fileName)) {
                TF_RUNTIME_ERROR("Failed to write image to %s",
                                 fileName.c_str());
            }
        }

        // Check convergence from the tasks first
        _isConverged = true;
        for (int visibleIdx = 0; visibleIdx < numVisibleFramePasses; ++visibleIdx) {
            const int i = framePassesVisible[visibleIdx]; // Get the actual pass index
            const hvt::FramePassPtr& currentPass = _GetFramePass(i);
            if (!currentPass) {
                continue;
            }

            if (!currentPass->IsConverged()) {
                _isConverged = false;
                break;
            }
        }

        if (!_isConverged) {
            // Check with AOVs as a second step, as some renderers may not properly set convergence
            // on the tasks
            // Check if all AOVs are converged for each visible pass.
            // Get the AOVs and check each render buffer's IsConverged().
            // Note: For Arnold, _taskController->IsConverged() always returns false; we must check
            // each AOV buffer directly.
            auto isConverged = [this, &framePassesVisible, numVisibleFramePasses]() {
                for (int visibleIdx = 0; visibleIdx < numVisibleFramePasses; ++visibleIdx) {
                    const int                i = framePassesVisible[visibleIdx];
                    const hvt::FramePassPtr& currentPass = _GetFramePass(i);
                    if (!currentPass) {
                        continue;
                    }

                    const auto* bufferManager = currentPass->GetRenderBufferManager().get();
                    if (!bufferManager) {
                        return false;
                    }

                    TfTokenVector renderOutputs = bufferManager->GetRenderOutputs();
                    if (renderOutputs.empty()) {
                        renderOutputs = GetAvailableFramePassAovs(i);
                    }
                    if (renderOutputs.empty()) {
                        TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER)
                            .Msg("RenderOutputs list is empty; assuming not converged.\n");
                        return false;
                    }

                    for (const TfToken& aovToken : renderOutputs) {
                        HdRenderBuffer* buffer = currentPass->GetRenderBuffer(aovToken);
                        if (!buffer) {
                            TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER)
                                .Msg("Render output '%s' not found; assuming not converged.\n",
                                     aovToken.GetText());
                            return false;
                        }
                        if (!buffer->IsConverged()) {
                            return false;
                        }
                    }
                }
                return true;
            };

            _isConverged = isConverged();
        }

        if (markTime) {
            std::lock_guard<std::mutex> lock(_lastRenderTimeMutex);
            _lastRenderTime = std::chrono::system_clock::now();
        }
    };
    if (_initializationAttempted && !_initializationSucceeded) {
        // Initialization must have failed already, stop trying.
        return MStatus::kFailure;
    }

    _DetectMayaDefaultLighting(drawContext);
    if (_needsClear.exchange(false)) {
        constexpr bool fullReset = false;
        ClearHydraResources(fullReset);
    }

    const unsigned int currentDisplayStyle = drawContext.getDisplayStyle();
    MayaHydraParams    delegateParams = _globals.delegateParams;
    delegateParams.displaySmoothMeshes
        = !(currentDisplayStyle & MHWRender::MFrameContext::kFlatShaded);

    if (!_initializationAttempted) {
        _InitHydraResources(drawContext, delegateParams);

        if (!_initializationSucceeded) {
            return MStatus::kFailure;
        }
    }

    _SetRenderPurposeTags(delegateParams);
   
    MFrameContext::LightingMode currentMayaLightingMode = MFrameContext::LightingMode::kSceneLights;

    //This code with strings comparison will go away if we have multiple render proxies when doing multi viewports
    MString panelName;
    std::string panelNameStr;
    auto framecontext = getFrameContext();
    if (framecontext){
        framecontext->renderingDestination(panelName);
        panelNameStr = std::string(panelName.asChar());

        TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_SCENE_INDEX_CHAIN_MGMT)
            .Msg("Rendering destination is %s\n", panelName.asChar());

        currentMayaLightingMode = framecontext->getLightingMode();

        auto& manager = Fvp::RenderViewDataManager::Get();
        if (false == manager.ViewIsAlreadyRegistered(panelNameStr)){
            //Create a HydraViewportInformation
            const Fvp::InformationInterface::RenderViewDesc hydraViewportInformation(panelNameStr, true);
            const bool dataProducerSceneIndicesAdded = manager.AddRenderViewData(
                hydraViewportInformation,
                renderIndex(),
                _dataProducerMergingSceneIndexProxy,
                _CreatePassFilteringSceneIndex(_framePassesData[0])
            );

            //Update the selection since we have added data producer scene indices through manager.AddViewportInformation to the merging scene index
            if (dataProducerSceneIndicesAdded && _selectionSceneIndex){
                _needToReplaceSelection = true;
            }
            //Update the leadObjectTacker in case it could not find the current lead object which could be in a custom data producer scene index or a maya usd proxy shape scene index
            if (_leadObjectPathTracker){
                _leadObjectPathTracker->updatePrimSelections();
            }
        }
    }

    if (_needToReplaceSelection){
        _selectionSceneIndex->ReplaceSelection(*Ufe::GlobalSelection::get());
        _needToReplaceSelection = false;
    }

    const bool currentUseDefaultMaterial = (drawContext.getDisplayStyle() & MHWRender::MFrameContext::kDefaultMaterial);

    if (_lightsManagementSceneIndex && _lightingMode != currentMayaLightingMode) {
        _lightsManagementSceneIndex->SetLightingMode(convertFromMayaLightingModeToFlowViewportLightMode(currentMayaLightingMode));
        _lightingMode = currentMayaLightingMode;
        _hasDefaultLighting = (MFrameContext::kLightDefault == _lightingMode);//Update default lighting
    }

    if (_mayaViewportSceneIndex) {
        _mayaViewportSceneIndex->SetDefaultLightEnabled(_hasDefaultLighting);
        _mayaViewportSceneIndex->SetDefaultLight(_defaultLight);
    }

    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->SetParams(delegateParams);
        _mayaHydraSceneIndex->FlushPendingUpdates();
    }

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
        // Make sure the isolate selection scene index set to the proper
        // isolate selection.  We currently have a single scene index tree,
        // thus a single isolate select scene index is common to and
        // provides prims to render all viewports.
        auto& isolateSelectMgr = Fvp::IsolateSelectManager::Get();
        auto isSi = isolateSelectMgr.GetIsolateSelectSceneIndex();
        auto isolateSelection = isolateSelectMgr.GetOrCreateIsolateSelection(panelNameStr);
        if (isSi && (isSi->GetIsolateSelection() != isolateSelection)) {
            TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_SCENE_INDEX_CHAIN_MGMT)
                .Msg("Switching scene index to isolate selection %p\n", &*isolateSelection);
            // Isolate select scene index is being switched to a different
            // viewport, set its isolate selection.
            isSi->SetViewport(panelNameStr, isolateSelection);
        }
        else {
            // This case includes disabled (null pointer) isolate selection.
            TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_SCENE_INDEX_CHAIN_MGMT)
                .Msg("Re-using isolate selection %p\n", (isolateSelection ? &*isolateSelection : (void*) 0));
        }
#endif

    if (_displayStyleSceneIndex) {
       _displayStyleSceneIndex->SetRefineLevel(delegateParams.refineLevel);
    }

    // Update "Show" menu filters
    {
        auto objectExclusions = framecontext->objectTypeExclusions();

        static const TfTokenVector polygonFilters = {
            FvpPruningTokens->meshes,
            FvpPruningTokens->capsules,
            FvpPruningTokens->cones,
            FvpPruningTokens->cubes,
            FvpPruningTokens->cylinders,
            FvpPruningTokens->spheres
        };
        static const std::map<MUint64, TfTokenVector> mayaFiltersToFvpPruningTokens = {
            { MHWRender::MFrameContext::kExcludeMeshes, polygonFilters },
            { MHWRender::MFrameContext::kExcludeNurbsCurves, {FvpPruningTokens->nurbsCurves} },
            { MHWRender::MFrameContext::kExcludeNurbsSurfaces, {FvpPruningTokens->nurbsPatches} }
        };

        for (auto [mayaFilter, fvpPruningTokens] : mayaFiltersToFvpPruningTokens) {
            for (const auto& fvpPruningToken : fvpPruningTokens) {
                _pruningSceneIndex->SetFilterStatus(fvpPruningToken, objectExclusions & mayaFilter);
            }
        }
    }

    // Toggle textures in the material network
    const unsigned int currentDisplayMode = drawContext.getDisplayStyle();
    bool isTextured = currentDisplayMode & MHWRender::MFrameContext::kTextured;
    if (_pruneTexturesSceneIndex &&
        _currentlyTextured != isTextured) {
        const bool pruneTextures = !isTextured;
        _pruneTexturesSceneIndex->MarkTexturesDirty(pruneTextures);
        _currentlyTextured = isTextured;
    }

    if (_defaultMaterialSceneIndex && _useDefaultMaterial != currentUseDefaultMaterial){
        _defaultMaterialSceneIndex->Enable(currentUseDefaultMaterial);
        _useDefaultMaterial = currentUseDefaultMaterial;
    }

    // Are we using Bounding Box display style ?
    const bool usingBBoxMode = (currentDisplayStyle & MHWRender::MFrameContext::kBoundingBox) != 0;
    _bboxSceneIndex->Enable(usingBBoxMode);
    
    // Set Required Hydra Repr (Wireframe/WireframeOnShaded/Shaded)
    // Hydra supports Wireframe and WireframeOnSurfaceRefined repr for wireframe on shaded mode.
    // Refinement level for Hydra is set in Hydra Render Globals
    const MFrameContext::WireOnShadedMode wireOnShadedMode = MFrameContext::wireOnShadedMode();//Get the user preference
    if ( (_reprSelectorSceneIndex && (currentDisplayStyle != _oldDisplayStyle) ) || (delegateParams.refineLevel != _oldRefineLevel)){
        if( (currentDisplayStyle & MHWRender::MFrameContext::kWireFrame) &&
            ((currentDisplayStyle & MHWRender::MFrameContext::kGouraudShaded) ||
            (currentDisplayStyle & MHWRender::MFrameContext::kTextured)) ) {
                // Wireframe on top of shaded
                if (MFrameContext::WireOnShadedMode::kWireframeOnShadedFull == wireOnShadedMode) {
                    _reprSelectorSceneIndex->SetReprType(Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeOnSurfaceRefined,
                                                         /*needsReprChanged=*/true, delegateParams.refineLevel);
                } else {
                    _reprSelectorSceneIndex->SetReprType(Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeOnSurface,
                                                         /*needsReprChanged=*/true, delegateParams.refineLevel);
                }
            }
            else if( (currentDisplayStyle & MHWRender::MFrameContext::kWireFrame) ) {
                    //wireframe only, not on top of shaded
                    _reprSelectorSceneIndex->SetReprType(Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeRefined,
                                                         /*needsReprChanged=*/true, delegateParams.refineLevel);
                }
            else // Shaded mode
                _reprSelectorSceneIndex->SetReprType(Fvp::ReprSelectorSceneIndex::RepSelectorType::Default,
                                                     /*needsReprChanged=*/false, delegateParams.refineLevel);

        _oldRefineLevel = delegateParams.refineLevel;
    }

    // Set MSAA as per Maya AntiAliasing settings
    const bool isMultiSampled
        = framecontext->getPostEffectEnabled(MHWRender::MFrameContext::kAntiAliasing);

    auto viewMatrix
        = GetGfMatrixFromMaya(drawContext.getMatrix(MHWRender::MFrameContext::kViewMtx));
    auto projectionMatrix
        = GetGfMatrixFromMaya(drawContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx));

    // Apply some settings
    const int numFramePasses = _GetNumFramePasses();
    for (int i = 0; i < numFramePasses; ++i) {
        const hvt::FramePassPtr& currentPass = _GetFramePass(i);
        if (!currentPass) {
            continue;
        }

        currentPass->params().enableMultisampling               = isMultiSampled;
        currentPass->params().viewInfo.viewMatrix               = viewMatrix;
        currentPass->params().viewInfo.projectionMatrix         = projectionMatrix;
        currentPass->params().selectionColor                    = _globals.colorSelectionHighlightColor;// Default color in usdview.
        currentPass->params().enableSelection                   = _globals.colorSelectionHighlight;
        currentPass->params().collection                        = _renderCollection; // Same collection for all passes
    }

    GfVec4f wireframeSelectionColor;
    if (Fvp::ColorPreferences::getInstance().getColor(
            FvpColorPreferencesTokens->wireframeSelection, wireframeSelectionColor)) {
        const int numFramePasses = _GetNumFramePasses();
        for (int i = 0; i < numFramePasses; ++i) {
            const hvt::FramePassPtr& currentPass = _GetFramePass(i);
            if (!currentPass) {
                continue;
            }

            currentPass->params().renderParams.wireframeColor = wireframeSelectionColor;
        }
    }

    int width = 0;
    int height = 0;
    drawContext.getRenderTargetSize(width, height);

    bool vpDirty;
    if ((vpDirty = (width != _viewport[2] || height != _viewport[3]))) {
        _viewport = GfVec4d(0, 0, width, height);
        // Only iterate over all passes
        const int numFramePasses = _GetNumFramePasses();
        for (int i = 0; i < numFramePasses; ++i) {
            const hvt::FramePassPtr& currentPass = _GetFramePass(i);
            if (!currentPass) {
                continue;
            }

            currentPass->params().renderBufferSize  = GfVec2i(width, height);
        }
    }

    const GfRange2f displayWindow(GfVec2f(0.0f), GfVec2f(width, height));
    GfRect2i renderRegion = MayaHydraRenderRegionCommand::getRenderRegion().has_value() ? MayaHydraRenderRegionCommand::getRenderRegion().value() : GfRect2i(GfVec2i(0.0f), GfVec2i(width, height));
    // Sanitize render region to avoid crash for some renderers (e.g., HdPrman-26)
    const int minX = std::clamp(renderRegion.GetMinX(), 0, width - 1);
    const int minY = std::clamp(renderRegion.GetMinY(), 0, height - 1);
    const int maxX = std::clamp(renderRegion.GetMaxX(), minX, width - 1);
    const int maxY = std::clamp(renderRegion.GetMaxY(), minY, height - 1);
    renderRegion = GfRect2i(GfVec2i(minX, minY), GfVec2i(maxX, maxY));
    for (int i = 0; i < numFramePasses; ++i) {
        const hvt::FramePassPtr& currentPass = _GetFramePass(i);
        if (!currentPass) {
            continue;
        }
        currentPass->params().viewInfo.framing = PXR_NS::CameraUtilFraming(displayWindow, renderRegion);
    }
    
    // For Storm, leave renderParams.camera empty so HVT renders through its free camera
    // (rebuilt each frame from params().viewInfo matrices). We used to supply
    // a custom camera path to HVT here, but HVT ignored it up until PR #173 :
    // https://github.com/Autodesk/hydra-viewport-toolbox/pull/173
    // When that PR came into effect, our custom camera path was now used, but 
    // it turns out it provided the wrong values, and desynced the render from
    // the picking. So for Storm we mark the camera path empty to intentionally use
    // the previous behaviour.
    //
    // Other delegates resolve their view matrix from a camera prim, and HVT's free camera
    // is not published as one, so they fall back to an identity view (rendering from the
    // world origin) unless the viewport camera prim is bound here.
    const bool needsCameraPrim = !_isUsingHdSt;

    SdfPath cameraPath;
    if (needsCameraPrim) {
        MStatus        status;
        const MDagPath camPath = getFrameContext()->getCurrentCameraPath(&status);
        if (status == MStatus::kSuccess) {
            const MString ufeCameraPathString = getFrameContext()->getCurrentUfeCameraPath(&status);
            const Ufe::Path ufeCameraPath = Ufe::PathString::path(ufeCameraPathString.asChar());
            // TODO: Support USD cameras.
            if (ufeCameraPath.runTimeId() == UfeExtensions::getMayaRunTimeId()) {
                MFnCamera camera(camPath, &status);
                // TODO: Support orthographic cameras.
                if (status == MStatus::kSuccess && !camera.isOrtho() && _mayaHydraSceneIndex) {
                    cameraPath = _mayaHydraSceneIndex->GetCameraPrimPath(camPath);
                }
            }
        }
    }

    for (int i = 0; i < numFramePasses; ++i) {
        const hvt::FramePassPtr& currentPass = _GetFramePass(i);
        if (!currentPass) {
            continue;
        }
        currentPass->params().renderParams.camera = cameraPath;
    }

    // Update all registered plugin before render.
    for (auto& entry : _sceneIndexRegistry->GetRegistrations()) {
        entry.second->Update();
    }

    if (_isUsingHdSt) {
        auto  enableShadows = true;
        auto* lightParam = drawContext.getLightParameterInformation(
            0, MHWRender::MDrawContext::kFilteredIgnoreLightLimit);
        if (lightParam != nullptr) {
            MIntArray intVals;
            if (lightParam->getParameter(
                    MHWRender::MLightParameterInformation::kGlobalShadowOn, intVals)
                && intVals.length() > 0) {
                enableShadows = intVals[0] != 0;
            }
        }
        HdxShadowTaskParams shadowParams;
        shadowParams.cullStyle = HdCullStyleNothing;

        // The light & shadow parameters currently (19.11-20.08) are only used for tasks specific to
        // Storm
        // Only iterate over passes
        const int numFramePasses = _GetNumFramePasses();
        for (int i = 0; i < numFramePasses; ++i) {
            const hvt::FramePassPtr& currentPass = _GetFramePass(i);
            if (!currentPass) {
                continue;
            }

            currentPass->SetEnableShadows(enableShadows);
            currentPass->SetShadowParams(shadowParams);
        }
        if (_mayaHydraSceneIndex) {
            _mayaHydraSceneIndex->SetShadowsEnabled(enableShadows);
        }

#ifndef MAYAHYDRALIB_OIT_ENABLED
        // This is required for HdStorm to display transparency.
        // We should fix this upstream, so HdStorm can setup
        // all the required states.
        MayaHydraSetRenderGLState state;
#endif
        renderFrame(true);

    } else {
        renderFrame(true);
    }

    //Store as old display style
    _oldDisplayStyle = currentDisplayStyle;

    return MStatus::kSuccess;
}

MtohRenderOverride* MtohRenderOverride::GetByName(TfToken rendererName)
{
    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        if (instance->_rendererDesc.rendererName == rendererName) {
            return instance;
        }
    }
    return nullptr;
}

void MtohRenderOverride::_ClearMayaHydraSceneIndex()
{
#ifdef CODE_COVERAGE_WORKAROUND
    // Leak the Maya scene index for code coverage, as its base class
    // HdRetainedSceneIndex dtor crashes in Windows clang code coverage build.
    _mayaHydraSceneIndex->_Destroy();
#else
    _dataProducerMergingSceneIndexProxy->RemoveSceneIndex(_mayaHydraSceneIndex);
#endif
    _mayaHydraSceneIndex.Reset();
}

void MtohRenderOverride::_InitHydraResources(
    const MHWRender::MDrawContext& drawContext,
    const MayaHydraParams& delegateParams)
{
    MH_PROFILE_FUNCTION();
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("MtohRenderOverride::_InitHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    _initializationAttempted = true;

    GlfContextCaps::InitInstance();

    static const TfTokenVector allPurposeRenderTags = { HdRenderTagTokens->geometry,
                                                        HdRenderTagTokens->render,
                                                        HdRenderTagTokens->proxy,
                                                        HdRenderTagTokens->guide };
    // Secondary graphics pass index
    static const int           secondaryGraphicsPassIndex = 1;

    // This is where the passes and their information is created
    _CreateFramePassesData();

    //Using passes data
    _CreateFramePasses();
  
    // The SkydomeTask is Storm-specific. HdxSkydomeTask::Execute will cast 
    // the render pass state to HdStRenderPassState and return if it's not
    // of that type.
    if (_isUsingHdSt)
    {
        // Add the 'SkyDome' task to the frame pass.
        // Get the first render task path.
        constexpr int skyDomePassIndex = 0; // first pass index
        const hvt::FramePassPtr& skyDomePass = _GetFramePass(skyDomePassIndex);

        auto renderTasks
            = skyDomePass->GetTaskManager()->GetTasks(hvt::TaskFlagsBits::kRenderTaskBit);
        
        const PXR_NS::SdfPath firstRenderTaskPath = renderTasks[0]->GetId();

        // Define a getter for the layer settings.
        const auto getLayerSettings
            = [&]() -> hvt::BasicLayerParams const* { return &skyDomePass->params(); };

        // Create the SkyDomeTask and insert it before the first existing render task.
        //This task is to display the skydome, it needs to be in one pass only
        
        hvt::CreateSkyDomeTask(
            skyDomePass->GetTaskManager(),
            skyDomePass->GetRenderBufferAccessor(),
            getLayerSettings,
            firstRenderTaskPath,
            hvt::TaskManager::InsertionOrder::insertBefore);
    }
        
    //Set passes constant parameters
    for (int i=0;i< _GetNumFramePasses(); ++i) {
        const auto& currentPass = _GetFramePass(i);
        // Set the passes to render all purposes by default, the actual render tag filtering will be done in
        // the scene indices filtering lambda function

        // Special case for the secondary graphics pass which has its own render tag
        // as Fvp::secondaryGraphicsRenderTagToken
        const bool isTheSecondaryGraphicsFramePass
            = (1 == _GetNumFramePasses() ) 
            ? true // When we have a single pass, it's also the secondary graphics pass
            : secondaryGraphicsPassIndex == i; // When we have multiple passes, index secondaryGraphicsPassIndex is 
                                               // the secondary graphics pass
        if (isTheSecondaryGraphicsFramePass) {
            // It's the secondary graphics pass, add the secondary graphics render tag
            //If it's not already inside allPurposeRenderTags
            TfTokenVector secondaryGraphicsRenderTags = allPurposeRenderTags;
            const bool hasAlreadyTheSecondaryGraphicsRenderTag = std::find( secondaryGraphicsRenderTags.cbegin(), 
                                                                            secondaryGraphicsRenderTags.cend(), 
                                                                            Fvp::secondaryGraphicsRenderTagToken) 
                                                                    != secondaryGraphicsRenderTags.cend();
            if ( ! hasAlreadyTheSecondaryGraphicsRenderTag) {
                secondaryGraphicsRenderTags.push_back(Fvp::secondaryGraphicsRenderTagToken);//Add it
            }

            currentPass->params().renderTags = secondaryGraphicsRenderTags;
        }
        else{
            currentPass->params().renderTags = allPurposeRenderTags;
        }
        
        //Register teminal scene index
        GetMayaHydraLibInterface().RegisterTerminalSceneIndex(
            currentPass->GetRenderIndex()->GetTerminalSceneIndex());

        //Set default values
        currentPass->params().backgroundColor                   = GfVec4f(0.0f, 0.0f, 0.0f, 0.0f);//For clearing
        currentPass->params().backgroundDepth                   = 1.0f;//For clearing
        currentPass->params().renderParams.enableLighting       = true;
#if PXR_VERSION <= 2508
        currentPass->params().renderParams.enableSceneMaterials = true;
#endif
        currentPass->params().renderParams.cullStyle            = HdCullStyleBackUnlessDoubleSided;
        currentPass->params().enableColorCorrection             = false; // Disable color correction to let Maya take care of it
        currentPass->params().visualizeAOV                      = HdAovTokens->color;
    }

    // Note that if there are multiple passes and they share render buffers,
    // the resulting image will depend on when the image writing code is
    // called, rather than which frame pass is passed as an argument.
    _fileWriterArgs = VtDictionary { { "framePass", VtValue(_GetFramePass(0).get()) } };//Use first pass
    if (_hgi) {
        _fileWriterArgs.SetValueAtPath("hgi", VtValue(_hgi.get()));
    }

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

    _dataProducerMergingSceneIndexProxy = std::make_shared<Fvp::DataProducerMergingSceneIndexProxy>();

    constexpr bool interactive = true;
    _mayaHydraSceneIndex = MayaHydraSceneIndex::New(mhInitData, interactive);
    TF_VERIFY(_mayaHydraSceneIndex, "Maya Hydra scene index not found, check mayaHydra plugin installation.");

    VtValue fvpSelectionTrackerValue(_fvpSelectionTracker);
    for (int i = 0; i < _GetNumFramePasses(); ++i) {
        const auto& currentPass = _GetFramePass(i);
        currentPass->SetTaskContextData(
            FvpTokens->fvpSelectionState, fvpSelectionTrackerValue);
    }

    _mayaHydraSceneIndex->Populate();
    //Add the scene index as an input scene index of the merging scene index
    _dataProducerMergingSceneIndexProxy->InsertSceneIndex(_mayaHydraSceneIndex, MAYA_NATIVE_ROOT);
    
    if (!_sceneIndexRegistry) {
        _sceneIndexRegistry.reset(new MayaHydraSceneIndexRegistry(_dataProducerMergingSceneIndexProxy->GetMergingSceneIndex()));
    }

    // We provide the pick context for pick handlers, so set the pick handler
    // registry accordingly.
    PickHandlerRegistry::Instance().SetPickContext(this);

    // Create internal scene indices chain
    _inputSceneIndexOfFilteringSceneIndicesChain = _dataProducerMergingSceneIndexProxy->GetMergingSceneIndex();

    //Put BlockPrimRemovalPropagationSceneIndex first as it can block/unblock the prim removal propagation on the whole scene indices chain
    _blockPrimRemovalPropagationSceneIndex = Fvp::BlockPrimRemovalPropagationSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _inputSceneIndexOfFilteringSceneIndicesChain = _blockPrimRemovalPropagationSceneIndex;

    // Adding the MayaViewportSceneIndex right after _blockPrimRemovalPropagationSceneIndex so that it's very early on in the chain, 
    // to keep it close to the Maya data it is primarily designed towards. However, in theory its placement shouldn't matter too much, 
    // as it is not designed around being in a specific place in the chain.
    _mayaViewportSceneIndex = MayaViewportSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain, _mayaHydraSceneIndex);
    _inputSceneIndexOfFilteringSceneIndicesChain = _mayaViewportSceneIndex;

    // Render globals are initialized after this method is called, so
    // included purposes attributes do not yet exist on the
    // defaultRenderGlobals node.  Pass in an empty set of included
    // purposes here.
    _purposeFilteringSceneIndex = Fvp::PurposeFilteringSceneIndex::New(
        _inputSceneIndexOfFilteringSceneIndicesChain, {});
        // RenderGlobalsUtils::GetIncludedPurposes());
    _pruningSceneIndex = Fvp::PruningSceneIndex::New(_purposeFilteringSceneIndex);
    if (!_mayaHydraSceneIndex ->useMeshAdapter()) {
        _pruningSceneIndex->AddExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya filtering is handled by VP2/OGS.
    }

    // Scene globals must be before the GP resolver so procedurals can
    // read the current frame during cooking
    _sceneGlobalsSceneIndex = HdsiSceneGlobalsSceneIndex::New(_pruningSceneIndex);

    // Set initial frame from Maya when scene globals scene index is created
    if (_sceneGlobalsSceneIndex) {
        const MTime  currentTime = MAnimControl::currentTime();
        const double currentFrame = currentTime.value();
        _SetCurrentFrameInHydraGlobalSceneIndex(currentFrame);

        // Register time change callback if not already registered
        if (_timeChangeCallback == 0) {
            MStatus status;
            _timeChangeCallback = MEventMessage::addEventCallback(
                "timeChanged", _TimeChangedCallback, this, &status);
            if (!status) {
                TF_WARN("Failed to register time change callback");
            }
        }
    }

    // Create HdGp before selection so generated prims are selectable/highlightable.
    // Use MhGenerativeProceduralResolvingSceneIndex so we match other MayaHydra
    // scene indices, can ApplyExcludedSceneRoot for Maya native content, and keep
    // HdGp usage centralized in mayaHydraLib.
    _gpResolvingSceneIndex = MhGenerativeProceduralResolvingSceneIndex::New(_sceneGlobalsSceneIndex);

    _selection = std::make_shared<Fvp::Selection>();
    _selectionSceneIndex = Fvp::SelectionSceneIndex::New(_gpResolvingSceneIndex, _selection);
    _selectionSceneIndex->SetDisplayName("Flow Viewport Selection Scene Index");
    _inputSceneIndexOfFilteringSceneIndicesChain = _selectionSceneIndex;

    _dirtyLeadObjectSceneIndex = MAYAHYDRA_NS::MhDirtyLeadObjectSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _inputSceneIndexOfFilteringSceneIndicesChain = _dirtyLeadObjectSceneIndex;

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    // _InitHydraResources() is always called from Render(), so
    // getFrameContext() will be valid and non-null.
    auto viewportId = getRenderingDestination(getFrameContext());

    // Add isolate select scene index.
    auto& isolateSelectMgr = Fvp::IsolateSelectManager::Get();
    auto selection = isolateSelectMgr.GetOrCreateIsolateSelection(viewportId);
    auto isSi = Fvp::IsolateSelectSceneIndex::New(
        viewportId, selection, _inputSceneIndexOfFilteringSceneIndicesChain);
    // At time of writing we have a single selection scene index serving
    // all viewports.
    isolateSelectMgr.SetIsolateSelectSceneIndex(isSi);
    _inputSceneIndexOfFilteringSceneIndicesChain = isSi;
#endif

    // Set the initial selection onto the selection scene index later.
    _needToReplaceSelection = true;

    _CreateSceneIndicesChainAfterMergingSceneIndex(drawContext);

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

    // We need to setup the viewport and matrices to avoid warnings
    // when calling GetRenderTasks; they will get updated when
    // actual rendering occurs anyway.
    int width = 0;
    int height = 0;
    drawContext.getRenderTargetSize(width, height);
    auto viewMatrix
        = GetGfMatrixFromMaya(drawContext.getMatrix(MHWRender::MFrameContext::kViewMtx));
    auto projectionMatrix
        = GetGfMatrixFromMaya(drawContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx));

    HdTaskSharedPtrVector tasks;//From all passes
    const int numFramePasses = _GetNumFramePasses();
    for (int i = 0; i < numFramePasses; ++i) {
        const auto& currentPass = _GetFramePass(i);
        currentPass->params().renderBufferSize          = GfVec2i(width, height);
        currentPass->params().viewInfo.viewMatrix       = viewMatrix;
        currentPass->params().viewInfo.projectionMatrix = projectionMatrix;
    }
    
    _initializationSucceeded = true;
}

//When fullReset is true, we remove the data producer scene indices that apply to all viewports.
//It means you are doing a full reset of hydra such as when doing "File New".
//Use fullReset = false when you still want to see the previously registered data producer scene indices when using an hydra viewport.
void MtohRenderOverride::ClearHydraResources(bool fullReset)
{
    MH_PROFILE_FUNCTION();
    if (!_initializationAttempted) {
        return;
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("MtohRenderOverride::ClearHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    // Stop render delegates before tearing down scene indices or render indices.
    // Matches hvt::ViewportEngine::CreateRenderer(), which calls Stop() before
    // replacing a render index.
    for (int i = 0; i < _GetNumFramePasses(); ++i) {
        const auto& renderIndexProxy = _framePassesData[i]->_renderIndexProxy;
        if (renderIndexProxy && renderIndexProxy->RenderIndex()) {
            if (HdRenderDelegate* renderDelegate = renderIndexProxy->RenderIndex()->GetRenderDelegate()) {
                renderDelegate->Stop();
            }
        }
    }

    //We don't have any viewport using Hydra any more
    Fvp::RenderViewDataManager::Get().RemoveAllRenderViewData();

    if (fullReset){
        //Remove the data producer scene indices that apply to all views
        Fvp::DataProducerSceneIndexInterfaceImp::get().ClearDataProducerSceneIndicesThatApplyToAllViews();
    }

    // Remove the scene index registry
    _sceneIndexRegistry.reset();

    _ClearMayaHydraSceneIndex();

    // HYDRA-2019 : We need to manually call the destruction code, as we have some 
    // lifetime management issues preventing the destructor from being called.
    _mayaViewportSceneIndex->Destroy();
    _mayaViewportSceneIndex.Reset();

    _displayStyleSceneIndex = nullptr;
    _pruneTexturesSceneIndex = nullptr;
    _defaultMaterialSceneIndex = nullptr;
    _currentlyTextured = false;
    _selectionSceneIndex.Reset();
    _selection.reset();
    _wireframeColorInterfaceImp.reset();
    _leadObjectPathTracker.reset();
    _oldDisplayStyle = 0;
    _oldRefineLevel = 0;

    // Cleanup passes

    // Now safely cleanup passes
    for (int i = 0; i < _GetNumFramePasses(); ++i) {
        auto& currentPass = _GetFramePass(i);
        if (currentPass) {
            auto renderIndex = currentPass->GetRenderIndex();
            if (renderIndex) {
                GetMayaHydraLibInterface().UnregisterTerminalSceneIndex(renderIndex->GetTerminalSceneIndex());
            }
            currentPass.reset();
        }

        _framePassesData[i]->_passFilteringSceneIndex = nullptr;//Reset scene index
        auto& _framePassesRenderer = _framePassesData[i]->_renderIndexProxy;
        if (_framePassesRenderer) {
#ifdef CODE_COVERAGE_WORKAROUND
            // Store the pointers so they don't get immediately destroyed,
            // as hvt::RenderIndexProxy's dtor will call HdRenderIndex's dtor,
            // which crashes on Clang.
            static std::vector<hvt::RenderIndexProxyPtr> leakedRenderIndexProxyPtrs;
            leakedRenderIndexProxyPtrs.push_back(_framePassesRenderer);
#endif
            _framePassesRenderer.reset();
        }
    }

    _ClearFramePassesData();

    _fileWriterArgs.clear();

    //Decrease ref count on the render index proxy which owns the merging scene index at the end of this function as some previous calls may likely use it to remove some scene indices
    _dataProducerMergingSceneIndexProxy.reset();

    _viewport = GfVec4d(0, 0, 0, 0);
    
    // Reset scene index refs to prevent use-after-destroy in callbacks
    _sceneGlobalsSceneIndex.Reset();
    
    // Unregister Maya event callbacks to prevent them from firing on cleared resources
    if (_timeChangeCallback) {
        MMessage::removeCallback(_timeChangeCallback);
        _timeChangeCallback = 0;
    }
    
    _initializationSucceeded = false;
    _initializationAttempted = false;

    // Remove the pick context from pick handlers.
    PickHandlerRegistry::Instance().SetPickContext(nullptr);
}

HdSceneIndexBaseRefPtr MtohRenderOverride::_CreatePassFilteringSceneIndex(
    Fvp::FramePassDataPtr& filteringData)
{
    auto passFilteringSceneIndex = Fvp::PassFilteringSceneIndex::New(
        _lastFilteringSceneIndexBeforeCustomFiltering, filteringData);
    filteringData->SetPassFilteringSceneIndex(passFilteringSceneIndex);//Store in pass filtering data
    return passFilteringSceneIndex;
}

void MtohRenderOverride::_CreateSceneIndicesChainAfterMergingSceneIndex(const MHWRender::MDrawContext& drawContext)
{
    // We need to recreate the filtering scene index chain after the merging scene index as there
    // was a change such as in the BBox display style which has been turned on or off.
    _lastFilteringSceneIndexBeforeCustomFiltering = nullptr; // Release

    //This function is where happens the ordering of filtering scene indices that are after the merging scene index
    //We use as its input scene index : _inputSceneIndexOfFilteringSceneIndicesChain
    _lastFilteringSceneIndexBeforeCustomFiltering = _inputSceneIndexOfFilteringSceneIndicesChain;

    // Add display style scene index
    _lastFilteringSceneIndexBeforeCustomFiltering = _displayStyleSceneIndex =
            Fvp::DisplayStyleOverrideSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering);
    _displayStyleSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims don't use global refinement

    // Add texture disabling Scene Index
    const bool pruneTextures = !(drawContext.getDisplayStyle() & MHWRender::MFrameContext::kTextured);
    _lastFilteringSceneIndexBeforeCustomFiltering = _pruneTexturesSceneIndex =
        Fvp::PruneTexturesSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, pruneTextures);
    _currentlyTextured = !pruneTextures;

    // Add default material scene index
    _lastFilteringSceneIndexBeforeCustomFiltering = _defaultMaterialSceneIndex = Fvp::DefaultMaterialSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering,
                                                                                _mayaViewportSceneIndex ? _mayaViewportSceneIndex->GetDefaultMaterialPath() : SdfPath(),
                                                                                _mayaViewportSceneIndex ? _mayaViewportSceneIndex->GetDefaultMaterialExclusionPaths(): SdfPathVector());

    if(! _leadObjectPathTracker){
        _leadObjectPathTracker = std::make_shared<MAYAHYDRA_NS_DEF::MhLeadObjectPathTracker>(_dirtyLeadObjectSceneIndex);
    }

    if (! _wireframeColorInterfaceImp){
        _wireframeColorInterfaceImp = std::make_shared<MAYAHYDRA_NS_DEF::MhWireframeColorInterfaceImp>(_selection, _leadObjectPathTracker);
    }

    // Insert the bounding box filtering scene index which converts geometries into a bounding box
    // using the extent attribute
    _bboxSceneIndex = Fvp::BboxSceneIndex::New(
        _lastFilteringSceneIndexBeforeCustomFiltering, _wireframeColorInterfaceImp);
    _bboxSceneIndex->AddExcludedSceneRoot(
        MAYA_NATIVE_ROOT); // Maya native prims are already converted by OGS
    _lastFilteringSceneIndexBeforeCustomFiltering = _bboxSceneIndex;

    // Repr selector Scene Index
    _lastFilteringSceneIndexBeforeCustomFiltering = _reprSelectorSceneIndex =
                                                 Fvp::ReprSelectorSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering,
                                                 _wireframeColorInterfaceImp);
    _reprSelectorSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT);
    _reprSelectorSceneIndex->SetReprType(Fvp::ReprSelectorSceneIndex::RepSelectorType::Default, false, _globals.delegateParams.refineLevel);

    // Setup selection highlight scene indices
    {
        //// At time of writing, wireframe selection highlighting of Maya native data
        //// is done by Maya at render item creation time, so avoid double wireframe
        //// selection highlighting by excluding MAYA_NATIVE_ROOT.

#if PXR_VERSION >= 2405
        _lastFilteringSceneIndexBeforeCustomFiltering = _geomSubsetWhSi = Fvp::GeomSubsetWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _geomSubsetWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);
#endif

        _lastFilteringSceneIndexBeforeCustomFiltering = _meshWhSi = Fvp::MeshWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _meshWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);

        _lastFilteringSceneIndexBeforeCustomFiltering = _niInstanceWhSi = Fvp::NiInstanceWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _niInstanceWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);

        _lastFilteringSceneIndexBeforeCustomFiltering = _niPrototypeWhSi = Fvp::NiPrototypeWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _niPrototypeWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);

        _lastFilteringSceneIndexBeforeCustomFiltering = _piInstancerWhSi = Fvp::PiInstancerWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _piInstancerWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);

        _lastFilteringSceneIndexBeforeCustomFiltering = _piPrototypeWhSi = Fvp::PiPrototypeWhSi::New(_lastFilteringSceneIndexBeforeCustomFiltering, _highlightHierarchyPrefix, _wireframeColorInterfaceImp);
        _piPrototypeWhSi->AddExcludedPath(MAYA_NATIVE_ROOT);
    }

    TF_AXIOM(_mayaViewportSceneIndex);
    _lastFilteringSceneIndexBeforeCustomFiltering = _lightsManagementSceneIndex = Fvp::LightsManagementSceneIndex::New(
        _lastFilteringSceneIndexBeforeCustomFiltering, _mayaViewportSceneIndex->DefaultLightPath());
    _lightsManagementSceneIndex->SetLightingMode(convertFromMayaLightingModeToFlowViewportLightMode(_lightingMode));
    _mayaViewportSceneIndex->SetLightsManagementSceneIndex(_lightsManagementSceneIndex);

#ifdef CODE_COVERAGE_WORKAROUND
    Fvp::leakSceneIndex(_lastFilteringSceneIndexBeforeCustomFiltering);//Should this be on the frame pass filtering scene index ?
#endif

    // Non main graphics passes
    _CreateNonMainFramePassesFilteringSceneIndices();
}

void MtohRenderOverride::_RemovePanel(MString panelName)
{
    auto foundPanelCallbacks = _FindPanelCallbacks(panelName);
    if (foundPanelCallbacks != _renderPanelCallbacks.end()) {
        MMessage::removeCallbacks(foundPanelCallbacks->second);
        Fvp::RenderViewDataManager::Get().RemoveRenderViewData(std::string(panelName.asChar()));
        _renderPanelCallbacks.erase(foundPanelCallbacks);
    }

    if (_renderPanelCallbacks.empty()) {
        constexpr bool fullReset = false;
        ClearHydraResources(fullReset);
    }
}

void MtohRenderOverride::SelectionChanged(
    const Ufe::SelectionChanged& notification
)
{
    MH_PROFILE_FUNCTION();
    TF_DEBUG(FVP_APP_SELECTION_CHANGE)
        .Msg("MtohRenderOverride::SelectionChanged(Ufe::SelectionChanged) called.\n");

    if (!_initializationSucceeded) {
        return;
    }

    TF_AXIOM(_selectionSceneIndex);

    // Two considerations:
    // 1) Reading from the Maya active selection list only returns
    //    Maya objects, so must read from the UFE selection.
    // 2) The UFE selection does not have Maya component selections.
    //    When we are ready to support these, must be read from the Maya
    //    selection.  A tricky aspect is that the UFE selection
    //    notification is sent before the Maya selection is ready, so
    //    reading the Maya selection must be done from the Maya selection
    //    changed callback, not the UFE selection changed callback.
    using SnOp = Ufe::SelectionCompositeNotification::Op;
    using SnSiPtr = Fvp::SelectionSceneIndexRefPtr;
    static auto appendSn = [](const SnOp& op, const SnSiPtr& si) {
        si->AddSelection(op.item->path());
    };
    static auto removeSn = [](const SnOp& op, const SnSiPtr& si) {
        si->RemoveSelection(op.item->path());
    };
    // FLOW_VIEWPORT_TODO  Support selection insert.  PPT, 19-Oct-2023
    static auto insertSn = [](const SnOp&, const SnSiPtr& si) {
        TF_WARN("Insert into selection not supported.");
    };
    static auto clearSn = [](const SnOp&, const SnSiPtr& si) {
        si->ClearSelection();
    };
    static auto replaceWithSn = [](const SnOp& op, const SnSiPtr& si) {
        si->ReplaceSelection(*Ufe::GlobalSelection::get());
    };
    static std::function<void(const SnOp&, const SnSiPtr&)> changeSn[] = {appendSn, removeSn, insertSn, clearSn, replaceWithSn};

    if (notification.opType() ==
        Ufe::SelectionChanged::SelectionCompositeNotification) {

        const auto& compositeNotification = notification.staticCast<Ufe::SelectionCompositeNotification>();

        for (const auto& op : compositeNotification) {
            changeSn[op.opType](op, _selectionSceneIndex);
        }
    }
    else {
        SnOp op(notification);

        changeSn[op.opType](op, _selectionSceneIndex);
    }


    // FLOW_VIEWPORT_TODO  Clarify new Flow Viewport selection tracker
    // architecture.  Here is where we would set the selection onto the
    // selection tracker, or trackers, if data provider plugins need to have
    // their own selection tracker.  The selection tracker makes the selection
    // and selection-derived data availabel to a selection task or selection
    // tasks through the task context data.  PPT, 18-Sep-2023
}

MHWRender::DrawAPI MtohRenderOverride::supportedDrawAPIs() const
{
    return MHWRender::kOpenGLCoreProfile | MHWRender::kOpenGL;
}

MStatus MtohRenderOverride::setup(const MString& destination)
{
    MStatus status;

    auto panelNameAndCallbacks = _FindPanelCallbacks(destination);
    if (panelNameAndCallbacks == _renderPanelCallbacks.end()) {
        // Install the panel callbacks
        MCallbackIdArray newCallbacks;

        auto id = MUiMessage::add3dViewDestroyMsgCallback(
            destination, _PanelDeletedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        id = MUiMessage::add3dViewRendererChangedCallback(
            destination, _RendererChangedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        id = MUiMessage::add3dViewRenderOverrideChangedCallback(
            destination, _RenderOverrideChangedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        _renderPanelCallbacks.emplace_back(destination, newCallbacks);
    }

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    if (!_viewSelectedChangedCb) {
        _viewSelectedChangedCb = MUiMessage::add3dViewSelectedChangedCallback(
            _ViewSelectedChangedCb, this, &status);
        if (status != MStatus::kSuccess) {
            return status;
        }
    }
#endif

    auto* renderer = MHWRender::MRenderer::theRenderer();
    if (renderer == nullptr) {
        return MStatus::kFailure;
    }

    if (_operations.empty()) {
        // Clear and draw pre scene elements (grid not pushed into hydra)
        _operations.push_back(std::make_unique<MayaHydraPreRender>("HydraRenderOverride_PreScene"));

        // The main hydra render
        // For the data server, this also invokes scene update then syncs the scene index after scene update
        _operations.push_back(std::make_unique<MayaHydraRender>("HydraRenderOverride_DataServer", this));

        // Draw post scene elements (cameras, CVs, shapes not pushed into hydra)
        _operations.push_back(std::make_unique<MayaHydraPostRender>("HydraRenderOverride_PostScene"));

        // Draw HUD elements
        _operations.push_back(std::make_unique<MHWRender::MHUDRender>());

        // Set final buffer options
        auto presentTarget = std::make_unique<MHWRender::MPresentTarget>("HydraRenderOverride_Present");
        presentTarget->setPresentDepth(true);
        presentTarget->setTargetBackBuffer(MHWRender::MPresentTarget::kCenterBuffer);
        _operations.push_back(std::move(presentTarget));
    }

    return MS::kSuccess;
}

MStatus MtohRenderOverride::cleanup()
{
    _currentOperation = -1;
    return MS::kSuccess;
}

bool MtohRenderOverride::startOperationIterator()
{
    _currentOperation = 0;
    return true;
}

MHWRender::MRenderOperation* MtohRenderOverride::renderOperation()
{
    if (_currentOperation >= 0 && _currentOperation < static_cast<int>(_operations.size())) {
        return _operations[_currentOperation].get();
    }
    return nullptr;
}

bool MtohRenderOverride::nextRenderOperation()
{
    return ++_currentOperation < static_cast<int>(_operations.size());
}

void MtohRenderOverride::_PopulateSelectionList(
    const PickHitVector&             hits,
    const MHWRender::MSelectionInfo& selectInfo,
    MSelectionList&                  selectionList,
    MPointArray&                     worldSpaceHitPts,
    bool&                            isOneMayaNodeInComponentsPickingMode)
{
    if (hits.empty() || !_mayaHydraSceneIndex || !_ufeSn) {
        return;
    }

    PickHandler::Output pickOutput(selectionList, worldSpaceHitPts, _ufeSn);

    MStatus status;
    for (const PickHit& hit : hits) {
        PickHandler::Input pickInput(hit, selectInfo, hits.size() == 1u);

        auto pickHandler = _PickHandler(hit);
        if (TF_VERIFY(pickHandler, "No pick handler found for pick hit %s!", hit.hdxPickHit.objectId.GetText())) {

            if (pickHandler->inSingleNodeComponentsPick(hit)) {
                isOneMayaNodeInComponentsPickingMode = true;
                return;
            }

            pickHandler->handlePickHit(pickInput, pickOutput);
        }
    }
}

PickHandlerConstPtr
MtohRenderOverride::_PickHandler(const PickHit& pickHit) const
{
    return PickHandlerRegistry::Instance().GetHandler(pickHit.hdxPickHit.objectId);
}

void MtohRenderOverride::_PickByRegion(
    PickHitVector& outHits,
    const MMatrix& viewMatrix,
    const MMatrix& projMatrix,
    bool singlePick,
    const TfToken& geomSubsetsPickMode,
    bool pointSnappingActive,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    unsigned int sel_x,
    unsigned int sel_y,
    unsigned int sel_w,
    unsigned int sel_h)
{
    MMatrix adjustedProjMatrix;
    // Compute a pick matrix that, when it is post-multiplied with the projection matrix, will
    // cause the picking region to fill the entire viewport for OpenGL selection.
    {
        double center_x = sel_x + sel_w * 0.5;
        double center_y = sel_y + sel_h * 0.5;

        MMatrix pickMatrix;
        pickMatrix[0][0] = view_w / double(sel_w);
        pickMatrix[1][1] = view_h / double(sel_h);
        pickMatrix[3][0] = (view_w - 2.0 * (center_x - view_x)) / double(sel_w);
        pickMatrix[3][1] = (view_h - 2.0 * (center_y - view_y)) / double(sel_h);

        adjustedProjMatrix = projMatrix * pickMatrix;
    }

    // Set up picking params.
    HdxPickTaskContextParams pickParams;
    // Use the same size as selection region is enough to get all pick results.
    pickParams.resolution.Set(sel_w, sel_h);
    pickParams.pickTarget = HdxPickTokens->pickPrimsAndInstances;
    pickParams.resolveMode = singlePick ? HdxPickTokens->resolveNearestToCenter : HdxPickTokens->resolveUnique;
    pickParams.doUnpickablesOcclude = false;
    pickParams.viewMatrix.Set(viewMatrix.matrix);
    pickParams.projectionMatrix.Set(adjustedProjMatrix.matrix);
    pickParams.collection = _renderCollection;
    pickParams.collection.SetExcludePaths({_highlightHierarchyPrefix});

    if (geomSubsetsPickMode == GeomSubsetsPickModeTokens->Faces) {
        pickParams.pickTarget = HdxPickTokens->pickFaces;
    }

    if (pointSnappingActive) {
        pickParams.pickTarget = HdxPickTokens->pickPoints;

        // Exclude selected Rprims to avoid self-snapping issue.
        pickParams.collection = _pointSnappingCollection;
        auto excludePaths = _selectionSceneIndex->GetFullySelectedPaths();
        excludePaths.push_back(_highlightHierarchyPrefix);
        pickParams.collection.SetExcludePaths(excludePaths);
    }

    // Execute picking tasks.
    //Accumulate the pick hits from all passes.
    for (int i = 0; i < _GetNumFramePasses(); ++i) {
        const hvt::FramePassPtr& pass = _GetFramePass(i);
        if (!pass) {
            continue;
        }

        // Set a temp HdxPickHitVector to avoid modifying the outHits vector
        // in order to accumulate the pick hits from multiple passes.
        HdxPickHitVector tempHits;
        pickParams.outHits = &tempHits;
        pass->Pick(pickParams);
    
        //Combine the 2 PickHitVector
        if (!tempHits.empty()) {
            // Reserve memory for efficiency
            outHits.reserve(outHits.size() + tempHits.size());
            // Insert all hits from tempHits into outHits
            for (const auto& hit : tempHits) {
                outHits.emplace_back(i, hit);
            }
        }
    }
}

bool MtohRenderOverride::select(
    const MHWRender::MFrameContext&  frameContext,
    const MHWRender::MSelectionInfo& selectInfo,
    bool /*useDepth*/,
    MSelectionList& selectionList,
    MPointArray&    worldSpaceHitPts)
{
    MH_PROFILE_FUNCTION();
    /*
    * There are 2 modes of selection picking for components in maya :
    * 1) You can be in components picking mode, this setting is global.This is detected in the function "isInComponentsPickingMode(selectInfo)"
    * 2) The second mode is when you right click on a node and choose a component to pick it (e.g : Face), this is
    * where we use the variable "isOneNodeInComponentsPickingMode" to detect that case, later in this function.
    */
    if (isInComponentsPickingMode(selectInfo)) {
        return false; //When being in components picking, returning false will use maya/OGS for components selection
    }

    MStatus status = MStatus::kFailure;

    MMatrix viewMatrix = frameContext.getMatrix(MHWRender::MFrameContext::kViewMtx, &status);
    if (status != MStatus::kSuccess)
        return false;

    MMatrix projMatrix = frameContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx, &status);
    if (status != MStatus::kSuccess)
        return false;

    int view_x, view_y, view_w, view_h;
    status = frameContext.getViewportDimensions(view_x, view_y, view_w, view_h);
    if (status != MStatus::kSuccess)
        return false;

    unsigned int sel_x, sel_y, sel_w, sel_h;
    status = selectInfo.selectRect(sel_x, sel_y, sel_w, sel_h);
    if (status != MStatus::kSuccess)
        return false;

    PickHitVector outHits;
    const bool singlePick = selectInfo.singleSelection();
    const TfToken geomSubsetsPickMode = GetGeomSubsetsPickMode();
    const bool pointSnappingActive = selectInfo.pointSnapping();
    if (pointSnappingActive)
    {
        int cursor_x, cursor_y;
        status = selectInfo.cursorPoint(cursor_x, cursor_y);
        if (status != MStatus::kSuccess)
            return false;

        // Performance optimization for large picking region.
        // The idea is start to do picking from a small region (width = 100), return the hit result if there's one.
        // Otherwise, increase the region size and do picking repeatly till the original region size is reached.
        static bool pickPerfOptEnabled = true;
        unsigned int curr_sel_w = 100;
        while (pickPerfOptEnabled && curr_sel_w < sel_w && outHits.empty())
        {
            unsigned int curr_sel_h = curr_sel_w * double(sel_h) / double(sel_w);

            unsigned int curr_sel_x = cursor_x > (int)curr_sel_w / 2 ? cursor_x - (int)curr_sel_w / 2 : 0;
            unsigned int curr_sel_y = cursor_y > (int)curr_sel_h / 2 ? cursor_y - (int)curr_sel_h / 2 : 0;

            _PickByRegion(outHits, viewMatrix, projMatrix, singlePick, geomSubsetsPickMode, pointSnappingActive,
                view_x, view_y, view_w, view_h, curr_sel_x, curr_sel_y, curr_sel_w, curr_sel_h);

            // Increase the size of picking region.
            curr_sel_w *= 2;
        }
    }

    // Pick from original region directly when point snapping is not active or no hit is found yet.
    if (outHits.empty())
    {
        _PickByRegion(outHits, viewMatrix, projMatrix, singlePick, geomSubsetsPickMode, pointSnappingActive,
            view_x, view_y, view_w, view_h, sel_x, sel_y, sel_w, sel_h);
    }

    if (pointSnappingActive) {
        // Find the hit nearest to the cursor point and use it for point snapping.
        int nearestHitIndex = -1;
        int cursor_x, cursor_y;
        if (selectInfo.cursorPoint(cursor_x, cursor_y)) {
            nearestHitIndex = GetNearestHitIndex(frameContext, outHits, cursor_x, cursor_y);
        }

        if (nearestHitIndex >= 0) {
            const auto hit = outHits[nearestHitIndex];
            outHits.clear();
            outHits.push_back(hit);
        } else {
            outHits.clear();
        }
    }

    //isOneMayaNodeInComponentsPickingMode will be true if one of the picked node is in components picking mode
    bool isOneMayaNodeInComponentsPickingMode = false;
    const unsigned int ufeSnSizeBefore = _ufeSn ? _ufeSn->size() : 0;
    const unsigned int mayaSnLengthBefore = selectionList.length();
    _PopulateSelectionList(outHits, selectInfo, selectionList, worldSpaceHitPts, isOneMayaNodeInComponentsPickingMode);
    if (isOneMayaNodeInComponentsPickingMode){
        return false;//When being in components picking on a node, returning false will use maya/OGS for components selection
    }

    // If a marquee selection is done with GeomSubset picking enabled, and there are more than 1 pick hits,
    // only GeomSubsets will be considered for picking. If the user forgets they have GeomSubset picking on, 
    // this can lead to situations where they do a marquee select on USD data and nothing is selected.
    // Warn the user about this so they are aware and potentially adjust their GeomSubset selection mode.
    if (geomSubsetsPickMode == GeomSubsetsPickModeTokens->Faces
        && !singlePick
        && !pointSnappingActive
        && !outHits.empty()
        && _ufeSn && _ufeSn->size() == ufeSnSizeBefore
        && selectionList.length() == mayaSnLengthBefore)
    {
        MGlobal::displayWarning(
            "Marquee selection found pick hits but no selectable data. Note that for USD data, marquee selections with "
            "GeomSubset picking enabled only select GeomSubsets and not the base geometry prims. "
            "Please adjust the USD GeomSubset selection mode if necessary.");
    }

    return true;
}

void MtohRenderOverride::_ClearHydraCallback(void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }
    constexpr bool fullReset = true;
    instance->ClearHydraResources(fullReset);
}

void MtohRenderOverride::_PlayblastingChanged(bool playBlasting, void* userData)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(userData);
    if (std::atomic_exchange(&instance->_playBlasting, playBlasting) == playBlasting)
        return;

    MStatus status;
    if (!playBlasting) {
        assert(instance->_timerCallback == 0 && "Callback exists");
        instance->_timerCallback
            = MTimerMessage::addTimerCallback(1.0f / 10.0f, _TimerCallback, instance, &status);
    } else {
        status = MMessage::removeCallback(instance->_timerCallback);
        instance->_timerCallback = 0;
    }
    CHECK_MSTATUS(status);
}

void MtohRenderOverride::_TimerCallback(float, float, void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (instance->_playBlasting || instance->_isConverged) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance->_lastRenderTimeMutex);
    if ((std::chrono::system_clock::now() - instance->_lastRenderTime) < std::chrono::seconds(5)) {
        MGlobal::executeCommandOnIdle("refresh -f");
    }
}

void MtohRenderOverride::_PanelDeletedCallback(const MString& panelName, void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    instance->_RemovePanel(panelName);
}

void MtohRenderOverride::_TimeChangedCallback(void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    // Guard against use-after-destroy: don't access scene indices if Hydra resources are not initialized
    if (!instance->_initializationSucceeded) {
        return;
    }

    // Only update if scene globals scene index is initialized
    if (!instance->_sceneGlobalsSceneIndex) {
        return;
    }

    // Get current frame from Maya
    const MTime currentTime = MAnimControl::currentTime();
    const double currentFrame = currentTime.value();

    // Update frame in Hydra scene globals scene index
    instance->_SetCurrentFrameInHydraGlobalSceneIndex(currentFrame);

    // A delegate bound to a camera prim is driven by that prim, and Maya's world-matrix
    // callbacks don't fire reliably under the Evaluation Manager during playback.
    if (instance->_mayaHydraSceneIndex) {
        instance->_mayaHydraSceneIndex->RefreshCamerasOnTimeChange();
    }
}

void MtohRenderOverride::_RendererChangedCallback(
    const MString& panelName,
    const MString& oldRenderer,
    const MString& newRenderer,
    void*          data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    if (newRenderer != oldRenderer) {
        instance->_RemovePanel(panelName);
    }
}

void MtohRenderOverride::_RenderOverrideChangedCallback(
    const MString& panelName,
    const MString& oldOverride,
    const MString& newOverride,
    void*          data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    if (newOverride != instance->name()) {
        instance->_RemovePanel(panelName);
    }
}

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
namespace {

void
_LogPrimSelectionsForViewSelectedIsolate(const char* ufePathCStr, const Fvp::PrimSelections& primSelections)
{
    if (primSelections.empty()) {
        TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
            .Msg(
                "    ufePathToPrimSelections returned 0 mapping(s) for %s — isolate will not include this "
                "object.\n",
                ufePathCStr);
        return;
    }
    TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
        .Msg("    ufePathToPrimSelections returned %zu mapping(s) for %s\n", primSelections.size(), ufePathCStr);
    for (const auto& ps : primSelections) {
        TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED).Msg("      primPath=%s\n", ps.primPath.GetText());
        for (const auto& nested : ps.nestedInstanceIndices) {
            TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                .Msg(
                    "        nested instancer=%s prototypeIndex=%d instanceIndexCount=%zu\n",
                    nested.instancerPath.GetText(),
                    nested.prototypeIndex,
                    nested.instanceIndices.size());
            constexpr size_t kMaxLoggedInstanceIndices = 16;
            size_t loggedCount = 0;
            for (int instIdx : nested.instanceIndices) {
                if (loggedCount >= kMaxLoggedInstanceIndices) {
                    TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                        .Msg("          ... and %zu more\n",
                             nested.instanceIndices.size() - kMaxLoggedInstanceIndices);
                    break;
                }
                TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED).Msg("          instanceIndex=%d\n", instIdx);
                ++loggedCount;
            }
        }
    }
}

//! If \p prim carries pull customData (key \c Maya:Pull:DagPath, same as mayaUsd pullInformation.cpp),
//! resolve the edited-as-Maya root DAG path. Keeps mayaHydra on mayaUsdAPI only (no mayaUsd link).
bool ReadMayaDagPathFromUsdPullMetadata(const UsdPrim& prim, MDagPath& outDagPath)
{
    static const TfToken kPullPrimMetadataKey("Maya:Pull:DagPath");
    VtValue              value = prim.GetCustomDataByKey(kPullPrimMetadataKey);
    if (value.IsEmpty() || !value.CanCast<std::string>()) {
        return false;
    }
    const std::string dagPathStr = value.Get<std::string>();
    if (dagPathStr.empty()) {
        return false;
    }
    MSelectionList sel;
    if (sel.add(dagPathStr.c_str()) != MS::kSuccess) {
        return false;
    }
    return sel.getDagPath(0, outDagPath) == MS::kSuccess && outDagPath.isValid();
}

} // namespace

// When isolate select is built from view-selected UFE paths, USD cameras and lights
// have native Maya visual representations (gizmo rprims, HdCamera sprims) drawn under
// MAYA_NATIVE_ROOT. These native rprims are render items whose paths include the source DAG path:
//   MAYA_NATIVE_ROOT/rprims/{source_dag}/{proxy_dag_}/{PrimName}_<suffix>_<id>
// Because {source_dag} is prepended, the native rprim lives under a different SdfPath subtree than
// the proxy prefix, and Fvp::Selection prefix/ancestor matching treats them as unrelated -- so they
// get hidden by isolate select.
//
// This function searches the scene index tree for native rprims whose leaf name starts with the
// selected USD prim name and whose path contains the proxy shape name, then adds them to the isolate
// selection. Camera-specific handling (panel camera drawables, defaultUfeProxyCamera) is also
// performed for camera prims. Light-specific handling adds ufeLightProxy gizmo rprims
// created by the drawUfe plugin for light prims.
//
// Invoked from _ViewSelectedChangedCb on the same MtohRenderOverride instance as the callback's
// clientData: multiple overrides each register a view-selected callback, and a single global
// expander would capture the wrong `this`.
TfHashSet<SdfPath, SdfPath::Hash> MtohRenderOverride::_ExpandIsolateSelectionForUsdPrims(
    Fvp::Selection&               selection,
    const std::vector<Ufe::Path>& selectedUfePaths,
    const MDagPath&               panelCameraDag)
{
    TfHashSet<SdfPath, SdfPath::Hash> forceVisiblePaths;
    if (!_mayaHydraSceneIndex || selectedUfePaths.empty()) {
        return forceVisiblePaths;
    }

    TfHashSet<SdfPath, SdfPath::Hash> added;

    // When forceVis is true, paths added via addPrimPath are also inserted
    // into forceVisiblePaths so the IsolateSelectSceneIndex forces their
    // visibility ON.  Callers pass false for user-hidden USD prims so their
    // gizmo render items stay hidden during isolate select.
    auto addPrimPath = [&](const SdfPath& p, bool forceVis) {
        if (p.IsEmpty() || added.count(p) != 0) {
            return;
        }
        if (selection.Add(Fvp::PrimSelection { p })) {
            added.insert(p);
            if (forceVis) {
                forceVisiblePaths.insert(p);
            }
        }
    };

    auto resolveProxyDag = [](const Ufe::Path& ufePath) -> MDagPath {
        if (ufePath.nbSegments() < 2) {
            return MDagPath();
        }
        return UfeExtensions::ufeToDagPath(Ufe::Path(ufePath.getSegments()[0]));
    };

    auto addMayaUsdProxyShapeNativePrefix = [&](const MDagPath& proxyDag, bool forceVis) {
        if (!proxyDag.isValid()) {
            return;
        }

        const SdfPath proxyRprimPrefix = _mayaHydraSceneIndex->GetPrimPath(proxyDag, false);
        if (proxyRprimPrefix.IsEmpty() || !proxyRprimPrefix.HasPrefix(MAYA_NATIVE_ROOT)) {
            return;
        }

        addPrimPath(proxyRprimPrefix, forceVis);
        const TfToken underscoredName(proxyRprimPrefix.GetNameToken().GetString() + "_");
        addPrimPath(proxyRprimPrefix.GetParentPath().AppendChild(underscoredName), forceVis);
    };

    // Native visual rprims (camera gizmos, light shapes) live in the frame-pass render index
    // that feeds the viewport; fall back to the scene index render index when the frame-pass
    // index is unavailable.
    HdRenderIndex* renderIndexForScan = renderIndex(0);
    if (!renderIndexForScan) {
        renderIndexForScan = _mayaHydraSceneIndex->GetRenderIndexPtr();
    }

    bool needDefaultUfeProxyCamera = false;
    const SdfPath rprimRoot = MAYA_NATIVE_ROOT.AppendChild(TfToken("rprims"));

    // Pre-collect all native rprim paths once from both the render index
    // and the scene index tree so all subsequent matching is
    // O(numCachedNativeRprims) without repeated full scans of all rprims.
    std::vector<SdfPath> nativeRprims;
    {
        TfHashSet<SdfPath, SdfPath::Hash> seen;
        if (renderIndexForScan) {
            for (const SdfPath& id : renderIndexForScan->GetRprimIds()) {
                if (id.HasPrefix(MAYA_NATIVE_ROOT) && seen.insert(id).second) {
                    nativeRprims.push_back(id);
                }
            }
        }
        for (const SdfPath& p :
             HdSceneIndexPrimView(_mayaHydraSceneIndex, rprimRoot)) {
            if (seen.insert(p).second) {
                nativeRprims.push_back(p);
            }
        }
    }

    static const std::string kDefaultUfeProxyToken("defaultUfeProxyCamera");
    static const std::string kUfeLightProxy("ufeLightProxy");

    auto addMayaCameraDrawables = [&](const MDagPath& camShapeDag, bool forceVis) {
        if (!camShapeDag.isValid() || !camShapeDag.hasFn(MFn::kCamera)) {
            return;
        }
        const SdfPath cameraSprimPath = _mayaHydraSceneIndex->GetPrimPath(camShapeDag, true);
        addPrimPath(cameraSprimPath, forceVis);

        MDagPath xfDag = camShapeDag;
        xfDag.pop();
        const SdfPath xfRprimPrefix = _mayaHydraSceneIndex->GetPrimPath(xfDag, false);
        const SdfPath shapeRprimPrefix = _mayaHydraSceneIndex->GetPrimPath(camShapeDag, false);

        // Isolate visibility matches via ancestor/descendant prefix on paths in the selection map.
        // Always register these Hydra prefixes, not only concrete rprim ids from GetRprimIds():
        // USD camera frustum/body rprims (e.g. .../Camera1_cameraBody_*) are often absent from
        // GetRprimIds() at isolate-build time, or may appear in a different HdRenderIndex than
        // renderIndexForScan, but still resolve under the same DAG-derived prefixes at draw time.
        if (!xfRprimPrefix.IsEmpty()) {
            addPrimPath(xfRprimPrefix, forceVis);
        }
        if (!shapeRprimPrefix.IsEmpty()) {
            addPrimPath(shapeRprimPrefix, forceVis);
        }

        for (const SdfPath& id : nativeRprims) {
            if ((!xfRprimPrefix.IsEmpty() && id.HasPrefix(xfRprimPrefix))
                || (!shapeRprimPrefix.IsEmpty() && id.HasPrefix(shapeRprimPrefix))
                || id.GetString().find(kDefaultUfeProxyToken) != std::string::npos)
            {
                addPrimPath(id, forceVis);
            }
        }
    };

    // Frustum/body gizmos for a selected USD camera are authored under the *panel* camera's branch
    // in Hydra (e.g. .../cameraShape_1/.../mayaUsdProxyShape1_/Camera1_cameraBody_*), not only under
    // defaultUfeProxyCamera. We add the DAG-derived rprim prefix paths themselves (see
    // addMayaCameraDrawables) so isolate select matches descendants even when GetRprimIds() is
    // incomplete at selection time; we still scan ids for any additional paths under those prefixes.
    if (panelCameraDag.isValid() && panelCameraDag.hasFn(MFn::kCamera)) {
        addMayaCameraDrawables(panelCameraDag, /*forceVis=*/true);
    }

    for (const Ufe::Path& ufePath : selectedUfePaths) {
        // Filter to the USD UFE clients we know how to expand for.  Non-USD
        // view-selected paths are handled by upstream isolate-selection only.
        if (ufePath.empty()
            || ufePath.runTimeId() != MayaUsdAPI::getUsdRunTimeId()) {
            continue;
        }
        const UsdPrim prim = MayaUsdAPI::ufePathToPrim(ufePath);
        if (!prim.IsValid()) {
            continue;
        }
        // UsdLuxLightAPI covers all USD light types.  Ufe::Light2::light()
        // cannot be used here because its handler only supports RectLight.
        const bool isLight = prim.HasAPI<UsdLuxLightAPI>();
        bool isCamera = false;
        if (!isLight) {
            auto sceneItem = Ufe::Hierarchy::createItem(ufePath);
            isCamera = sceneItem && (Ufe::Camera::camera(sceneItem) != nullptr);
            if (!isCamera) {
                continue;
            }
        }

        // Only force gizmo render items visible when the USD prim itself
        // is visible.  If the user has hidden the prim, its gizmo should
        // stay hidden during isolate select (matching VP2 behavior).
        UsdGeomImageable imageable(prim);
        const bool forceVis = !imageable
            || imageable.ComputeVisibility() != UsdGeomTokens->invisible;

        // Include the MayaUsd proxy shape's native Hydra rprim prefix so
        // the proxy shape itself stays visible during isolate select.
        // This prefix (e.g. .../rprims/stage1/stageShape1) is NOT an
        // ancestor of individual camera/light body rprims (which live
        // under a different source-DAG branch), so it does not make
        // unrelated native rprims visible.
        const MDagPath proxyDag = resolveProxyDag(ufePath);
        addMayaUsdProxyShapeNativePrefix(proxyDag, forceVis);

        // Generic native rprim search for camera gizmos and light shapes
        // that Maya creates as render items under MAYA_NATIVE_ROOT.
        // These rprims have paths:
        //   rprims/{source_dag}/{proxy_dag_}/{PrimName}_<suffix>_<id>
        // Search all native rprims for paths whose leaf name starts with
        // the USD prim name and whose path contains the proxy shape name.
        {
            const std::string primNamePrefix = prim.GetName().GetString() + "_";

            std::string proxyPathToken;
            if (proxyDag.isValid()) {
                const SdfPath proxyRprimPfx = _mayaHydraSceneIndex->GetPrimPath(proxyDag, false);
                if (!proxyRprimPfx.IsEmpty() && proxyRprimPfx.HasPrefix(rprimRoot)) {
                    proxyPathToken = proxyRprimPfx.GetNameToken().GetString();
                }
            }

            std::string proxySlash;
            std::string proxyUnderscore;
            if (!proxyPathToken.empty()) {
                proxySlash      = "/" + proxyPathToken + "/";
                proxyUnderscore = "/" + proxyPathToken + "_";
            }

            TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                .Msg("    _Expand: searching for primNamePrefix='%s' proxyPathToken='%s'\n",
                     primNamePrefix.c_str(), proxyPathToken.c_str());

            for (const SdfPath& id : nativeRprims) {
                if (id.GetNameToken().GetString().compare(
                        0, primNamePrefix.size(), primNamePrefix) != 0) {
                    continue;
                }
                if (!proxyPathToken.empty()) {
                    const std::string& pathStr = id.GetString();
                    if (pathStr.find(proxyUnderscore) == std::string::npos
                        && pathStr.find(proxySlash) == std::string::npos) {
                        continue;
                    }
                }
                TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                    .Msg("    _Expand: native rprim match: %s\n", id.GetText());
                addPrimPath(id, forceVis);
            }
        }

        if (isCamera) {
            // Camera-specific handling: pulled cameras and defaultUfeProxyCamera.
            MDagPath pulledDag;
            const bool pulledOk = ReadMayaDagPathFromUsdPullMetadata(
                prim, pulledDag) && pulledDag.isValid();
            if (pulledOk && pulledDag.hasFn(MFn::kCamera)) {
                addMayaCameraDrawables(pulledDag, forceVis);
            } else {
                needDefaultUfeProxyCamera = true;
            }
        } else {
            // Light-specific handling: ufeLightProxy gizmo rprims.
            // The drawUfe plugin creates internal Maya proxy light nodes
            // (ufeLightProxy{N} / ufeLightProxyGizmo{N}) for each USD light.
            // Their rprim paths follow the pattern:
            //   rprims/ufeLightProxy{N}/ufeLightProxyGizmo{N}/Gizmo_{id}
            // which does not match the generic primNamePrefix search above.
            // NOTE: when multiple USD lights exist this currently includes all
            // proxy gizmos, not only the one for the selected light, because the
            // proxy nodes carry no attribute linking them back to a specific
            // USD prim.  This is acceptable: the IsolateSelectSceneIndex already
            // keeps excluded lights contributing lighting (visOff is skipped for
            // light prims), so extra gizmo geometry is the only side-effect.
            for (const SdfPath& id : nativeRprims) {
                if (id.GetString().find(kUfeLightProxy) != std::string::npos) {
                    TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                        .Msg("    _Expand: ufeLightProxy rprim match: %s\n",
                             id.GetText());
                    addPrimPath(id, forceVis);
                }
            }
        }
    }

    if (!needDefaultUfeProxyCamera) {
        return forceVisiblePaths;
    }

    static const MString kDefaultUfeProxyCameraShapePath(
        "|defaultUfeProxyCameraTransformParent|defaultUfeProxyCameraTransform|defaultUfeProxyCameraShape");
    MSelectionList sl;
    if (sl.add(kDefaultUfeProxyCameraShapePath) != MS::kSuccess) {
        return forceVisiblePaths;
    }
    MDagPath camDag;
    if (sl.getDagPath(0, camDag) != MS::kSuccess || !camDag.isValid()) {
        return forceVisiblePaths;
    }
    addMayaCameraDrawables(camDag, /*forceVis=*/true);
    return forceVisiblePaths;
}

/* static */
void MtohRenderOverride::_ViewSelectedChangedCb(
    const MString& viewName,
    bool           viewSelectedObjectsChanged,
    void*          data
)
{
    // For simplicity, we leave the view selected changed callback active even
    // when Maya Hydra isn't the renderer, and early out.  Another option would
    // be to add and remove the callback in _InitHydraResources() and
    // ClearHydraResources().
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }
    if (!instance->_initializationSucceeded) {
        return;
    }

    auto& nbCalls = instance->_nbViewSelectedChangedCalls;
    ++nbCalls;
    Fvp::Instruments::instance().set(
        kNbViewSelectedChangedCalls, VtValue(nbCalls));

    M3dView view;
    if (!TF_VERIFY(M3dView::getM3dViewFromModelPanel(viewName, view) == MS::kSuccess,
                   "No view found for view name %s.", viewName.asChar())) {
        return;
    }

    // Every MtohRenderOverride registers this callback; all of them fire for each panel. Only the
    // override that is actually driving the panel may call ReplaceIsolateSelection — otherwise a
    // different instance can overwrite the isolate set without USD-camera native rprim expansion
    // (wrong HdRenderIndex / scene index), hiding e.g. Camera1_cameraBody_* under MAYA_NATIVE_ROOT.
    MStatus panelRoStatus;
    const MString panelRenderOverride = view.renderOverrideName(&panelRoStatus);
    if (panelRoStatus == MS::kSuccess && panelRenderOverride.length() > 0) {
        if (panelRenderOverride != instance->name()) {
            TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                .Msg(
                    "  _ViewSelectedChangedCb skip panel=%s: active override=%s != this instance "
                    "%s\n",
                    viewName.asChar(),
                    panelRenderOverride.asChar(),
                    instance->name().asChar());
            return;
        }
    }

    auto found = instance->_isolateSelectState.find(viewName.asChar());
    if (found == instance->_isolateSelectState.end()) {
        found = instance->_isolateSelectState.insert(found, VpIsolateSelectStates::value_type(viewName.asChar(), IsolateSelectState::IsolateSelectOff));
    }

    auto isolateStateLabel = [](IsolateSelectState s) -> const char* {
        switch (s) {
        case IsolateSelectState::IsolateSelectOff: return "IsolateSelectOff";
        case IsolateSelectState::IsolateSelectPendingObjects: return "IsolateSelectPendingObjects";
        case IsolateSelectState::IsolateSelectOn: return "IsolateSelectOn";
        default: return "IsolateSelectUnknown";
        }
    };

    TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
        .Msg(
            "_ViewSelectedChangedCb panel=%s viewSelectedObjectsChanged=%d state=%s viewSelected=%d "
            "numViewSelectedObjects=%u\n",
            viewName.asChar(),
            viewSelectedObjectsChanged ? 1 : 0,
            isolateStateLabel(found->second),
            view.viewSelected() ? 1 : 0,
            view.numViewSelectedObjects());

    // The M3dView returns the list of view selected objects as strings.
    // If isolate select is turned off, we want to disable isolate selection.
    // Otherwise, replace with what is in the M3dView.
    auto& isolateSelectMgr = Fvp::IsolateSelectManager::Get();
    if (!view.viewSelected()) {
        TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
            .Msg("  isolate select disabled for panel=%s\n", viewName.asChar());
        // Set state to Off BEFORE notifying.  DisableIsolateSelection triggers
        // _SendPrimsDirtied, which can synchronously re-enter this callback via
        // Hydra→Maya observers (more likely with larger dirty sets, i.e. when
        // numViewSelectedObjects > 1).  If the re-entrant call reads state==On
        // it falls through to the "stay on, notify" branch and rebuilds the
        // isolate set from the still-stale view-selected list, leaving the
        // outer disable in an inconsistent state.  Setting the state first
        // makes any re-entry see Off and take the correct path.
        found->second = IsolateSelectState::IsolateSelectOff;
        // DisableIsolateSelection also clears the per-viewport force-visible
        // paths inside the manager, so no separate Clear call is needed.
        isolateSelectMgr.DisableIsolateSelection(viewName.asChar());
        return;
    }

    // Deal with IsolateSelectState changes.  The messages we receive are
    // viewSelectedObjectsChanged true or false.
    //
    // State transitions:
    //
    // off: message false --> go to pendingObjects, do not notify.
    // off: message true --> illegal, warn, do not notify.
    //
    // pendingObjects: message false --> illegal, warn, do not notify.
    // pendingObjects: message true --> notify, go to on.
    //
    // on: message false --> go to off, notify.
    // on: message true --> stay on, notify.

    if (found->second == IsolateSelectState::IsolateSelectOff) {
        if (TF_VERIFY(!viewSelectedObjectsChanged)) {
            found->second = IsolateSelectState::IsolateSelectPendingObjects;
        }
        TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
            .Msg(
                "  state transition -> IsolateSelectPendingObjects (waiting for object list); panel=%s\n",
                viewName.asChar());
        return;
    }
    else if (found->second == IsolateSelectState::IsolateSelectPendingObjects) {
        if (!TF_VERIFY(viewSelectedObjectsChanged)) {
            TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                .Msg(
                    "  expected viewSelectedObjectsChanged while PendingObjects; skipping isolate update "
                    "panel=%s\n",
                    viewName.asChar());
            return;
        }
        found->second = IsolateSelectState::IsolateSelectOn;
        TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
            .Msg("  state transition -> IsolateSelectOn; building isolate set panel=%s\n", viewName.asChar());
    }

    TF_VERIFY(found->second == IsolateSelectState::IsolateSelectOn);

    // The M3dView returns the list of view selected objects as strings.
    // Loop over the view selected objects and try to create UFE paths from
    // them.  When objectStrings has a single element it is a regular object
    // selection.  When it has more than one element, Maya is using a component
    // representation — this happens for point instances of the same
    // PointInstancer when multiple are selected simultaneously (each string is
    // the UFE path of one instance).  We handle both cases by iterating over
    // all strings in the array.  The single-element case where that one string
    // itself encodes a component range (e.g. "pMesh1.f[0:10]") is not yet
    // supported and is logged as a warning.
    auto isolateSelection = std::make_shared<Fvp::Selection>();
    const auto nbObjects = view.numViewSelectedObjects();
    TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
        .Msg("  building isolate selection from %u view-selected object(s)\n", nbObjects);

    std::vector<Ufe::Path> selectedUfePaths;
    selectedUfePaths.reserve(nbObjects);
    for (unsigned int i = 0; i < nbObjects; ++i) {
        MStringArray objectStrings;
        TF_VERIFY(view.viewSelectedObject(i, objectStrings) == MS::kSuccess);

        // Iterate over every string in the array.  For a normal object there
        // is exactly one string.  For multiple point instances from the same
        // PointInstancer there is one UFE-path string per selected instance.
        for (unsigned int j = 0; j < objectStrings.length(); ++j) {
            const char* ufeStr = objectStrings[j].asChar();

            // A string that contains a Maya-component separator ('.') is a
            // legacy component selector (e.g. "pMesh1.f[0]") that we don't
            // support yet.
            if (std::strchr(ufeStr, '.') != nullptr) {
                TF_WARN("Isolate select: unsupported component selector '%s' "
                        "skipped.", ufeStr);
                continue;
            }

            TF_DEBUG(FVP_ISOLATE_SELECT_VIEW_SELECTED)
                .Msg("  [%u][%u] viewSelectedObject UFE string: %s\n", i, j, ufeStr);
            auto path = Ufe::PathString::path(ufeStr);
            auto primSelections = Fvp::ufePathToPrimSelections(path);

            _LogPrimSelectionsForViewSelectedIsolate(ufeStr, primSelections);
            for (const auto& primSelection : primSelections) {
                isolateSelection->Add(primSelection);
            }
            if (!path.empty()) {
                selectedUfePaths.push_back(std::move(path));
            }
        }
    }

    MDagPath panelCameraDag;
    if (view.getCamera(panelCameraDag) != MS::kSuccess || !panelCameraDag.isValid()) {
        panelCameraDag = MDagPath();
    }

    TfHashSet<SdfPath, SdfPath::Hash> forceVisiblePaths = instance->_ExpandIsolateSelectionForUsdPrims(
        *isolateSelection, selectedUfePaths, panelCameraDag);

    // Store the force-visible paths in the manager BEFORE calling
    // ReplaceIsolateSelection: ReplaceIsolateSelection switches the shared
    // scene index to this viewport and pushes the corresponding per-viewport
    // force-visible set, so it must already be present in the manager.
    isolateSelectMgr.SetForceVisiblePaths(viewName.asChar(), std::move(forceVisiblePaths));
    isolateSelectMgr.ReplaceIsolateSelection(viewName.asChar(), isolateSelection);
}
#endif

std::shared_ptr<const MayaHydraSceneIndexRegistry>
MtohRenderOverride::sceneIndexRegistry() const
{
    return _sceneIndexRegistry;
}

std::string MtohRenderOverride::renderIndexName(int passIndex /*= 0*/) const
{
    if (passIndex < 0 || passIndex >= static_cast<int>(_framePassesData.size())) {
        TF_CODING_ERROR(
            "Invalid pass index %d, must be in range [0, %d)",
            passIndex,
            static_cast<int> (_framePassesData.size()));
        return nullptr;
    }
    return (_framePassesData[passIndex]) 
        ? _framePassesData[passIndex]->_rendererName.GetString()
        : "";
}

HdRenderIndex* MtohRenderOverride::renderIndex(int passIndex /*= 0*/) const
{
    const int numPasses = static_cast<int>(_framePassesData.size());
    if (passIndex < 0 || passIndex >= numPasses)
        {
            TF_CODING_ERROR(
                "Invalid pass index %d, must be in range [0, %d)",
                passIndex, numPasses);
        return nullptr;
    }
    return (_framePassesData[passIndex] && _framePassesData[passIndex]->HasRenderIndexProxy())
        ? _framePassesData[passIndex]->GetRenderIndexProxy()->RenderIndex()
        : nullptr;
}

const hvt::FramePassPtr&
MtohRenderOverride::_GetFramePass(int passIndex)const
{   
    if (passIndex < 0 || passIndex >= static_cast<int> (_framePassesData.size())
        || !_framePassesData[passIndex]
        || !_framePassesData[passIndex]->IsValid()) {
        static const hvt::FramePassPtr nullFramePass;
        TF_CODING_ERROR("Invalid pass index: %d", passIndex);
        return nullFramePass;
    }
    return _framePassesData[passIndex]->GetFramePass();
}

hvt::FramePassPtr& MtohRenderOverride::_GetFramePass(int passIndex)
{
    if (passIndex < 0 || passIndex >= static_cast<int> (_framePassesData.size())
        || !_framePassesData[passIndex]
        || !_framePassesData[passIndex]->IsValid()) {
        static hvt::FramePassPtr nullFramePass;
        return nullFramePass;
    }
    return _framePassesData[passIndex]->_framePass;
}

int MtohRenderOverride::_GetNumFramePasses()const
{
    return static_cast<int>(_framePassesData.size());
}

int MtohRenderOverride::_GetNumVisibleFramePasses() const
{
    const MIntArray& framePassesVisible
        = MayaHydraSetVisibleFramePasses::getVisibleFramePasses();

    return framePassesVisible.length();
}

void MtohRenderOverride::_ClearFramePassesData()
{
    _framePassesData.clear();
}

void MtohRenderOverride::_CreateFramePass(
    const std::string& rendererName,
    const SdfPath&     passId,
    const int passIndex)
{
    // Create renderer
    hvt::RendererDescriptor rendererDescriptor;
    rendererDescriptor.hgiDriver    = &_hgiDriver;
    rendererDescriptor.rendererName = rendererName;

    hvt::RenderIndexProxyPtr renderer;
    hvt::ViewportEngine::CreateRenderer(renderer, rendererDescriptor);

    // Create frame pass
    hvt::FramePassDescriptor framePassDescriptor;
    framePassDescriptor.renderIndex = renderer->RenderIndex();
    framePassDescriptor.uid         = passId;
    auto framePass                  = hvt::ViewportEngine::CreateFramePass(framePassDescriptor);

    // Remove the default selection tasks as we do not use them.
    framePass->GetTaskManager()->RemoveTask(HdxPrimitiveTokens->colorizeSelectionTask);
    framePass->GetTaskManager()->RemoveTask(TfToken("selectionTask"));

    // Update the consolidated frame pass data
    _framePassesData[passIndex]->_renderIndexProxy = renderer;
    _framePassesData[passIndex]->_framePass = std::move(framePass);
}

void MtohRenderOverride::_CreateFramePasses()
{
    // Keep the number of passes to create
    const int numPasses = static_cast<int> (_framePassesData.size());

    // Create passes for each renderer
    for (int i = 0; i < numPasses; ++i) {
        std::string passNumber = std::string("/Pass") + std::to_string(i);
        _CreateFramePass(_framePassesData[i]->_rendererName.GetString(), SdfPath(passNumber), i);
    }
}

void MtohRenderOverride::_CreateNonMainFramePassesFilteringSceneIndices()
{
    // Note that we start at the second pass index
    // because the main pass is already done
    constexpr int secondFramePassIndex = 1; 
    for (int i = secondFramePassIndex; i < _GetNumFramePasses(); ++i) {
        const auto& pass = _GetFramePass(i);
        if (!pass || ! (pass->GetRenderIndex())) {
            continue;
        }
        
        pass->GetRenderIndex()->InsertSceneIndex(
            _CreatePassFilteringSceneIndex(_framePassesData[i]),
            SdfPath::AbsoluteRootPath());
    }
}


//This is where the passes and their information is created
void MtohRenderOverride::_CreateFramePassesData()
{ 
    _ClearFramePassesData();

    // Check if we should use single frame pass when using the same renderer
    static const bool _useSingleFramePass = useSingleFramePass();
    const bool shouldUseSingleFramePass = _useSingleFramePass && 
        (_rendererDesc.rendererName == MtohTokens->HdStormRendererPlugin);

    // Main pass
    {
        auto filteringData = std::make_shared<Fvp::FramePassData>();
        filteringData->_rendererName = _rendererDesc.rendererName;//Render delegate chosen by the user
        filteringData->_includePaths = {};
        filteringData->_excludePaths = (shouldUseSingleFramePass) 
                                        ? SdfPathVector{}
                                        : SdfPathVector{_highlightHierarchyPrefix}; // Ignore selection highlight prims if we have multiple passes
        filteringData->_removeLights = false; // Keep all lights in this pass
        filteringData->_supportPrimsWithNoPurposeRenderTag
            = true; // Main graphics pass supports prims with no purpose render tag
        
        _framePassesData.emplace_back(filteringData);
        
        // Define the render tags update function after emplacing, capturing shared ptr to the element
        const size_t currentIndex = _framePassesData.size() - 1;
        _framePassesData[currentIndex]->_renderTagsUpdateFn = [this, currentIndex, shouldUseSingleFramePass](
                  bool includeRenderPurpose, bool includeProxyPurpose, bool includeGuidePurpose) {
            auto& filteringData = _framePassesData[currentIndex];
            filteringData->_includeRenderTags = { HdRenderTagTokens->geometry }; // main pass
            if (includeRenderPurpose) {
                filteringData->_includeRenderTags.insert(HdRenderTagTokens->render); // main pass
            }
            if (includeProxyPurpose) {
                filteringData->_includeRenderTags.insert(HdRenderTagTokens->proxy); // main pass
            }

            if (shouldUseSingleFramePass) { 
                // When using a single pass, everything should be included in the main pass
                filteringData->_includeRenderTags.insert(Fvp::secondaryGraphicsRenderTagToken);
                // Include guide tags in the main pass
                if (includeGuidePurpose) {
                    filteringData->_includeRenderTags.insert(HdRenderTagTokens->guide);
                }
            }
        };
    }

    // Secondary graphics pass - only create if not using single frame pass
    if (!shouldUseSingleFramePass) {
        auto filteringData = std::make_shared<Fvp::FramePassData>();
        filteringData->_rendererName = MtohTokens->HdStormRendererPlugin;//Storm by default
        filteringData->_includePaths = { _highlightHierarchyPrefix, MayaViewportSceneIndex::DefaultLightPath() }; // include selection highlight prims.
        filteringData->_excludePaths = { };
        filteringData->_removeLights = true; // Remove lights from this pass except for the default light, kept through the include paths
        filteringData->_supportPrimsWithNoPurposeRenderTag
            = false; // Secondary graphics pass does not support prims with no purpose render tag
        _framePassesData.emplace_back(filteringData);

         // Define the render tags update function after emplacing, capturing shared ptr to the
        // element
        const size_t currentIndex = _framePassesData.size() - 1;
        _framePassesData[currentIndex]->_renderTagsUpdateFn
            = [this, currentIndex](
                  bool includeRenderPurpose, bool includeProxyPurpose, bool includeGuidePurpose) {
                  auto& filteringData = _framePassesData[currentIndex];
                  
                  // Set the render tags for the secondary graphics pass
                  filteringData->_includeRenderTags = { Fvp::secondaryGraphicsRenderTagToken };

                  if (includeGuidePurpose) {
                      // Insert guide tag (std::set automatically handles duplicates)
                      filteringData->_includeRenderTags.insert(HdRenderTagTokens->guide);
                  }
               };
    }
}

void MtohRenderOverride::_SetRenderPurposeTags(const MayaHydraParams& delegateParams)
{
    const bool renderPurpose = delegateParams.renderPurpose;
    const bool proxyPurpose = delegateParams.proxyPurpose;
    const bool guidePurpose = delegateParams.guidePurpose;

    // Update the render tags for each pass
    const int numFramePassesData = static_cast<int>(_framePassesData.size());
    for (int i = 0; i < numFramePassesData; ++i) {
        auto& filteringData = _framePassesData[i];
        if (filteringData && filteringData->_renderTagsUpdateFn) {
            filteringData->_renderTagsUpdateFn(renderPurpose, proxyPurpose, guidePurpose);
        }
    }

    _purposeFilteringSceneIndex->UpdatePrimsFromIncludedPurposes(RenderGlobalsUtils::GetIncludedPurposes());
}

void MtohRenderOverride::_SetCurrentFrameInHydraGlobalSceneIndex(double currentFrame)
{
    if (!TF_VERIFY(_sceneGlobalsSceneIndex, "Scene globals scene index not yet initialized")) {
        return;
    }

    _sceneGlobalsSceneIndex->SetCurrentFrame(currentFrame);
}

PXR_NAMESPACE_CLOSE_SCOPE
