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

#include "mhMayaUsdProxyShapeSceneIndexBase.h"

#include <flowViewport/fvpInstruments.h>

//mayaHydra headers
#include "ufeExtensions/Global.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MayaUsdProxyShapeSceneIndexBase::MayaUsdProxyShapeSceneIndexBase(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle
)
    : ParentClass(sceneIndexChainLastElement)
    , InputSceneIndexUtils(sceneIndexChainLastElement)
    , _usdImagingStageSceneIndex(usdImagingStageSceneIndex)
    , _proxyStage(proxyStage)
    , _dagNodeHandle(dagNodeHandle)
{
    TfWeakPtr<MayaUsdProxyShapeSceneIndexBase> ptr(this);
    _stageSetNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_StageSet);
    _stageInvalidateNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_StageInvalidate);
    _objectsChangedNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_ObjectsChanged);

    Fvp::Instruments::instance().set(kNbPopulateCalls, VtValue(_nbPopulateCalls));
}

MayaUsdProxyShapeSceneIndexBase::~MayaUsdProxyShapeSceneIndexBase()
{
    _Destroy();                 // In a destructor calls are not virtual
}

void MayaUsdProxyShapeSceneIndexBase::_Destroy()
{
    TfNotice::Revoke(_stageSetNoticeKey);
    TfNotice::Revoke(_stageInvalidateNoticeKey);
    TfNotice::Revoke(_objectsChangedNoticeKey);
}

MayaUsdProxyShapeSceneIndexBaseRefPtr MayaUsdProxyShapeSceneIndexBase::New(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage, 
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle
)
{
    return TfCreateRefPtr(new MayaUsdProxyShapeSceneIndexBase(proxyStage, sceneIndexChainLastElement, usdImagingStageSceneIndex, dagNodeHandle));
}

void MayaUsdProxyShapeSceneIndexBase::UpdateTime()
{
    if (_usdImagingStageSceneIndex && _dagNodeHandle.isValid()) {
        _usdImagingStageSceneIndex->SetTime(_proxyStage.getTime());//We have the possibility to scale and offset the time in _proxyShapeBase
    }
}

void MayaUsdProxyShapeSceneIndexBase::_StageSet(const MAYAUSDAPI_NS::ProxyStageSetNotice& notice) 
{ 
    _populated = false;
    Populate(); 
}

// Stage invalidated.  See
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/nodes/proxyShapeBase.cpp    
// for all inputs that can invalidate the stage, among which:
// - the USD file path
// - the USD prim at the root of the stage
// - the input stage cache ID, e.g. for a Bifrost-generated stage.  Note that
//   in this case, the mayaUsd stage pointer DOES NOT CHANGE: the 
//   Bifrost-generated stage is added as a sub-layer of the mayaUsd stage.
// - etc.
// In these cases we set the stage to null and start over.
void MayaUsdProxyShapeSceneIndexBase::_StageInvalidate(const MAYAUSDAPI_NS::ProxyStageInvalidateNotice& notice) 
{ 
    constexpr char const* INVALID_PROXY_SHAPE_MSG = 
        "Stage invalidate notification for invalid proxy shape node at path %s";

    if (!TF_VERIFY(_dagNodeHandle.isValid(), INVALID_PROXY_SHAPE_MSG, notice.GetProxyShapePath().data())) {
        return;
    }

    // Is the notification for us?
    if (notice.GetProxyShapeObj() != _dagNodeHandle.object()) {
        return;
    }

    _usdImagingStageSceneIndex->SetStage(nullptr);
    _populated = false;
    // Simply mark populate as dirty and do not call
    // Populate();
    // here.  Doing so is incorrect for two reasons:
    // - _StageInvalidate() is a callback called during Maya invalidation.
    //   Populate() calls MayaUsdProxyShapeBase::getUsdStage(), which calls
    //   MayaUsdProxyShapeBase::compute(), which should not be done during
    //   dirty propagation.
    // - Calling getUsdStage() through Populate() creates an invalidate
    //   callback dependency between _StageInvalidate() and
    //   the mayaUsd plugin MayaStagesSubject::onStageInvalidate().  During
    //   getUsdStage(), MayaStagesSubject::setupListeners() is called, and it
    //   depends on MayaStagesSubject::onStageInvalidate() being called first,
    //   otherwise setupListeners() and therefore getUsdStage() will fail.
    //
    //   Invalidate callbacks should not have dependencies on one another ---
    //   it should be possible to call them in random order.
}

void MayaUsdProxyShapeSceneIndexBase::_ObjectsChanged(
    const MAYAUSDAPI_NS::ProxyStageObjectsChangedNotice& notice)
{
    PopulateAndApplyPendingChanges();
}

void MayaUsdProxyShapeSceneIndexBase::PopulateAndApplyPendingChanges() 
{ 
    Populate();
    _usdImagingStageSceneIndex->ApplyPendingUpdates();
}

void MayaUsdProxyShapeSceneIndexBase::Populate()
{
    if (!_populated) {
        auto stage = _proxyStage.getUsdStage();
        // Check whether the pseudo-root has children
        if (stage && (!stage->GetPseudoRoot().GetChildren().empty())) {
            ++_nbPopulateCalls;
            Fvp::Instruments::instance().set(kNbPopulateCalls, VtValue(_nbPopulateCalls));
            _usdImagingStageSceneIndex->SetStage(stage);
            // Set the initial time
            UpdateTime();
            _populated = true;
        }
    }
}

Ufe::Path MayaUsdProxyShapeSceneIndexBase::InterpretRprimPath(
    const HdSceneIndexBaseRefPtr& sceneIndex,
    const SdfPath&                path)
{
    if (MayaUsdProxyShapeSceneIndexBaseRefPtr proxyShapeSceneIndex = TfDynamic_cast<MayaUsdProxyShapeSceneIndexBaseRefPtr>(sceneIndex)) {
        MDagPath dagPath;
        MStatus status = MDagPath::getAPathTo(proxyShapeSceneIndex->_dagNodeHandle.object(), dagPath);
        if (status != MS::kSuccess || !dagPath.isValid()) {
            return Ufe::Path();
        }
        return Ufe::Path(
            { UfeExtensions::dagPathToUfePathSegment(dagPath), UfeExtensions::sdfPathToUfePathSegment(path,  UfeExtensions::getUsdRunTimeId()) });
    }

    return Ufe::Path();
}

// satisfying HdSceneIndexBase
HdSceneIndexPrim MayaUsdProxyShapeSceneIndexBase::GetPrim(const SdfPath& primPath) const
{
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector MayaUsdProxyShapeSceneIndexBase::GetChildPrimPaths(const SdfPath& primPath) const
{
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

bool MayaUsdProxyShapeSceneIndexBase::HasPendingUpdates() const
{
    //When we receive a stage invalidate we remove the stage and set populate to false
    //We need to re-populate to see the changes
    return (false == _populated);
}

} // namespace MAYAHYDRA_NS_DEF
