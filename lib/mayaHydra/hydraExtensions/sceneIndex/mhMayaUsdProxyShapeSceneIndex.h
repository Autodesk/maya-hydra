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
#ifndef MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_PLUGIN_H
#define MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_PLUGIN_H

//MayaHydra headers
#include "mayaHydraLib/api.h"

#include <mayaHydraLib/sceneIndex/mhMayaUsdProxyShapeSceneIndexBase.h>

// Flow Viewport Toolkit headers.
#include <flowViewport/selection/fvpPathMapper.h>

#include <ufe/observer.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

class MayaUsdProxyShapeSceneIndex;

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
typedef PXR_NS::TfRefPtr<MayaUsdProxyShapeSceneIndex> MayaUsdProxyShapeSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MayaUsdProxyShapeSceneIndex> MayaUsdProxyShapeSceneIndexConstRefPtr;

/// <summary>
/// Simply wraps single stage scene index for initial stage assignment and population
/// </summary>
class MayaUsdProxyShapeSceneIndex : public MayaUsdProxyShapeSceneIndexBase
{
public:
    using ParentClass = MayaUsdProxyShapeSceneIndexBase;

    static MayaUsdProxyShapeSceneIndexRefPtr
    New(const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
        const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
        const MObjectHandle&                   dagNodeHandle,
        const PXR_NS::SdfPath&                 sceneIndexPathPrefix,
        const Ufe::Path&                       sceneIndexAppPath);

    // From HdSceneIndexBase
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;
    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override;

    MayaUsdProxyShapeSceneIndex(const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
                                const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
                                const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
                                const MObjectHandle&                   dagNodeHandle,
                                const PXR_NS::SdfPath&                 sceneIndexPathPrefix,
                                const Ufe::Path&                       sceneIndexAppPath);

    virtual ~MayaUsdProxyShapeSceneIndex();

    // When we receive a stage invalidate we remove the stage and set populate to false
    // We need to re-populate to see the changes
    //HasPendingUpdates() is true when this is the case
    bool HasPendingUpdates() const;
    void PopulateAndApplyPendingChanges();


    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const;

    const Ufe::Path& GetSceneIndexAppPath() const { return _sceneIndexAppPath; }
    void             SetSceneIndexAppPath(const Ufe::Path& sceneIndexAppPath)
    {
        _sceneIndexAppPath = sceneIndexAppPath;
    }

private:
    MayaUsdProxyShapeSceneIndex(
        const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
        const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
        const MObjectHandle&                   dagNodeHandle,
        const PXR_NS::SdfPath&                 sceneIndexPathPrefix,
        const Ufe::Path&                       sceneIndexAppPath);

private:
    void _ObjectsChanged(const MAYAUSDAPI_NS::ProxyStageObjectsChangedNotice& notice);
    void _StageSet(const MAYAUSDAPI_NS::ProxyStageSetNotice& notice);
    void _StageInvalidate(const MAYAUSDAPI_NS::ProxyStageInvalidateNotice& notice);

private:

    UsdImagingStageSceneIndexRefPtr _usdImagingStageSceneIndex {nullptr};
    MAYAUSDAPI_NS::ProxyStage       _proxyStage;
    std::atomic<bool>               _populated { false };
    MObjectHandle                   _dagNodeHandle;
    TfNotice::Key                   _stageSetNoticeKey;
    TfNotice::Key                   _stageInvalidateNoticeKey;
    TfNotice::Key                   _objectsChangedNoticeKey;
    long int                        _nbPopulateCalls{0};

    // Path mapper support.
    const SdfPath                   _sceneIndexPathPrefix;
    Ufe::Path                       _sceneIndexAppPath;
    const Ufe::Observer::Ptr        _appSceneObserver{};
    const Fvp::PathMapperConstPtr   _usdPathMapper{};

    bool _unregisterPickHandler{false};
    bool _unregisterPathMapper{false};
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_PLUGIN_H