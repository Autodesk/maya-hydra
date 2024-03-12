//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#include "mayaHydraSceneProducer.h"

#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/mayaHydraLibInterface.h>

#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h>
#include <flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h>

PXR_NAMESPACE_OPEN_SCOPE

MayaHydraSceneProducer::MayaHydraSceneProducer(
    const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy,
    const SdfPath&               id,
    MayaHydraInitData& initData,
    bool                         lightEnabled
) : _renderIndexProxy(renderIndexProxy)
{
    initData.name = TfToken("MayaHydraSceneIndex");
    initData.delegateID = id.AppendChild(
        TfToken(TfStringPrintf("_Index_MayaHydraSceneIndex_%p", this)));
    initData.producer = this;
    _sceneIndex = MayaHydraSceneIndex::New(initData, lightEnabled);
    TF_VERIFY(_sceneIndex, "Maya Hydra scene index not found, check mayaHydra plugin installation.");
}

MayaHydraSceneProducer::~MayaHydraSceneProducer()
{
    _renderIndexProxy->RemoveSceneIndex(_sceneIndex);
    _sceneIndex->RemoveCallbacksAndDeleteAdapters();//This should be called before calling _sceneIndex.Reset(); which will call the destructor if the ref count reaches 0
    _sceneIndex.Reset();
}

#ifdef CODE_COVERAGE_WORKAROUND
void MayaHydraSceneProducer::Cleanup()
{
    _sceneIndex->RemoveCallbacksAndDeleteAdapters();
}
#endif

void MayaHydraSceneProducer::HandleCompleteViewportScene(const MDataServerOperation::MViewportScene& scene, MFrameContext::DisplayStyle ds)
{
    return _sceneIndex->HandleCompleteViewportScene(scene, ds);
}

void MayaHydraSceneProducer::Populate()
{
    _sceneIndex->Populate();
    //Add the scene index as an input scene index of the merging scene index
    _renderIndexProxy->InsertSceneIndex(_sceneIndex, SdfPath::AbsoluteRootPath());
}

SdfPath MayaHydraSceneProducer::SetCameraViewport(const MDagPath& camPath, const GfVec4d& viewport)
{
    return _sceneIndex->SetCameraViewport(camPath, viewport);
}

void MayaHydraSceneProducer::SetLightsEnabled(const bool enabled)
{
    return _sceneIndex->SetLightsEnabled(enabled);
}

void MayaHydraSceneProducer::SetDefaultLightEnabled(const bool enabled)
{   
    _sceneIndex->SetDefaultLightEnabled(enabled);
}


void MayaHydraSceneProducer::SetDefaultLight(const GlfSimpleLight& light)
{
    _sceneIndex->SetDefaultLight(light);
}

const MayaHydraParams& MayaHydraSceneProducer::GetParams() const
{
    return _sceneIndex->GetParams();
}

void MayaHydraSceneProducer::SetParams(const MayaHydraParams& params)
{
    return _sceneIndex->SetParams(params);
}

bool MayaHydraSceneProducer::AddPickHitToSelectionList(
    const HdxPickHit& hit,
    const MHWRender::MSelectionInfo& selectInfo,
    MSelectionList& selectionList,
    MPointArray& worldSpaceHitPts)
{
    return _sceneIndex->AddPickHitToSelectionList(
            hit,
            selectInfo,
            selectionList,
            worldSpaceHitPts);
}

HdRenderIndex& MayaHydraSceneProducer::GetRenderIndex()
{
    return _sceneIndex->GetRenderIndex();
}

bool MayaHydraSceneProducer::IsHdSt() const
{
    return _sceneIndex->IsHdSt();
}

bool MayaHydraSceneProducer::GetPlaybackRunning() const
{
    return false;
}

SdfPath MayaHydraSceneProducer::GetPrimPath(const MDagPath& dg, bool isSprim)
{
    return _sceneIndex->GetPrimPath(dg, isSprim);
}

void MayaHydraSceneProducer::InsertRprim(
    MayaHydraAdapter* adapter,
    const TfToken& typeId,
    const SdfPath& id,
    const SdfPath& instancerId)
{
    return _sceneIndex->InsertPrim(adapter, typeId, id);
}

void MayaHydraSceneProducer::RemoveRprim(const SdfPath& id)
{
     _sceneIndex->RemovePrim(id);
}

void MayaHydraSceneProducer::MarkRprimDirty(const SdfPath& id, HdDirtyBits dirtyBits)
{
    _sceneIndex->MarkRprimDirty(id, dirtyBits);
}

void MayaHydraSceneProducer::MarkInstancerDirty(const SdfPath& id, HdDirtyBits dirtyBits)
{
    _sceneIndex->MarkInstancerDirty(id, dirtyBits);
}

void MayaHydraSceneProducer::InsertSprim(
    MayaHydraAdapter* adapter,
    const TfToken& typeId,
    const SdfPath& id,
    HdDirtyBits initialBits)
{
    _sceneIndex->InsertPrim(adapter, typeId, id);
}

void MayaHydraSceneProducer::RemoveSprim(const TfToken& typeId, const SdfPath& id)
{
    _sceneIndex->RemovePrim(id);
}

void MayaHydraSceneProducer::MarkSprimDirty(const SdfPath& id, HdDirtyBits dirtyBits)
{
    _sceneIndex->MarkSprimDirty(id, dirtyBits);
}

SdfPath MayaHydraSceneProducer::GetDelegateID(TfToken name) const
{
    return _sceneIndex->GetDelegateID(name);
}

void MayaHydraSceneProducer::PreFrame(const MHWRender::MDrawContext& drawContext)
{
    _sceneIndex->PreFrame(drawContext);
}

void MayaHydraSceneProducer::PostFrame()
{
    _sceneIndex->PostFrame();
}

void MayaHydraSceneProducer::RemoveAdapter(const SdfPath& id)
{
    return _sceneIndex->RemoveAdapter(id);
}

void MayaHydraSceneProducer::RecreateAdapterOnIdle(const SdfPath& id, const MObject& obj)
{
    return _sceneIndex->RecreateAdapterOnIdle(id, obj);
}

SdfPath MayaHydraSceneProducer::GetLightedPrimsRootPath() const
{
    return _sceneIndex->GetLightedPrimsRootPath();
}

void MayaHydraSceneProducer::MaterialTagChanged(const SdfPath& id)
{
    _sceneIndex->MaterialTagChanged(id);
}

GfInterval MayaHydraSceneProducer::GetCurrentTimeSamplingInterval() const
{
    return _sceneIndex->GetCurrentTimeSamplingInterval();
}

PXR_NAMESPACE_CLOSE_SCOPE
