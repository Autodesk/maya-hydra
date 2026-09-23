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
#include <optional>
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
    /// Called when a host color preference changes. Flags the outline style and the affected prims'
    /// wireframe colors for refresh on the next Render.
    ///
    /// \param token The preference that changed. The selection colors invalidate only selected
    ///        prims and the highlight hierarchy; polymeshDormant invalidates every prim.
    void ColorPreferencesChanged(const PXR_NS::TfToken& token);
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
    HVT_NS::Outline::OutlineStyle _BuildOutlineStyle() const;

    /// Whether the outline is the selection highlight. Stricter than the render global: only Storm
    /// can host the outline tasks.
    bool _UseOutlineSelectionHighlighting() const;

    /// Whether the legacy wireframe highlight is suppressed: either the outline replaces it, or
    /// mayaHydraForceDisableSelectionHighlight turns all highlighting off.
    bool _SuppressLegacySelectionHighlight() const;

    /// Whether any panel this override drives is currently drawing wireframes.
    bool _AnyPanelDrawsWireframes(const std::string& currentPanel, unsigned int currentStyle) const;

    /// Whether the per-mouse-move pick runs: when hover highlighting is on, or in outline mode when
    /// mayaHydraForceEnableInteractiveHitTest forces it for profiling.
    bool _HitTestEnabled() const;

    /// Whether the resolved hover path is drawn using outlines.
    bool _OutlineHoverHightlightingEnabled() const;

    /// Hover state, per panel: one MtohRenderOverride serves every panel using the renderer, so a
    /// shared state would highlight every viewport at once.
    struct HoverState
    {
        // Device pixels, Qt top-left origin; -1 means no hover. _ResolveHoverPath() flips the y.
        std::atomic<int>  deviceX { -1 };
        std::atomic<int>  deviceY { -1 };
        std::atomic<bool> active { false }; // cursor inside viewport, no button held
        std::atomic<bool> dirty { true };   // re-resolve/re-push hover on the next Render()

        /// View-projection matrix of the last hover resolve. When the view moves, a stationary
        /// cursor can be over a different prim with no mouse event, so Render() compares against
        /// this to re-resolve. Render thread only.
        MMatrix lastViewProjMatrix;

        /// Last resolved hover path, reused until the hover is dirtied so the pick does not rerun.
        /// Render thread only.
        PXR_NS::SdfPath resolvedPath;
    };

    /// The hover state for \p panelName, or nullptr when that panel has none.
    HoverState* _GetHoverState(const std::string& panelName);

    // Install / remove the hover event filter on a model panel's viewport widget.
    void _InstallHoverEventFilter(const MString& panelName);
    void _RemoveHoverEventFilter(const MString& panelName);
    // Called by the hover event filter (UI thread). Records the cursor position and schedules a
    // viewport refresh.
    void _SetHoverPosition(const std::string& panelName, int deviceX, int deviceY, bool active);
    // Picks the prim under the cursor pixel (HdxPickTask via the outline frame pass). Returns an
    // empty path when not hovering or over background.
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
    
    // OutlineManager is render-thread-only and unsynchronized (Install, SetInputs and SetStyle
    // must be called from the thread that commits the frame pass). Observers therefore only set
    // the flags below; the manager is touched only from Render().

    // Selection changed: rebuild and push the OutlineManager inputs.
    std::atomic<bool>                     _outlineInputsDirty = { true };

    // Selection changed: invalidate the wireframe colors of the prims involved. Needed in both
    // highlight modes, unlike _outlineInputsDirty.
    std::atomic<bool>                     _selectionColorsDirty = { true };

    // Fully-selected paths at the last color invalidation, so newly deselected prims are dirtied
    // too. Kept sorted for std::set_symmetric_difference.
    PXR_NS::SdfPathVector                 _previouslySelectedPaths;

    // A color preference changed: rebuild and push the OutlineManager style.
    std::atomic<bool>                     _outlineStyleDirty = { true };

    // wireframeSelection or wireframeSelectionSecondary changed. Only selected prims and the
    // highlight hierarchy use these colors, so only they are invalidated.
    std::atomic<bool>                     _selectionWireframeColorsDirty = { false };

    // polymeshDormant changed. Every prim uses it, but only in wireframe, wireframe-on-shaded and
    // bounding-box styles. In other styles the invalidation is skipped, not deferred: switching to
    // one of those styles dirties every prim anyway (SetReprType(), BboxSceneIndex::Enable()).
    std::atomic<bool>                     _dormantWireframeColorDirty = { false };

    /// Hover path currently pushed into the shared OutlineManager. It is the only per-panel input;
    /// comparing it avoids re-pushing inputs every frame. Render thread only.
    PXR_NS::SdfPath                       _pushedOutlineHoverPath;

    /// Selection currently pushed into the OutlineManager, cached so a hover-only push does not pay
    /// for Selection::GetFullySelectedPaths(), which walks the whole selection. Render thread only.
    /// The lead path is deliberately not cached; see the push site.
    PXR_NS::SdfPathVector                 _pushedOutlineSelectedPaths;

    /// Keyed by panel name. Entries are created and destroyed with the hover event filter, on the
    /// main thread. Fields are individually atomic, not snapshot-consistent; a torn x/y pair costs
    /// at most one frame.
    std::map<std::string, std::unique_ptr<HoverState>> _hoverStates;

    /// Destination panel of the upcoming render, recorded in setup(), where the hover event filter
    /// is installed before any frame context exists. Key for _hoverStates, _hoverEventFilters and
    /// _oldDisplayStyles.
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

    std::unique_ptr<HVT_NS::Outline::OutlineManager> _outlineManager;

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

    // Last display style drawn by each panel. Per panel because one MtohRenderOverride serves every
    // model panel using this renderer.
    std::map<std::string, unsigned int> _oldDisplayStyles;

    /// Legacy-highlight treatment currently applied to the render item adapters. The adapters are
    /// shared by every panel, so this records their state rather than any panel's wish, and a panel
    /// switch re-applies it when they differ. Empty when nothing has been applied.
    struct RenderItemTreatment
    {
        bool legacyMayaNativeHighlightEnabled;
        bool viewportDrawsWireframes;

        bool operator==(const RenderItemTreatment& o) const
        {
            return legacyMayaNativeHighlightEnabled == o.legacyMayaNativeHighlightEnabled
                && viewportDrawsWireframes == o.viewportDrawsWireframes;
        }
        bool operator!=(const RenderItemTreatment& o) const { return !(*this == o); }
    };
    std::optional<RenderItemTreatment> _appliedRenderItemTreatment;

    /// Repr currently pushed into _reprSelectorSceneIndex, which is shared by every panel. It is
    /// re-pushed whenever the panel being drawn needs a different one. Includes refineLevel, so a
    /// refinement change is handled the same way. Empty when nothing has been pushed.
    struct ReprTreatment
    {
        Fvp::ReprSelectorSceneIndex::RepSelectorType reprType;
        bool                                         needsReprChanged;
        int                                          refineLevel;

        bool operator==(const ReprTreatment& o) const
        {
            return reprType == o.reprType && needsReprChanged == o.needsReprChanged
                && refineLevel == o.refineLevel;
        }
        bool operator!=(const ReprTreatment& o) const { return !(*this == o); }
    };
    std::optional<ReprTreatment> _appliedReprTreatment;

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
