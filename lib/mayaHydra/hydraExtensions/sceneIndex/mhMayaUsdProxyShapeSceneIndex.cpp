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

#include "mhMayaUsdProxyShapeSceneIndex.h"

#if defined(MAYAHYDRALIB_MAYAUSD_ENABLED)

//Maya headers
#include <maya/MEventMessage.h>//For timeChanged callback

//mayaHydra headers
#include "ufeExtensions/Global.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace MAYAHYDRA_NS_DEF {

MayaUsdProxyShapeSceneIndex::MayaUsdProxyShapeSceneIndex(const MAYAUSDAPI_NS::ProxyStage& proxyStage,
                                                        const HdSceneIndexBaseRefPtr& sceneIndexChainLastElement,
                                                        const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
                                                        const MObjectHandle& dagNodeHandle)
    : ParentClass(sceneIndexChainLastElement)
    , _usdImagingStageSceneIndex(usdImagingStageSceneIndex)
    , _proxyStage(proxyStage)
    , _dagNodeHandle(dagNodeHandle)
{
    TfWeakPtr<MayaUsdProxyShapeSceneIndex> ptr(this);
    TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndex::_StageSet);
    TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndex::_ObjectsChanged);
    
    _timeChangeCallbackId = MEventMessage::addEventCallback("timeChanged", onTimeChanged, this);
}

MayaUsdProxyShapeSceneIndex::~MayaUsdProxyShapeSceneIndex()
{
    MMessage::removeCallback(_timeChangeCallbackId);
}

MayaUsdProxyShapeSceneIndexRefPtr MayaUsdProxyShapeSceneIndex::New(const MAYAUSDAPI_NS::ProxyStage& proxyStage, 
                                                                   const HdSceneIndexBaseRefPtr& sceneIndexChainLastElement,
                                                                   const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
                                                                   const MObjectHandle& dagNodeHandle)
{
    return TfCreateRefPtr(new MayaUsdProxyShapeSceneIndex(proxyStage, sceneIndexChainLastElement, usdImagingStageSceneIndex, dagNodeHandle));
}

void MayaUsdProxyShapeSceneIndex::onTimeChanged(void* data)
{
    auto* instance = reinterpret_cast<MayaUsdProxyShapeSceneIndex*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }
    instance->UpdateTime();
}

void MayaUsdProxyShapeSceneIndex::UpdateTime()
{
    if (_usdImagingStageSceneIndex && _dagNodeHandle.isValid()) {
        _usdImagingStageSceneIndex->SetTime(_proxyStage.getTime());//We have the possibility to scale and offset the time in _proxyShapeBase
    }
}

void MayaUsdProxyShapeSceneIndex::_StageSet(const MAYAUSDAPI_NS::ProxyStageSetNotice& notice) 
{ 
    Populate(); 
}

void MayaUsdProxyShapeSceneIndex::_ObjectsChanged(
    const MAYAUSDAPI_NS::ProxyStageObjectsChangedNotice& notice)
{
    _PopulateAndApplyPendingChanges();
}

void MayaUsdProxyShapeSceneIndex::_PopulateAndApplyPendingChanges() 
{ 
    Populate();
    _usdImagingStageSceneIndex->ApplyPendingUpdates();
}

void MayaUsdProxyShapeSceneIndex::Populate()
{
    if (!_populated) {
        auto stage = _proxyStage.getUsdStage();
        // Check whether the pseudo-root has children
        if (stage && (!stage->GetPseudoRoot().GetChildren().empty())) {
            _usdImagingStageSceneIndex->SetStage(stage);
            // Set the initial time
            UpdateTime();
            _populated = true;
        }
    }
}

Ufe::Path MayaUsdProxyShapeSceneIndex::InterpretRprimPath(
    const HdSceneIndexBaseRefPtr& sceneIndex,
    const SdfPath&                path)
{
    if (MayaUsdProxyShapeSceneIndexRefPtr proxyShapeSceneIndex = TfDynamic_cast<MayaUsdProxyShapeSceneIndexRefPtr>(sceneIndex)) {
        MDagPath dagPath(MDagPath::getAPathTo(proxyShapeSceneIndex->_dagNodeHandle.object()));
        return Ufe::Path(
            { UfeExtensions::dagPathToUfePathSegment(dagPath), UfeExtensions::sdfPathToUfePathSegment(path,  UfeExtensions::getUsdRunTimeId()) });
    }

    return Ufe::Path();
}

// satisfying HdSceneIndexBase
HdSceneIndexPrim MayaUsdProxyShapeSceneIndex::GetPrim(const SdfPath& primPath) const
{
    return _GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector MayaUsdProxyShapeSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

} // namespace MAYAUSD_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE
#endif //MAYAHYDRALIB_MAYAUSD_ENABLED
