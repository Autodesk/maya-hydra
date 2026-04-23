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
#ifndef MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_BASE_PLUGIN_H
#define MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_BASE_PLUGIN_H

//MayaHydra headers
#include "mayaHydraLib/api.h"

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

#include <ufe/path.h>

#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

class MayaUsdProxyShapeSceneIndexBase;

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
typedef PXR_NS::TfRefPtr<MayaUsdProxyShapeSceneIndexBase> MayaUsdProxyShapeSceneIndexBaseRefPtr;
typedef PXR_NS::TfRefPtr<const MayaUsdProxyShapeSceneIndexBase> MayaUsdProxyShapeSceneIndexBaseConstRefPtr;

/*! \brief Wraps a single stage scene index for initial stage assignment and population.
 */
class MayaUsdProxyShapeSceneIndexBase : public HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MayaUsdProxyShapeSceneIndexBase>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;
    using ParentClass = HdSingleInputFilteringSceneIndexBase;

    static constexpr char kNbPopulateCalls[] = "MayaUsdProxyShapeSceneIndexBase:NbPopulateCalls";

    static MayaUsdProxyShapeSceneIndexBaseRefPtr
    New(const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
        const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
        const MObjectHandle&                   dagNodeHandle
    );

    // From HdSceneIndexBase
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;
    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override;

    virtual ~MayaUsdProxyShapeSceneIndexBase();

    // When we receive a stage invalidate we remove the stage and set populate to false
    // We need to re-populate to see the changes
    //HasPendingUpdates() is true when this is the case
    bool HasPendingUpdates() const;
    void PopulateAndApplyPendingChanges();

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

#ifndef CODE_COVERAGE_WORKAROUND
protected:
#endif
    virtual void _Destroy();

protected:

    MayaUsdProxyShapeSceneIndexBase(
        const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
        const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
        const MObjectHandle&                   dagNodeHandle
    );

private:

    void _ObjectsChanged(const MAYAUSDAPI_NS::ProxyStageObjectsChangedNotice& notice);
    void _StageSet(const MAYAUSDAPI_NS::ProxyStageSetNotice& notice);
    void _StageInvalidate(const MAYAUSDAPI_NS::ProxyStageInvalidateNotice& notice);

    UsdImagingStageSceneIndexRefPtr _usdImagingStageSceneIndex {nullptr};
    MAYAUSDAPI_NS::ProxyStage       _proxyStage;
    std::atomic<bool>               _populated { false };
    MObjectHandle                   _dagNodeHandle;
    TfNotice::Key                   _stageSetNoticeKey;
    TfNotice::Key                   _stageInvalidateNoticeKey;
    TfNotice::Key                   _objectsChangedNoticeKey;
    long int                        _nbPopulateCalls{0};
};

} // namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_MAYAUSD_PROXY_SHAPE_SCENE_INDEX_BASE_PLUGIN_H
