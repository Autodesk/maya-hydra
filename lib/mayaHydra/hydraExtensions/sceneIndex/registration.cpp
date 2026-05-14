//
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "mayaHydraLib/mayaUtils.h"
#include "mayaHydraLib/mixedUtils.h"
#include "mayaHydraLib/sceneIndex/registration.h"
#include "mayaHydraLib/sceneIndex/mhExternalCameraOverrideSceneIndex.h"
#include "mayaHydraLib/sceneIndex/mhExternalCameraResolvingSceneIndex.h"
#include "mayaHydraLib/sceneIndex/mhMayaUsdProxyShapeSceneIndex.h"

#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>
#include <flowViewport/sceneIndex/fvpPrimRemovalEnforcingSceneIndex.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/selection/fvpPrefixPathMapper.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/fvpUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/prefixingSceneIndex.h>

#include <mayaUsdAPI/proxyStage.h>

#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDag.h>
#include <maya/MMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/MFileIO.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include <ufeExtensions/Global.h>

#include <optional>

namespace {

constexpr char kMayaUsdProxyShapeNode[] = { "mayaUsdProxyShape" };

const Ufe::Path usdDefaultRenderSettingsNodePath(Ufe::PathSegment(
    Ufe::PathComponent(std::string(MayaHydra::kUsdDefaultRenderSettingsNodeName)),
    UfeExtensions::getMayaRunTimeId(), '\0'));

} // namespace

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

struct MayaUsdSceneIndexRegistration : public MayaHydraSceneIndexRegistration
{
    void Update() override {
        auto proxyShapeSceneIndex
            = TfDynamic_cast<::MayaHydra::MayaUsdProxyShapeSceneIndexBaseRefPtr>(pluginSceneIndex);
        proxyShapeSceneIndex->UpdateTime();
    }

#ifdef CODE_COVERAGE_WORKAROUND
    void Destroy() override {
        auto proxyShapeSceneIndex = TfDynamic_cast<MayaUsdProxyShapeSceneIndexBaseRefPtr>(pluginSceneIndex);
        proxyShapeSceneIndex->_Destroy();
    }
#endif
};

// MayaHydraSceneIndexRegistration is used to register a scene index for
// mayaUsdPlugin proxy shape nodes.
    MayaHydraSceneIndexRegistry::MayaHydraSceneIndexRegistry(
          const HdSceneIndexBaseRefPtr& dataProducerMergingSceneIndex,
          bool                          interactive)
    : _dataProducerMergingSceneIndex(dataProducerMergingSceneIndex)
    , _interactive(interactive)
{
    if (!MFnPlugin::isNodeRegistered(kMayaUsdProxyShapeNode)) {
        MGlobal::displayWarning("mayaUsdPlugin not loaded, cannot be registered to Maya Hydra.  Please load mayaUsdPlugin, then switch back to a Maya Hydra viewport renderer.");
        return;
    }

    MCallbackId id;
    MStatus     status;
    id = MDGMessage::addNodeAddedCallback(
        _SceneIndexNodeAddedCallback, kMayaUsdProxyShapeNode, this, &status);//We need only to monitor the MayaUsdProxyShapeNode
    if (TF_VERIFY(status == MS::kSuccess, "NodeAdded callback registration failed."))
        _DGCallbackIds.append(id);
    id = MDGMessage::addNodeRemovedCallback(
        _SceneIndexNodeRemovedCallback, kMayaUsdProxyShapeNode, this, &status);//We need only to monitor the MayaUsdProxyShapeNode
    if (TF_VERIFY(status == MS::kSuccess, "NodeRemoved callback registration failed."))
        _DGCallbackIds.append(id);

    //Because we cannot process a node while loading a maya file, we are storing them in an array in _SceneIndexNodeAddedCallback 
    // and process them once the after load has completed through _AfterOpenCallback.
    _AfterOpenCBId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, _AfterOpenCallback, this, &status);
    TF_VERIFY(status == MS::kSuccess, "MSceneMessage::kAfterOpen callback registration failed.");

    static const MTypeId MAYAUSD_PROXYSHAPE_ID(0x58000095); //Hardcoded
        
    // Iterate over scene to find out existing node which will miss eventual dagNode added callbacks
    MItDag nodesDagIt(MItDag::kDepthFirst, MFn::kInvalid);
    for (; !nodesDagIt.isDone(); nodesDagIt.next()) {
        MObject dagNode(nodesDagIt.currentItem(&status));
        //Act only on MayaUsdProxyShapeBase nodes
        MFnDependencyNode  dep(dagNode);
        if (MAYAUSD_PROXYSHAPE_ID != dep.typeId()) {
            continue; 
        }
        
        if (TF_VERIFY(status == MS::kSuccess)) {
            _AddSceneIndexForNode(dagNode);
        }
    }

    _RegisterDefaultRenderSettingsNode();
}

// Retrieve information relevant to registration such as UFE compatibility of a particular scene
// index
MayaHydraSceneIndexRegistrationPtr
MayaHydraSceneIndexRegistry::GetSceneIndexRegistrationForRprim(const SdfPath& rprimPath) const
{
    const std::string& rprimPathAsString = rprimPath.GetString();
    for (auto& reg : _registrations){
        auto& key = reg.first.GetString();
        const size_t found = rprimPathAsString.find(key);
        if (found != std::string::npos){
            return reg.second;
        }
    }
    
    return nullptr;
}

const MayaHydraSceneIndexRegistry::Registrations& 
MayaHydraSceneIndexRegistry::GetRegistrations() const
{
    return _registrations;
}

MayaHydraSceneIndexRegistry::~MayaHydraSceneIndexRegistry()
{
    MDGMessage::removeCallbacks(_DGCallbackIds);
    _DGCallbackIds.clear();
    if (_AfterOpenCBId){
        MSceneMessage::removeCallback(_AfterOpenCBId);
    }
    _AfterOpenCBId = 0;
    _UnregisterDefaultRenderSettingsNode();
    _RemoveAllSceneIndexNodes();
    _registrationsByObjectHandle.clear();
    _registrations.clear();
}

void MayaHydraSceneIndexRegistry::_RemoveAllSceneIndexNodes()
{
    //Always take the first element and remove it until it is empty
    while (_registrationsByObjectHandle.begin() != _registrationsByObjectHandle.end()){
        _RemoveSceneIndexForNode(_registrationsByObjectHandle.begin()->first.object());
    }
}

bool MayaHydraSceneIndexRegistry::_RemoveSceneIndexForNode(const MObject& dagNode)
{
    MObjectHandle dagNodeHandle(dagNode);
    auto it = _registrationsByObjectHandle.find(dagNodeHandle);
    if (it != _registrationsByObjectHandle.end()) {
        MayaHydraSceneIndexRegistrationPtr registration(it->second);
        Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
        dataProducerSceneIndexInterface.removeDataProducerSceneIndex(registration->rootSceneIndex);
        _registrationsByObjectHandle.erase(dagNodeHandle);
        _registrations.erase(registration->sceneIndexPathPrefix);
#ifdef CODE_COVERAGE_WORKAROUND
        registration->Destroy();
        Fvp::leakSceneIndex(registration->rootSceneIndex);
#endif
        return true;
    }
    return false;
}

void MayaHydraSceneIndexRegistry::_AddSceneIndexForNode(MObject& dagNode)
{
    const MayaHydraSceneIndexRegistrationPtr registration(new MayaUsdSceneIndexRegistration());

    MStatus  status;
    MDagPath dagPath;
    status = MDagPath::getAPathTo(dagNode, dagPath);
    if (!TF_VERIFY(status == MS::kSuccess, "Unable to find Dag path to given node")) {
        return;
    }

    registration->dagNode = MObjectHandle(dagNode);
    registration->sceneIndexPathPrefix = sceneIndexPathPrefix(
        _dataProducerMergingSceneIndex, dagNode);
        
    //We receive only dag nodes of type MayaUsdProxyShapeNode
    MAYAUSDAPI_NS::ProxyStage proxyStage(dagNode);

    //Add the usdimaging stage scene index chain as a data producer scene index in flow viewport

    //Since we want to insert a parent primitive for the stage scene index to be transformed or set visible/invisible, we need to set this scene indices chain 
    //before some of the instancing scene indices for UsdImaginsStageSceneIndex, and there is a slot for that purpose which is createInfo.overridesSceneIndexCallback
    //With this callback you can insert some scene indices which will be applied before the prototype scene indices.
    //This will be done inside Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex later
    UsdImagingCreateSceneIndicesInfo createInfo;
    // Insert the ExternalCameraOverrideSceneIndex into the scene index
    // chain. This callback will be applied by the
    // Fvp::DataProducerSceneIndexDataBase.
    createInfo.overridesSceneIndexCallback = [](HdSceneIndexBaseRefPtr const& inputSi) {
        return MayaHydra::MhExternalCameraOverrideSceneIndex::New(inputSi);
    };

    auto stage = proxyStage.getUsdStage();
    // Check whether the pseudo-root has children
    if (stage && (!stage->GetPseudoRoot().GetChildren().empty())) {
        createInfo.stage = stage;//Add the stage to the creation parameters
    }
            
    //We will get the following scene indices from Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex
    HdSceneIndexBaseRefPtr finalSceneIndex = nullptr;
    UsdImagingStageSceneIndexRefPtr stageSceneIndex = nullptr;

    // We are explicitly adding a prefixing scene index just downstream (after)
    // the MayaUsdProxyShapeSceneIndex.  To register a mapping from the Maya
    // node to the prefix SdfPath, we give send 
    // registration->sceneIndexPathPrefix to .addUsdStageSceneIndex but it will
    // be used only to register the mapping.
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
        Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex(createInfo, finalSceneIndex, stageSceneIndex, 
                                                                             registration->sceneIndexPathPrefix, (void*)&dagNode);
    if (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex){
        TF_CODING_ERROR("Error (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex) !");
    }
        
    // Create Maya USD proxy shape scene index.  Since this scene index
    // contains Maya data, it cannot be added by the Flow Viewport API.
    // Pass in the scene index prefix for the proxy shape scene index, so it
    // can register a pick handler.
    auto mayaUsdProxyShapeSceneIndex = _interactive ? 
        MayaUsdProxyShapeSceneIndexBaseRefPtr(MayaUsdProxyShapeSceneIndex::New(proxyStage, finalSceneIndex, stageSceneIndex, MObjectHandle(dagNode), registration->sceneIndexPathPrefix, Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath)))) :
        MayaUsdProxyShapeSceneIndexBase::New(proxyStage, finalSceneIndex, stageSceneIndex, MObjectHandle(dagNode));
    registration->pluginSceneIndex = mayaUsdProxyShapeSceneIndex;
    registration->interpretRprimPathFn = &(MayaUsdProxyShapeSceneIndex::InterpretRprimPath);
    mayaUsdProxyShapeSceneIndex->Populate();

    // This sets the required prefix just downstream (after) the
    // MayaUsdProxyShapeSceneIndex, as required.
    auto pfsi = HdPrefixingSceneIndex::New(
        registration->pluginSceneIndex,
        registration->sceneIndexPathPrefix);

    auto externalCameraResolvingSi = MayaHydra::ExternalCameraResolvingSceneIndex::New(pfsi);

    // Add a scene index that enforces a coherent prim removal + prim retrieval behavior.
    // This is to work around a bug in OpenUSD with the UsdImagingDrawModeSceneIndex where 
    // it can send PrimsRemoved notifications, but still return valid prims and prim paths 
    // when calling GetPrim and GetChildPrimPaths afterwards. This issue has likely been
    // latent for a while, but became an active problem starting with USD 25.02, with this
    // change in HdMergingSceneIndex : 
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/v25.08/pxr/imaging/hd/mergingSceneIndex.cpp#L507-L533
    // See fvpPrimRemovalEnforcingSceneIndex.h for a more detailed breakdown of the issue.
    auto primRemovalSi = Fvp::PrimRemovalEnforcingSceneIndex::New(externalCameraResolvingSi);

    registration->rootSceneIndex = primRemovalSi;

    //Set the chain back into the dataProducerSceneIndexData in both members
    dataProducerSceneIndexData->SetDataProducerSceneIndex(registration->rootSceneIndex);
    dataProducerSceneIndexData->SetDataProducerLastSceneIndexChain(registration->rootSceneIndex);

    //Add this chain scene index to the render index proxy from all views
    const bool bRes = Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageDataProducerSceneIndexDataBaseToAllViews(dataProducerSceneIndexData);
    if (false == bRes){
        TF_CODING_ERROR("Fvp::DataProducerSceneIndexInterfaceImp::get().addDataProducerSceneIndex returned false !");
    }

    // Add registration record if everything succeeded
    _registrations.insert({ registration->sceneIndexPathPrefix, registration });
    _registrationsByObjectHandle.insert({ registration->dagNode, registration });
}

void MayaHydraSceneIndexRegistry::_SceneIndexNodeAddedCallback(MObject& dagNode, void* clientData)
{
    if (dagNode.isNull() || dagNode.apiType() != MFn::kPluginShape){
        return;
    }

    auto mayaHydraSceneIndexRegistry = static_cast<MayaHydraSceneIndexRegistry*>(clientData);
    if (MFileIO::isOpeningFile()){
        //We cannot process a node while loading a file
        mayaHydraSceneIndexRegistry->_AppendNodeToProcessAfterOpenScene(dagNode);
    }else{
        mayaHydraSceneIndexRegistry->_AddSceneIndexForNode(dagNode);
    }
}

//We need to check if some nodes that need to be processed were added to our array during a file load
void MayaHydraSceneIndexRegistry::_AfterOpenCallback(void *clientData) 
{
    if (! clientData){
        return;
    }

    auto mayaHydraSceneIndexRegistry = static_cast<MayaHydraSceneIndexRegistry*>(clientData);
    mayaHydraSceneIndexRegistry->_ProcessNodesAfterOpen();
}

void MayaHydraSceneIndexRegistry::_SceneIndexNodeRemovedCallback(MObject& dagNode, void* clientData)
{
    if (dagNode.isNull() || dagNode.apiType() != MFn::kPluginShape)
        return;
    auto renderOverride = static_cast<MayaHydraSceneIndexRegistry*>(clientData);
    renderOverride->_RemoveSceneIndexForNode(dagNode);
}

void MayaHydraSceneIndexRegistry::_ProcessNodesAfterOpen()
{
    for (auto& dagNode : _nodesToProcessAfterOpenScene){
        if (dagNode.isNull() || dagNode.apiType() != MFn::kPluginShape){
            continue;
        }
        _AddSceneIndexForNode(dagNode);
    }
    _nodesToProcessAfterOpenScene.clear();

    _RegisterDefaultRenderSettingsNode();
}

void MayaHydraSceneIndexRegistry::ApplyPendingUpdates()
{
    for (auto& reg : _registrations) {
        auto& registration = reg.second;
        if (registration->pluginSceneIndex) {
            MayaUsdProxyShapeSceneIndexRefPtr proxyShapeSceneIndex
                = TfDynamic_cast<MayaUsdProxyShapeSceneIndexRefPtr>(registration->pluginSceneIndex);
            if (proxyShapeSceneIndex && proxyShapeSceneIndex->HasPendingUpdates()) {
                proxyShapeSceneIndex->PopulateAndApplyPendingChanges();
            }
        }
    }
}

SdfPath MayaHydraSceneIndexRegistry::_usdDefaultRenderSettingsPathPrefix;

SdfPath MayaHydraSceneIndexRegistry::GetUsdDefaultRenderSettingsPathPrefix()
{
    return _usdDefaultRenderSettingsPathPrefix;
}

void MayaHydraSceneIndexRegistry::_RegisterDefaultRenderSettingsNode()
{
    if (_defaultRenderSettingsDataProducer) {
        return;
    }

    MObject nodeObj;
    if (!GetDependNodeFromNodeName(kUsdDefaultRenderSettingsNodeName.data(), nodeObj)) {
        return;
    }

    MAYAUSDAPI_NS::ProxyStage proxyStage(nodeObj);
    auto stage = proxyStage.getUsdStage();
    if (!stage || stage->GetPseudoRoot().GetChildren().empty()) {
        return;
    }

    UsdImagingCreateSceneIndicesInfo createInfo;
    createInfo.stage = stage;
    createInfo.overridesSceneIndexCallback = [](HdSceneIndexBaseRefPtr const& inputSi) {
        return MayaHydra::MhExternalCameraOverrideSceneIndex::New(inputSi);
    };

    SdfPath prefix = sceneIndexPathPrefix(
        _dataProducerMergingSceneIndex, nodeObj);

    HdSceneIndexBaseRefPtr finalSceneIndex = nullptr;
    UsdImagingStageSceneIndexRefPtr stageSceneIndex = nullptr;
    auto dataProducerSIData = Fvp::DataProducerSceneIndexInterfaceImp::get()
        .addUsdStageSceneIndex(createInfo, finalSceneIndex, stageSceneIndex,
                               prefix, /*dccNode=*/nullptr);

    if (!dataProducerSIData || !finalSceneIndex || !stageSceneIndex) {
        TF_CODING_ERROR("Failed to create USD stage scene index for UsdDefaultRenderSettings.");
        return;
    }

    auto pfsi = HdPrefixingSceneIndex::New(finalSceneIndex, prefix);
    auto externalCameraResolvingSi =
        MayaHydra::ExternalCameraResolvingSceneIndex::New(pfsi);
    auto primRemovalSi =
        Fvp::PrimRemovalEnforcingSceneIndex::New(externalCameraResolvingSi);

    dataProducerSIData->SetDataProducerSceneIndex(primRemovalSi);
    dataProducerSIData->SetDataProducerLastSceneIndexChain(primRemovalSi);

    Fvp::DataProducerSceneIndexInterfaceImp::get()
        .addUsdStageDataProducerSceneIndexDataBaseToAllViews(dataProducerSIData);

    _usdDefaultRenderSettingsPathPrefix = prefix;
    _defaultRenderSettingsDataProducer = dataProducerSIData;

    auto pathMapper = std::make_shared<Fvp::PrefixPathMapper>(
        usdDefaultRenderSettingsNodePath, prefix);
    Fvp::PathMapperRegistry::Instance().Register(
        usdDefaultRenderSettingsNodePath, pathMapper);
}

void MayaHydraSceneIndexRegistry::_UnregisterDefaultRenderSettingsNode()
{
    if (!_defaultRenderSettingsDataProducer) {
        return;
    }

    Fvp::PathMapperRegistry::Instance().Unregister(
        usdDefaultRenderSettingsNodePath);

    Fvp::DataProducerSceneIndexInterface::get()
        .removeDataProducerSceneIndex(
            _defaultRenderSettingsDataProducer->GetDataProducerLastSceneIndexChain());
    _defaultRenderSettingsDataProducer = TfNullPtr;
    _usdDefaultRenderSettingsPathPrefix = SdfPath();
}

PXR_NAMESPACE_CLOSE_SCOPE
