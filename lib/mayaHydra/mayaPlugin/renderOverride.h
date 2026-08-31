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
#include <mayaHydraLib/sceneIndex/mhDirtySelectionColorsSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mhGenerativeProceduralResolvingSceneIndex.h>
#include <mayaHydraLib/pick/mhPickHandlerFwd.h>
#include <mayaHydraLib/pick/mhPickContext.h>
#include <mayaHydraLib/pick/mhPickHitFwd.h>

#include <hvt/tasks/outline/outlineManager.h>

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
#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MString.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <map>

#include <pxr/base/tf/hashset.h>

#include <ufe/ufe.h>
UFE_NS_DEF {
class Path;
class SelectionChanged;
class Selection;
}

namespace MAYAHYDRA_NS_DEF {
class HoverEventFilter;
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
    /// This currently gathers AOVs from all viewports indiscriminately.
    /// With proper multi-viewport support, it should also be possible to
    /// specify which viewport to get the AOVs for.
    static TfTokenVector GetAvailableFramePassAovs(int passIndex);

    static MtohRenderOverride* GetByName(TfToken rendererName);

    /// Returns a list of rprims in the render index for the given render
    /// delegate.
    ///
    /// Intended mostly for use in debugging and testing.
    static SdfPathVector RendererRprims(TfToken rendererName, bool visibleOnly = false);

    /// Returns the scene index root path for the given render delegate and
    /// scene index name.
    ///
    /// Intended mostly for use in debugging and testing.
    static SdfPath RendererSceneDelegateId(TfToken rendererName, TfToken sceneDelegateName);

    /// Returns whether the given renderer has converged.
    /// This currently only checks the first viewport found that uses the
    /// given renderer. With proper multi-viewport support, it should also
    /// be possible to specify which viewport to check the convergence for.
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
    /// Called when a host color preference changes, so the outline style can be
    /// rebuilt and pushed to the OutlineManager on the next Render.
    void ColorPreferencesChanged();
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

    // Returns scene statistics as a map for the currently active render delegate from Hydra
    // primitives
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
    HVT_NS::Outline::OutlineStyle _BuildOutlineStyle() const;

    /// Whether the pixel outline is the selection-highlight mechanism for this override. Not the
    /// same as the render global: a non-Storm delegate cannot host the outline tasks.
    bool _UseOutlineSelectionHighlighting() const;

    /// Whether hover highlighting is active for this override. The outline condition is what stops
    /// mouse movement in Legacy mode scheduling a refresh per event for a hover nothing can draw.
    bool _HoverEnabled() const;

    /// Viewport hover state, kept per panel: one MtohRenderOverride serves every panel using the
    /// renderer, as do _outline and the frame passes, so a single shared state would highlight
    /// every viewport at once.
    struct HoverState
    {
        // Device pixels, Qt top-left origin; -1 means no hover. _ResolveHoverPath() flips the y.
        std::atomic<int>  deviceX { -1 };
        std::atomic<int>  deviceY { -1 };
        std::atomic<bool> active { false }; // cursor inside viewport, no button held
        std::atomic<bool> dirty { true };   // re-resolve/re-push hover on the next Render()

        /// View-projection matrix from the last frame that resolved a hover for this panel. A
        /// stationary cursor sits over a different prim once the view moves, with no mouse event to
        /// signal it, so Render() compares against this to dirty the hover. Per panel because each
        /// has its own camera. Render thread only.
        MMatrix lastViewProjMatrix;
    };

    /// The hover state for \p panelName, or nullptr when that panel has none.
    HoverState* _GetHoverState(const std::string& panelName);

    // Install / remove the hover event filter on a model panel's viewport widget.
    void _InstallHoverEventFilter(const MString& panelName);
    void _RemoveHoverEventFilter(const MString& panelName);
    // Called by the hover event filter (UI thread) with the cursor position in device
    // pixels; records it and schedules a viewport refresh so Render() picks it up.
    void _SetHoverPosition(const std::string& panelName, int deviceX, int deviceY, bool active);
    // Resolve the prim under the cursor into a Hydra prim path with a small pick at the
    // cursor pixel (HdxPickTask via the outline frame pass). Returns an empty path when
    // not hovering or the cursor is over background. Drives path-based hover highlighting.
    PXR_NS::SdfPath _ResolveHoverPath(const MHWRender::MDrawContext& drawContext);

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

    /// After building isolate selection from UFE paths, append Maya-native
    /// Hydra paths for camera/light gizmo drawables that correspond to the
    /// selected UFE camera and light prims.  Those rprims live under
    /// MAYA_NATIVE_ROOT with a different path prefix than the source proxy,
    /// so they would otherwise be hidden by isolate's prefix-based
    /// visibility.
    ///
    /// Returns the set of paths whose visibility must be forced ON by the
    /// IsolateSelectSceneIndex (every such path is also added to \p selection).
    /// VP2's isolate-select filtering incorrectly hides these render items
    /// because VP2 is unaware that Hydra has included them.
    ///
    /// USD is the only UFE client this function currently understands; non-USD
    /// view-selected paths are ignored.  The function inspects each path
    /// itself, so callers can pass through every view-selected UFE path.
    ///
    /// \param selectedUfePaths Raw view-selected UFE paths (from
    ///        M3dView::viewSelectedObject).
    /// \param panelCameraDag Shape DAG path of the model panel camera
    ///        (from M3dView::getCamera); used to include native rprims under
    ///        that camera's Hydra branch.
    TfHashSet<SdfPath, SdfPath::Hash> _ExpandIsolateSelectionForUsdPrims(
        Fvp::Selection&               selection,
        const std::vector<Ufe::Path>& selectedUfePaths,
        const MDagPath&               panelCameraDag);
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
    
    // These flags exist because OutlineManager is render-thread-only: HVT documents that Install /
    // SetInputs / SetStyle must all be called from the thread driving the frame pass commit, with
    // no internal synchronization. Observers (selection, color preferences, the hover event filter)
    // may therefore only flag work here; the manager itself is touched exclusively from Render().

    // Set whenever the selection (and thus the outline inputs) changes, so Render only
    // rebuilds and pushes OutlineManager inputs when needed rather than every frame.
    std::atomic<bool>                     _outlineInputsDirty = { true };

    // Set whenever the selection changes, so Render invalidates the wireframe colors of the prims
    // involved. Separate from _outlineInputsDirty because this is needed in both highlight modes,
    // whereas that one is only consumed when the outline is installed.
    std::atomic<bool>                     _selectionColorsDirty = { true };

    // The fully-selected paths as of the last color invalidation, so the prims that just became
    // deselected can be dirtied too -- they need to drop the selection color, not only acquire it.
    // Kept sorted, as one side of the std::set_symmetric_difference that decides what to dirty.
    PXR_NS::SdfPathVector                 _previouslySelectedPaths;

    // Set whenever a host color preference changes, so Render rebuilds and pushes the
    // OutlineManager style only when needed rather than every frame.
    std::atomic<bool>                     _outlineStyleDirty = { true };

    // Set whenever a host color preference changes, so Render invalidates every prim that pulls a
    // wireframe color rather than only the ones whose selection state changed.
    std::atomic<bool>                     _wireframeColorsDirty = { false };

    /// Keyed by panel name. Entries are created and destroyed alongside the hover event filter,
    /// both on the main thread, so the map structure is never mutated concurrently. The fields are
    /// individually atomic rather than snapshot-consistent: a frame can pair an x from before a
    /// mouse move with a y from after, which is one frame of one pixel and not worth a lock.
    std::map<std::string, std::unique_ptr<HoverState>> _hoverStates;

    /// The panel currently being drawn, recorded by setup() and read by Render(). Maya calls
    /// setup() with the destination panel before each of that panel's renders.
    MString _currentPanelName;

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
    MAYAHYDRA_NS_DEF::MhGenerativeProceduralResolvingSceneIndexRefPtr _gpResolvingSceneIndex;
    HdsiSceneGlobalsSceneIndexRefPtr                  _sceneGlobalsSceneIndex;

    // Naming this identifier _ufeSelection clashes with UFE's selection.h
    // include guard and produces
    // "error C2351: obsolete C++ constructor initialization syntax"
    // with Visual Studio 2022, in MtohRenderOverride::MtohRenderOverride().
    std::shared_ptr<Ufe::Selection>           _ufeSn;
    class SelectionObserver;
    using SelectionObserverPtr = std::shared_ptr<SelectionObserver>;
    SelectionObserverPtr                      _mayaSelectionObserver;
    class ColorPreferencesObserver;
    using ColorPreferencesObserverPtr = std::shared_ptr<ColorPreferencesObserver>;
    ColorPreferencesObserverPtr               _colorPreferencesObserver;
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
    MAYAHYDRA_NS_DEF::MhDirtySelectionColorsSceneIndexRefPtr
        _dirtySelectionColorsSceneIndex { nullptr };

    std::unique_ptr<HVT_NS::Outline::OutlineManager> _outline;

#ifdef MAYAHYDRA_HAS_QT
    // One hover event filter per model panel, keyed by panel name.
    std::map<std::string, std::unique_ptr<MAYAHYDRA_NS_DEF::HoverEventFilter>>
        _hoverEventFilters;
#endif

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
