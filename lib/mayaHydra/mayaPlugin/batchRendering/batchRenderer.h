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
#ifndef MAYAHYDRA_BATCH_RENDERER_H
#define MAYAHYDRA_BATCH_RENDERER_H

#include "batchRenderTypes.h"
#include "renderGlobals.h"
#include "pluginUtils.h"

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexDataFactoriesSetup.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mhRenderingColorSpaceResolvingSceneIndex.h>

#include <flowViewport/selection/fvpSelectionTracker.h>
#include <flowViewport/sceneIndex/fvpFrameNbResolvingSceneIndex.h>
#include <flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h>

#include <pxr/base/gf/rect2i.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rprimCollection.h>
#include <pxr/imaging/hd/pluginRenderDelegateUniqueHandle.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/hdsi/sceneGlobalsSceneIndex.h>
#include <pxr/pxr.h>

#include <maya/MCallbackIdArray.h>

#include <ufe/path.h>

#include <atomic>
#include <memory>
#include <optional>

PXR_NAMESPACE_OPEN_SCOPE
class MayaHydraSceneIndexRegistry;
struct HdxRenderTaskParams;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

using HgiUniquePtr = std::unique_ptr<class PXR_NS::Hgi>;

class BatchRendererMayaRenderSettings;
class BatchRendererHydraV1RenderSettings;
class BatchRendererHydraV2RenderSettings;

/*! \brief BatchRenderer performs Maya batch renders through Hydra.
 */
class BatchRenderer
{
public:

    struct InputParams {
        unsigned int       width{0};
        unsigned int       height{0};
        RenderVarsInfo     renderVarsInfo;
        Ufe::Path          ufeCameraPath; // Is used to get the Maya camera MDagPath to get the view and projection matrices for the task controller.

        // Optional crop region (`-reg` flag, USD UsdRenderProduct.dataWindowNDC)
        // expressed as an inclusive pixel rect (Y-up, origin bottom-left).  When
        // set, the output AOV stays at full (width x height) but only pixels
        // inside this rect are rendered; the rest keep the AOV clear value.
        std::optional<PXR_NS::GfRect2i> dataWindow;
    };

    BatchRenderer(const MtohRendererDescription& desc);
    ~BatchRenderer();

    MStatus RenderFromMayaRenderSettings(const InputParams& inputParams);
    MStatus RenderFromHydraV1RenderSettings(const InputParams& inputParams);
    MStatus RenderFromHydraV2RenderSettings();
    PXR_NS::TfToken GetRendererName() const { return _rendererDesc.rendererName; }

    // Unit tests inspect the Hydra scene once the hydraRender command has
    // completed, which is past the point where the batch renderer would
    // normally be destroyed.  In test mode, the ownership is handed over to
    // RetainForTest(), keeping the renderer and therefore its render index and
    // whole scene index chain intact.  ReleaseRetainedForTest() destroys it.
    static bool TestModeEnabled();
    static void RetainForTest(std::unique_ptr<BatchRenderer> batchRenderer);
    static void ReleaseRetainedForTest();

    bool Initialize();

    PXR_NS::HdRenderIndex* renderIndex() const;

private:

    friend class BatchRendererMayaRenderSettings;
    friend class BatchRendererHydraV1RenderSettings;
    friend class BatchRendererHydraV2RenderSettings;

    static constexpr const char* kBatchRenderDummyPanelName = "batchRenderDummyPanel";

    void              _InitHydraResources();
    void              _ClearHydraResources();
    PXR_NS::HdRenderDelegate* _GetRenderDelegate();   
    void              _ClearMayaHydraSceneIndex();
    void              _SetActiveRenderSettingsPrimFromScene();

    void              _SetRenderPurposeTags(const PXR_NS::MayaHydraParams& delegateParams);
    void              _CreateSceneIndicesChainAfterMergingSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndexOfFilteringSceneIndicesChain
    );
    bool              _PrepareRender(
        unsigned int width,
        unsigned int height,
        PXR_NS::HdxRenderTaskParams& outParams,
        const std::optional<PXR_NS::GfRect2i>& dataWindow = std::nullopt);
    void              _FinalizeHydraBatchRender(const PXR_NS::HdxRenderTaskParams& params);
    void              _ExecuteHydraBatchRenderFrame();

    // Callbacks
    static void _ClearHydraCallback(void* data);
    static void _TimeChangedCallback(void* data);

    void _SetCurrentFrameInHydraGlobalSceneIndex(double currentFrame);

    MtohRendererDescription _rendererDesc;

    std::shared_ptr<PXR_NS::MayaHydraSceneIndexRegistry> _sceneIndexRegistry;
    MCallbackIdArray                             _callbacks;
    const PXR_NS::MtohRenderGlobals&             _globals;

    std::atomic<bool>                     _isConverged = { false };

    /// Hgi and HdDriver should be constructed before HdEngine to ensure they
    /// are destructed last. Hgi may be used during engine/delegate destruction.
    HgiUniquePtr                              _hgi;
    PXR_NS::HdDriver                          _hgiDriver;
    PXR_NS::HdEngine                          _engine;
    PXR_NS::HdRendererPlugin*                 _rendererPlugin = nullptr;
    std::unique_ptr<PXR_NS::HdxTaskController> _taskController;
    PXR_NS::HdPluginRenderDelegateUniqueHandle _renderDelegate = nullptr;
    PXR_NS::HdSceneIndexBaseRefPtr            _lastFilteringSceneIndexBeforeCustomFiltering {nullptr};
    PXR_NS::HdRenderIndex*                    _renderIndex = nullptr;
    // Required by selection task.
    Fvp::SelectionTrackerSharedPtr            _fvpSelectionTracker;
    PXR_NS::HdsiSceneGlobalsSceneIndexRefPtr  _sceneGlobalsSceneIndex;
    Fvp::FrameNbResolvingSceneIndexRefPtr     _frameNbResolvingSceneIndex {};
    MayaHydra::MhRenderingColorSpaceResolvingSceneIndexRefPtr _renderingColorSpaceSceneIndex;
    Fvp::DataProducerMergingSceneIndexProxyPtr _dataProducerMergingSceneIndexProxy { nullptr };

    // Batch renderer kept alive for unit tests, see RetainForTest().
    static std::unique_ptr<BatchRenderer> _retainedForTest;

    PXR_NS::HdRprimCollection                 _renderCollection {
        PXR_NS::HdTokens->geometry,
        PXR_NS::HdReprSelector(PXR_NS::HdReprTokens->refined),
        PXR_NS::SdfPath::AbsoluteRootPath() };

    PXR_NS::MayaHydraSceneIndexRefPtr _mayaHydraSceneIndex;

    /** This class creates the scene index data factories and set them up into the flow viewport library to be able to create DCC 
    *   specific scene index data classes without knowing their content in Flow viewport.
    *   This is done in the constructor of this class
    */
    SceneIndexDataFactoriesSetup  _sceneIndexDataFactoriesSetup;

    PXR_NS::SdfPath _ID; // Root path to runtime data (like task controller) 

    PXR_NS::GfVec4d _viewport;

    int _currentOperation = -1;

    const bool _isUsingHdSt = false;
    bool       _initializationAttempted = false;
    bool       _initializationSucceeded = false;

    // Maya is the single point of truth for time, so update on change.
    MCallbackId _timeChangeCallbackId = 0;
};

}

#endif // MAYAHYDRA_BATCH_RENDERER_H
