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
#ifndef BATCH_RENDERER_H
#define BATCH_RENDERER_H

#include "renderGlobals.h"
#include "pluginUtils.h"

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexDataFactoriesSetup.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <flowViewport/selection/fvpSelectionTracker.h>
#include <flowViewport/sceneIndex/fvpBlockPrimRemovalPropagationSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPruningSceneIndex.h>
#include <flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h>

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

#include <ufe/path.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <vector>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE
class MayaHydraSceneIndexRegistry;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

using HgiUniquePtr = std::unique_ptr<class PXR_NS::Hgi>;

/*! \brief BatchRenderer performs Maya batch renders through Hydra.
 */
class BatchRenderer
{
public:

    struct InputParams {
        unsigned int       width{0};
        unsigned int       height{0};
        PXR_NS::GfMatrix4d viewMatrix{};
        PXR_NS::GfMatrix4d projectionMatrix{};
        MDagPath           cameraPath;
        Ufe::Path          ufeCameraPath;
    };

    BatchRenderer(const MtohRendererDescription& desc);
    ~BatchRenderer();

    /// Returns a list of rprims in the render index.
    ///
    /// Intended mostly for use in debugging and testing.
    PXR_NS::SdfPathVector RendererRprims(bool visibleOnly = false);

    /// Returns the scene delegate id for the given scene delegate name.
    ///
    /// Intended mostly for use in debugging and testing.
    PXR_NS::SdfPath RendererSceneDelegateId(PXR_NS::TfToken sceneDelegateName);

    //! Main entry point for rendering, called by Maya.
    MStatus Render(
        const InputParams&                                     inputParams,
        const MHWRender::MDataServerOperation::MViewportScene& scene);

    ///When fullReset is true, we remove the data producer scene indices that apply to all viewports and the scene index registry where the usd stages have been loaded.
    ///It means you are doing a full reset of hydra such as when doing "File New".
    ///Use fullReset = false when you still want to see the previously registered data producer scene indices when using an hydra viewport.
    void ClearHydraResources(bool fullReset);
    void SetRenderPurposeTags(const PXR_NS::MayaHydraParams& delegateParams) { _SetRenderPurposeTags(delegateParams); };

    PXR_NS::HdRenderIndex* renderIndex() const;

private:

    void              _InitHydraResources();
    PXR_NS::HdRenderDelegate* _GetRenderDelegate();   
    void              _ClearMayaHydraSceneIndex();
    void              _SetActiveRenderSettingsPrimFromStageMetadata();
    void              _SetActiveRenderSettingsPrimPath(const PXR_NS::SdfPath& path);

    void              _SetRenderPurposeTags(const PXR_NS::MayaHydraParams& delegateParams);
    void              _CreateSceneIndicesChainAfterMergingSceneIndex();
    PXR_NS::VtValue   _GetUsedGPUMemory() const;

    void _AddPluginSelectionHighlighting();

    // Callbacks
    static void _ClearHydraCallback(void* data);
    static void _TimerCallback(float, float, void* data);

    MtohRendererDescription _rendererDesc;

    std::shared_ptr<PXR_NS::MayaHydraSceneIndexRegistry> _sceneIndexRegistry;
    MCallbackIdArray                             _callbacks;
    MCallbackId                                  _timerCallback = 0;
    const PXR_NS::MtohRenderGlobals&             _globals;

    std::mutex                            _lastRenderTimeMutex;
    std::chrono::system_clock::time_point _lastRenderTime;
    std::atomic<bool>                     _isConverged = { false };
    std::atomic<bool>                     _needsClear = { false };

    /// Hgi and HdDriver should be constructed before HdEngine to ensure they
    /// are destructed last. Hgi may be used during engine/delegate destruction.
    HgiUniquePtr                              _hgi;
    PXR_NS::HdDriver                          _hgiDriver;
    PXR_NS::HdEngine                          _engine;
    PXR_NS::HdRendererPlugin*                 _rendererPlugin = nullptr;
    std::unique_ptr<PXR_NS::HdxTaskController> _taskController;
    PXR_NS::HdPluginRenderDelegateUniqueHandle _renderDelegate = nullptr;
    PXR_NS::VtDictionary                      _fileWriterArgs{};
    PXR_NS::HdSceneIndexBaseRefPtr            _lastFilteringSceneIndexBeforeCustomFiltering {nullptr};
    PXR_NS::HdSceneIndexBaseRefPtr            _inputSceneIndexOfFilteringSceneIndicesChain {nullptr};
    PXR_NS::HdRenderIndex*                    _renderIndex = nullptr;
    // Required by selection task, figure out how to remove this data member.
    Fvp::SelectionTrackerSharedPtr            _fvpSelectionTracker;
    Fvp::BlockPrimRemovalPropagationSceneIndexRefPtr  _blockPrimRemovalPropagationSceneIndex;
    Fvp::PruningSceneIndexRefPtr                      _pruningSceneIndex;
    PXR_NS::HdsiSceneGlobalsSceneIndexRefPtr  _sceneGlobalsSceneIndex;
    Fvp::DataProducerMergingSceneIndexProxyPtr _dataProducerMergingSceneIndexProxy { nullptr };

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

    bool _needToReplaceSelection = false;
    const bool _isUsingHdSt = false;
    bool       _initializationAttempted = false;
    bool       _initializationSucceeded = false;
};

}

#endif // BATCH_RENDERER_H
