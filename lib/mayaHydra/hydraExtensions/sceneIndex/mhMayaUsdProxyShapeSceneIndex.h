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

#if defined(MAYAHYDRALIB_MAYAUSDAPI_ENABLED)
//MayaHydra headers
#include "mayaHydraLib/api.h"
#include "mayaHydraLib/mayaHydra.h"

//MayaUsdAPI headers
#include <mayaUsdAPI/proxyStage.h>
#include <mayaUsdAPI/proxyShapeNotice.h>

// Flow Viewport Toolkit headers.
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

//Usd/Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/usdImaging/usdImaging/stageSceneIndex.h>
#include <pxr/usdImaging/usdImaging/sceneIndices.h>// In USD 23.11+
#include <pxr/imaging/hd/filteringSceneIndex.h>

//Maya headers
#include <maya/MObjectHandle.h>
#include <maya/MMessage.h>

//Ufe headers
#include <ufe/path.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

namespace MAYAHYDRA_NS_DEF {

class MayaUsdProxyShapeSceneIndex;
TF_DECLARE_WEAK_AND_REF_PTRS(MayaUsdProxyShapeSceneIndex);

/// <summary>
/// Simply wraps single stage scene index for initial stage assignment and population
/// </summary>
class MayaUsdProxyShapeSceneIndex : public HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MayaUsdProxyShapeSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;
    using ParentClass = HdSingleInputFilteringSceneIndexBase;

    static MayaUsdProxyShapeSceneIndexRefPtr
    New(const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
        const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
        const MObjectHandle&                   dagNodeHandle);

    // From HdSceneIndexBase
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;
    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override;

    MayaUsdProxyShapeSceneIndex(const MAYAUSDAPI_NS::ProxyStage&        proxyStage,
                                const HdSceneIndexBaseRefPtr&           sceneIndexChainLastElement,
                                const UsdImagingStageSceneIndexRefPtr&  usdImagingStageSceneIndex,
                                const MObjectHandle&                    dagNodeHandle);

    virtual ~MayaUsdProxyShapeSceneIndex();

    void Populate();
    void UpdateTime();
    static Ufe::Path InterpretRprimPath(const HdSceneIndexBaseRefPtr& sceneIndex,const SdfPath& path);

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const HdSceneIndexBase& sender, const HdSceneIndexObserver::AddedPrimEntries& entries) override{
        _SendPrimsAdded(entries);
    }
    void _PrimsRemoved(const HdSceneIndexBase& sender, const HdSceneIndexObserver::RemovedPrimEntries& entries)override{
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(const HdSceneIndexBase& sender, const HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        _SendPrimsDirtied(entries);
    }

private:
    void _ObjectsChanged(const MAYAUSDAPI_NS::ProxyStageObjectsChangedNotice& notice);
    void _StageSet(const MAYAUSDAPI_NS::ProxyStageSetNotice& notice);
    void _PopulateAndApplyPendingChanges();

private:
    static void onTimeChanged(void* data);

    UsdImagingStageSceneIndexRefPtr _usdImagingStageSceneIndex {nullptr};
    MAYAUSDAPI_NS::ProxyStage       _proxyStage;
    std::atomic<bool>               _populated { false };
    MCallbackId                     _timeChangeCallbackId = 0;
    MObjectHandle                   _dagNodeHandle;
    TfNotice::Key                   _stageSetNoticeKey;
    TfNotice::Key                   _objectsChangedNoticeKey;
};

} // namespace MAYAHYDRA_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE

#endif //MAYAHYDRALIB_MAYAUSDAPI_ENABLED

#endif //MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_PLUGIN_H
