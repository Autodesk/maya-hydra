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
// Copyright 2023 Autodesk
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
#ifndef MTOH_VIEW_OVERRIDE_H
#define MTOH_VIEW_OVERRIDE_H

#include "renderGlobals.h"
#include "pluginUtils.h"

#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexDataFactoriesSetup.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mayaViewportSceneIndex.h>
#include <mayaHydraLib/mhWireframeColorInterfaceImp.h>
#include <mayaHydraLib/mhLeadObjectPathTracker.h>
#include <mayaHydraLib/sceneIndex/mhDirtyLeadObjectSceneIndex.h>
#include <mayaHydraLib/pick/mhPickHandlerFwd.h>
#include <mayaHydraLib/pick/mhPickContext.h>
#include <mayaHydraLib/pick/mhPickHitFwd.h>

#include <flowViewport/fvpFramePassData.h>
#include <flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h>
#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>
#include <flowViewport/selection/fvpSelectionTracker.h>
#include <flowViewport/selection/fvpSelectionFwd.h>
#include <flowViewport/sceneIndex/fvpDisplayStyleOverrideSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPruneTexturesSceneIndex.h>
#include <flowViewport/sceneIndex/fvpDefaultMaterialSceneIndex.h>
#include <flowViewport/sceneIndex/fvpReprSelectorSceneIndex.h>
#include <flowViewport/sceneIndex/fvpBlockPrimRemovalPropagationSceneIndex.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpGeomSubsetWhSi.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpMeshWhSi.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpNiInstanceWhSi.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpNiPrototypeWhSi.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpPiInstancerWhSi.h>
#include <flowViewport/sceneIndex/wireframeHighlights/fvpPiPrototypeWhSi.h>
#include <flowViewport/sceneIndex/fvpLightsManagementSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPruningSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPurposeFilteringSceneIndex.h>
#include <flowViewport/sceneIndex/fvpBBoxSceneIndex.h>

#include <pxr/base/tf/singleton.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h>
#include <pxr/imaging/hdSt/renderDelegate.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/hdsi/sceneGlobalsSceneIndex.h>
#include <pxr/pxr.h>

#include <maya/MCallbackIdArray.h>
#include <maya/MString.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <map>

#include <ufe/ufe.h>
UFE_NS_DEF {
class SelectionChanged;
class Selection;
}

PXR_NAMESPACE_OPEN_SCOPE
// Remove this using statement once the following code is moved into the MayaHydra namespace
using MtohRendererDescription = MayaHydra::MtohRendererDescription;

using HgiUniquePtr = std::unique_ptr<class Hgi>;
class MayaHydraSceneIndexRegistry;
class MayaHydraSceneDelegate;

/*! \brief MtohRenderOverride is a rendering override class for the viewport to use Hydra instead of
 * VP2.0.
 */
class MtohRenderOverride : public MHWRender::MRenderOverride,
    public MayaHydra::PickContext
{
public:
#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    static constexpr char kNbViewSelectedChangedCalls[]
        = "MtohRenderOverride:NbViewSelectedChangedCalls";
#endif

    MtohRenderOverride(const MtohRendererDescription& desc);
    ~MtohRenderOverride() override;

    /// Mark a setting (or all settings when attrName is '') as out of date
    static void UpdateRenderGlobals(const MtohRenderGlobals& globals, const TfToken& attrName = {});

    /// The names of all render delegates that are being used by at least
    /// one modelEditor panel.
    static std::vector<MString> AllActiveRendererNames();

    /// Returns the names of all AOVs made available by the render delegates
    /// for a given render pass index.
    /// TODO 2025-08-29 : This currently gathers AOVs from all viewports indiscriminately.
    /// Once we have proper multi-viewport support, we should also be able to
    /// specify which viewport to get the AOVs for.
    static TfTokenVector GetAvailableFramePassAovs(int passIndex);

    static MtohRenderOverride* GetByName(TfToken rendererName);

    /// Returns a list of rprims in the render index for the given render
    /// delegate.
    ///
    /// Intended mostly for use in debugging and testing.
    static SdfPathVector RendererRprims(TfToken rendererName, bool visibleOnly = false);

    /// Returns the scene delegate id for the given render delegate and
    /// scene delegate names.
    ///
    /// Intended mostly for use in debugging and testing.
    static SdfPath RendererSceneDelegateId(TfToken rendererName, TfToken sceneDelegateName);

    /// Returns whether the given renderer has converged.
    /// TODO 2025-10-21 : This currently only checks the first viewport found
    /// that uses the given renderer. Once we have proper multi-viewport support, 
    /// we should also be able to specify which viewport to check the convergence for.
    ///
    /// Intended mostly for use in debugging and testing.
    static bool HasConverged(TfToken rendererName);

    //! Main entry point for rendering, called by Maya.
    MStatus Render(
        const MHWRender::MDrawContext&                         drawContext,
        const MHWRender::MDataServerOperation::MViewportScene& scene);

    /// When fullReset is true, we remove the data producer scene indices that apply to all
    /// viewports and the scene index registry where the usd stages have been loaded. It means you
    /// are doing a full reset of hydra such as when doing "File New". Use fullReset = false when
    /// you still want to see the previously registered data producer scene indices when using an
    /// hydra viewport.
    void ClearHydraResources(bool fullReset);
    void SelectionChanged(const Ufe::SelectionChanged& notification);
    void SetRenderPurposeTags(const MayaHydraParams& delegateParams)
    {
        _SetRenderPurposeTags(delegateParams);
    };
    MString uiName() const override { return MString(_rendererDesc.displayName.GetText()); }

    MHWRender::DrawAPI supportedDrawAPIs() const override;

    MStatus setup(const MString& destination) override;
    MStatus cleanup() override;

    // Utility function to get GPU memory usage stats
    static int GetUsedGPUMemory();

    // Returns scene statistics as a map for the currently active render delegate from Hydra primitives
    static std::map<std::string, int> GetSceneStatistics();

    bool                         startOperationIterator() override;
    MHWRender::MRenderOperation* renderOperation() override;
    bool                         nextRenderOperation() override;

    bool select(
        const MHWRender::MFrameContext&  frameContext,
        const MHWRender::MSelectionInfo& selectInfo,
        bool                             useDepth,
        MSelectionList&                  selectionList,
        MPointArray&                     worldSpaceHitPts) override;

    // MayaHydra::PickContext overrides.
    std::shared_ptr<const MayaHydraSceneIndexRegistry> sceneIndexRegistry() const override;

    std::string renderIndexName(int passIndex = 0) const;

    HdRenderIndex* renderIndex(int passIndex = 0) const override;
    int            getNumFramePasses() const { return _GetNumFramePasses(); }

private:
    typedef std::pair<MString, MCallbackIdArray> PanelCallbacks;
    typedef std::vector<PanelCallbacks>          PanelCallbacksList;

    void _InitHydraResources(
        const MHWRender::MDrawContext& drawContext,
        const MayaHydraParams&         delegateParams);
    void              _RemovePanel(MString panelName);
    void              _DetectMayaDefaultLighting(const MHWRender::MDrawContext& drawContext);
    HdRenderDelegate* _GetRenderDelegate(int renderPassIndex = 0);
    HdRenderDelegate* _GetRenderDelegate(int renderPassIndex = 0) const;
    void              _ClearMayaHydraSceneIndex();
    void              _SetCurrentFrameInHydraGlobalSceneIndex(double currentFrame);

    void              _SetRenderPurposeTags(const MayaHydraParams& delegateParams);
    void _CreateSceneIndicesChainAfterMergingSceneIndex(const MHWRender::MDrawContext& drawContext);
    HdSceneIndexBaseRefPtr _CreatePassFilteringSceneIndex(Fvp::FramePassDataPtr& filteringData);
    VtValue _GetUsedGPUMemory() const;

    void _PickByRegion(
        MayaHydra::PickHitVector& outHits,
        const MMatrix&    viewMatrix,
        const MMatrix&    projMatrix,
        bool              singlePick,
        const TfToken&    geomSubsetsPickMode,
        bool              pointSnappingActive,
        int               view_x,
        int               view_y,
        int               view_w,
        int               view_h,
        unsigned int      sel_x,
        unsigned int      sel_y,
        unsigned int      sel_w,
        unsigned int      sel_h);

    inline PanelCallbacksList::iterator _FindPanelCallbacks(MString panelName)
    {
        // There should never be that many render panels, so linear iteration
        // should be fine

        return std::find_if(
            _renderPanelCallbacks.begin(),
            _renderPanelCallbacks.end(),
            [&panelName](const PanelCallbacks& item) { return item.first == panelName; });
    }

    void _PopulateSelectionList(
        const MayaHydra::PickHitVector&    hits,
        const MHWRender::MSelectionInfo& selectInfo,
        MSelectionList&                  selectionList,
        MPointArray&                     worldSpaceHitPts,
        bool&                            isOneMayaNodeInComponentsPickingMode);

    void _AddPluginSelectionHighlighting();

    // Determine the pick handler which should handle a pick hit, to transform
    // the pick hit into a selection.
    MayaHydra::PickHandlerConstPtr _PickHandler(const MayaHydra::PickHit& hit) const;

    // Callbacks
    static void _ClearHydraCallback(void* data);
    static void _TimerCallback(float, float, void* data);
    static void _PlayblastingChanged(bool state, void*);
    static void _PanelDeletedCallback(const MString& panelName, void* data);
    static void _TimeChangedCallback(void* data);
    static void _RendererChangedCallback(
        const MString& panelName,
        const MString& oldRenderer,
        const MString& newRenderer,
        void*          data);
    static void _RenderOverrideChangedCallback(
        const MString& panelName,
        const MString& oldOverride,
        const MString& newOverride,
        void*          data);
#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    static void
    _ViewSelectedChangedCb(const MString& panelName, bool viewSelectedObjectsChanged, void* data);
#endif

    MtohRendererDescription _rendererDesc;

    std::shared_ptr<MayaHydraSceneIndexRegistry>              _sceneIndexRegistry;
    std::vector<std::unique_ptr<MHWRender::MRenderOperation>> _operations;
    MCallbackIdArray                                          _callbacks;
    MCallbackId                                               _timerCallback = 0;
    MCallbackId                                               _timeChangeCallback = 0;
    PanelCallbacksList                                        _renderPanelCallbacks;
    const MtohRenderGlobals&                                  _globals;

#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    MCallbackId _viewSelectedChangedCb { 0 };
#endif

    std::mutex                            _lastRenderTimeMutex;
    std::chrono::system_clock::time_point _lastRenderTime;
    std::atomic<bool>                     _playBlasting = { false };
    std::atomic<bool>                     _isConverged = { false };
    std::atomic<bool>                     _needsClear = { false };

    /// Hgi and HdDriver should be constructed before HdEngine to ensure they
    /// are destructed last. Hgi may be used during engine/delegate destruction.
    HgiUniquePtr _hgi;
    HdDriver     _hgiDriver;

    // Data per pass - each FramePassData contains both configuration data and the actual FramePass
    // This ensures they stay synchronized and eliminates index-based access issues
    Fvp::FramePassDataPtrVector                                _framePassesData;
    
    int                      _GetNumVisibleFramePasses() const;
    int                      _GetNumFramePasses() const;
    const hvt::FramePassPtr& _GetFramePass(int passIndex)const;
    hvt::FramePassPtr&       _GetFramePass(int passIndex);
    void                     _CreateFramePasses();
    void                     _CreateNonMainFramePassesFilteringSceneIndices();
    void                     _ClearFramePassesData();
    void                     _CreateFramePass(
                                const std::string&                    rendererName,
                                const SdfPath&                        passId,
                                const int passIndex);
    void                    _CreateFramePassesData();

    Fvp::DataProducerMergingSceneIndexProxyPtr _dataProducerMergingSceneIndexProxy { nullptr };
    VtDictionary                              _fileWriterArgs{};
    HdSceneIndexBaseRefPtr                    _lastFilteringSceneIndexBeforeCustomFiltering {nullptr};
    HdSceneIndexBaseRefPtr                    _inputSceneIndexOfFilteringSceneIndicesChain {nullptr};
    Fvp::DisplayStyleOverrideSceneIndexRefPtr _displayStyleSceneIndex;
    Fvp::PruneTexturesSceneIndexRefPtr        _pruneTexturesSceneIndex;
    Fvp::ReprSelectorSceneIndexRefPtr         _reprSelectorSceneIndex;
    Fvp::BboxSceneIndexRefPtr                 _bboxSceneIndex;
    Fvp::DefaultMaterialSceneIndexRefPtr      _defaultMaterialSceneIndex;
    Fvp::SelectionTrackerSharedPtr            _fvpSelectionTracker;
    Fvp::SelectionSceneIndexRefPtr            _selectionSceneIndex;
    Fvp::SelectionPtr                         _selection;
    SdfPath                                   _highlightHierarchyPrefix{"/FlowViewportSelectionHighlights"};
#if PXR_VERSION >= 2405
    Fvp::GeomSubsetWhSiRefPtr                 _geomSubsetWhSi;
#endif
    Fvp::MeshWhSiRefPtr                       _meshWhSi;
    Fvp::NiInstanceWhSiRefPtr                 _niInstanceWhSi;
    Fvp::NiPrototypeWhSiRefPtr                _niPrototypeWhSi;
    Fvp::PiInstancerWhSiRefPtr                _piInstancerWhSi;
    Fvp::PiPrototypeWhSiRefPtr                _piPrototypeWhSi;
    Fvp::BlockPrimRemovalPropagationSceneIndexRefPtr  _blockPrimRemovalPropagationSceneIndex;
    Fvp::PruningSceneIndexRefPtr                      _pruningSceneIndex;
    Fvp::PurposeFilteringSceneIndexRefPtr     _purposeFilteringSceneIndex;
    Fvp::LightsManagementSceneIndexRefPtr _lightsManagementSceneIndex;
    HdsiSceneGlobalsSceneIndexRefPtr          _sceneGlobalsSceneIndex;

    // Naming this identifier _ufeSelection clashes with UFE's selection.h
    // include guard and produces
    // "error C2351: obsolete C++ constructor initialization syntax"
    // with Visual Studio 2022, in MtohRenderOverride::MtohRenderOverride().
    std::shared_ptr<Ufe::Selection>           _ufeSn;
    class SelectionObserver;
    using SelectionObserverPtr = std::shared_ptr<SelectionObserver>;
    SelectionObserverPtr                      _mayaSelectionObserver;
    HdRprimCollection                         _renderCollection { HdTokens->geometry,
                                          HdReprSelector(HdReprTokens->refined),
                                          SdfPath::AbsoluteRootPath() };

    HdRprimCollection _pointSnappingCollection {
        HdTokens->geometry,
        HdReprSelector(HdReprTokens->refined, TfToken(), HdReprTokens->points),
        SdfPath::AbsoluteRootPath()
    };

    GlfSimpleLight _defaultLight;

    MayaHydraSceneIndexRefPtr _mayaHydraSceneIndex;
    MAYAHYDRA_NS::MayaViewportSceneIndexRefPtr _mayaViewportSceneIndex;

    //Lead object selection and wireframe color for selection highlight
    std::shared_ptr<MAYAHYDRA_NS_DEF::MhWireframeColorInterfaceImp> _wireframeColorInterfaceImp {nullptr};
    std::shared_ptr<MAYAHYDRA_NS_DEF::MhLeadObjectPathTracker> _leadObjectPathTracker {nullptr};
    MAYAHYDRA_NS_DEF::MhDirtyLeadObjectSceneIndexRefPtr _dirtyLeadObjectSceneIndex{nullptr};

    /** This class creates the scene index data factories and set them up into the flow viewport library to be able to create DCC
    *   specific scene index data classes without knowing their content in Flow viewport.
    *   This is done in the constructor of this class
    */
    MAYAHYDRA_NS_DEF::SceneIndexDataFactoriesSetup  _sceneIndexDataFactoriesSetup;

    SdfPath _ID; // Root path to runtime data (like task controller)

    GfVec4d _viewport;

    int _currentOperation = -1;    

    bool _needToReplaceSelection = false;
    const bool _isUsingHdSt = false;
    bool       _initializationAttempted = false;
    bool       _initializationSucceeded = false;
    bool       _hasDefaultLighting = false;
    bool       _currentlyTextured = false;
    unsigned int _oldDisplayStyle {0};
    int        _oldRefineLevel {0};
    bool       _useDefaultMaterial;
    MFrameContext::LightingMode _lightingMode = MFrameContext::LightingMode::kSceneLights;
#ifdef MAYA_HAS_VIEW_SELECTED_OBJECT_API
    long int   _nbViewSelectedChangedCalls{0};
#endif

    // Maya has an awkward notification mechanism for isolate select,
    // with a view selected objects changed boolean that indicates
    // whether the state has changed (false), or the isolate selected
    // objects have changed (true).  When changing a viewport from
    // isolate select off to on, two notifications are therefore sent,
    // first false (state change), then true (objects set).  To avoid
    // double dirtying in Hydra, we track the following isolate select
    // states per viewport:
    //
    enum class IsolateSelectState {IsolateSelectOff, IsolateSelectPendingObjects,
				   IsolateSelectOn};

    using VpIsolateSelectStates = std::map<std::string, IsolateSelectState>;
    VpIsolateSelectStates _isolateSelectState;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MTOH_VIEW_OVERRIDE_H
