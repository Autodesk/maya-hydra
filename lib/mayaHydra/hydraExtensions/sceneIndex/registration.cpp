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

#include "mayaHydraLib/hydraUtils.h"
#include "mayaHydraLib/sceneIndex/registration.h"
#include "mayaHydraLib/sceneIndex/mhMayaUsdProxyShapeSceneIndex.h"

#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/sceneIndex/fvpPathInterfaceSceneIndex.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>

#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/usd/sdf/path.h>

#if defined(MAYAHYDRALIB_MAYAUSDAPI_ENABLED)
    #include <mayaUsdAPI/proxyStage.h>
#endif

#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDag.h>
#include <maya/MMessage.h>
#include <maya/MNodeMessage.h>

#include <ufeExtensions/Global.h>

namespace {

// Pixar macros won't work without PXR_NS.
PXR_NAMESPACE_USING_DIRECTIVE

/// \class PathInterfaceSceneIndex
///
/// Implement the path interface for plugin scene indices.
///
/// FLOW_VIEWPORT_TODO  The following is USD-specific, generalize to all data
/// models.  PPT, 22-Sep-2023.

class PathInterfaceSceneIndex : public Fvp::PathInterfaceSceneIndexBase
{
public:

    static HdSceneIndexBaseRefPtr New(
        const HdSceneIndexBaseRefPtr& inputSceneIndex,
        const SdfPath&                sceneIndexPathPrefix
    )
    {
        return TfCreateRefPtr(
            new PathInterfaceSceneIndex(inputSceneIndex, sceneIndexPathPrefix));
    }

    SdfPath SceneIndexPath(const Ufe::Path& appPath) const override
    {
        // We only handle USD objects, so if the UFE path is not a USD object,
        // early out with failure.
        if (appPath.runTimeId() != UfeExtensions::getUsdRunTimeId()) {
            return {};
        }

        // Determine if the application path maps to our scene index.  At time
        // of writing the mapping is that the final component of the scene
        // index prefix is the Maya USD proxy shape node name, which is the
        // final component of the Ufe::Path first segment.  This is weak and
        // non-unique, and incrementing integer suffix schemes to fix this are
        // unintuitive and difficult to understand.  PPT, 26-Oct-2023.
        if (appPath.getSegments()[0].components().back().string() !=
            _sceneIndexPathPrefix.GetName()) {
            return {};
        }

        // The scene index path is composed of 2 parts, in order:
        // 1) The scene index path prefix, which is fixed on construction.
        // 2) The second segment of the UFE path, with each UFE path component
        //    becoming an SdfPath component.
        SdfPath sceneIndexPath = _sceneIndexPathPrefix;
        TF_AXIOM(appPath.nbSegments() == 2);
        const auto& secondSegment = appPath.getSegments()[1];
        for (const auto& pathComponent : secondSegment) {
            sceneIndexPath = sceneIndexPath.AppendChild(
                TfToken(pathComponent.string()));
        }
        return sceneIndexPath;
    }

private:

    PathInterfaceSceneIndex(
        const HdSceneIndexBaseRefPtr& inputSceneIndex,
        const SdfPath&                sceneIndexPathPrefix
    ) : PathInterfaceSceneIndexBase(inputSceneIndex)
      , _sceneIndexPathPrefix(sceneIndexPathPrefix)
    {}

    SdfPath _sceneIndexPathPrefix;
};

constexpr char kMayaUsdProxyShapeNode[] = { "mayaUsdProxyShape" };
}

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

// MayaHydraSceneIndexRegistration is used to register a scene index for a given dag node type.
MayaHydraSceneIndexRegistry::MayaHydraSceneIndexRegistry(const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy)
    : _renderIndexProxy(renderIndexProxy)
{
    MCallbackId id;
    MStatus     status;
    id = MDGMessage::addNodeAddedCallback(
        _SceneIndexNodeAddedCallback, kMayaUsdProxyShapeNode, this, &status);//We need only to monitor the MayaUsdProxyShapeNode
    if (TF_VERIFY(status == MS::kSuccess, "NodeAdded callback registration failed."))
        _sceneIndexDagNodeMessageCallbacks.append(id);
    id = MDGMessage::addNodeRemovedCallback(
        _SceneIndexNodeRemovedCallback, kMayaUsdProxyShapeNode, this, &status);//We need only to monitor the MayaUsdProxyShapeNode
    if (TF_VERIFY(status == MS::kSuccess, "NodeRemoved callback registration failed."))
        _sceneIndexDagNodeMessageCallbacks.append(id);

    static const MTypeId MAYAUSD_PROXYSHAPE_ID(0x58000095); //Hardcoded
        
    // Iterate over scene to find out existing node which will miss eventual dagNode added callbacks
    MItDag nodesDagIt(MItDag::kDepthFirst, MFn::kInvalid);
    for (; !nodesDagIt.isDone(); nodesDagIt.next()) {
        MObject dagNode(nodesDagIt.item(&status));
        //Act only on MayaUsdProxyShapeBase nodes
        MFnDependencyNode  dep(dagNode);
        if (MAYAUSD_PROXYSHAPE_ID != dep.typeId()) {
            continue; 
        }
        
        if (TF_VERIFY(status == MS::kSuccess)) {
            _AddSceneIndexForNode(dagNode);
        }
    }
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
    MDGMessage::removeCallbacks(_sceneIndexDagNodeMessageCallbacks);
    _sceneIndexDagNodeMessageCallbacks.clear();
    _registrationsByObjectHandle.clear();
    _registrations.clear();
}

bool MayaHydraSceneIndexRegistry::_RemoveSceneIndexForNode(const MObject& dagNode)
{
    MObjectHandle dagNodeHandle(dagNode);
    auto it = _registrationsByObjectHandle.find(dagNodeHandle);
    if (it != _registrationsByObjectHandle.end()) {
        MayaHydraSceneIndexRegistrationPtr registration(it->second);
        Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
        dataProducerSceneIndexInterface.removeViewportDataProducerSceneIndex(registration->rootSceneIndex);
        _registrationsByObjectHandle.erase(dagNodeHandle);
        _registrations.erase(registration->sceneIndexPathPrefix);
        return true;
    }
    return false;
}

#ifdef MAYAHYDRALIB_MAYAUSDAPI_ENABLED
void MayaHydraSceneIndexRegistry::_AddSceneIndexForNode(MObject& dagNode)
{
    //We receive only dag nodes of type MayaUsdProxyShapeNode
    MAYAUSDAPI_NS::ProxyStage proxyStage(dagNode);
    MayaHydraSceneIndexRegistrationPtr registration(new MayaHydraSceneIndexRegistration());

    //Add the usdimaging stage scene index chain as a data producer scene index in flow viewport

    //Since we want to insert a parent primitive for the stage scene index to be transformed or set visible/invisible, we need to set this scene indices chain 
    //before some of the instancing scene indices for UsdImaginsStageSceneIndex, and there is a slot for that purpose which is createInfo.overridesSceneIndexCallback
    //With this callback you can insert some scene indices which will be applied before the prototype scene indices.
    //This will be done inside Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex later
    UsdImagingCreateSceneIndicesInfo createInfo;
        
    auto stage = proxyStage.getUsdStage();
    // Check whether the pseudo-root has children
    if (stage && (!stage->GetPseudoRoot().GetChildren().empty())) {
        createInfo.stage = stage;//Add the stage to the creation parameters
    }
            
    MStatus  status;
    MDagPath dagPath(MDagPath::getAPathTo(dagNode, &status));
    if (TF_VERIFY(status == MS::kSuccess, "Incapable of finding dag path to given node")) {
        registration->dagNode = MObjectHandle(dagNode);
        // Construct the scene index path prefix appended to each rprim created by it.
        // It is composed of the "scene index plugin's name" + "dag node name" +
        // "disambiguator" The dag node name disambiguator is necessary in situation
        // where node name isn't unique and may clash with other node defined by the
        // same plugin.
        MFnDependencyNode dependNodeFn(dagNode);
        std::string dependNodeNameString (dependNodeFn.name().asChar());
        SanitizeNameForSdfPath(dependNodeNameString);
                
        registration->sceneIndexPathPrefix = 
                    SdfPath::AbsoluteRootPath()
                    .AppendPath(SdfPath(dependNodeNameString
                        + (dependNodeFn.hasUniqueName()
                                ? ""
                                : "__" + std::to_string(_incrementedCounterDisambiguator++))));


        //We will get the following scene indices from Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex
        HdSceneIndexBaseRefPtr finalSceneIndex = nullptr;
        UsdImagingStageSceneIndexRefPtr stageSceneIndex = nullptr;

        PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
            Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex(createInfo, finalSceneIndex, stageSceneIndex, 
                                                                                                registration->sceneIndexPathPrefix, (void*)&dagNode);
        if (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex){
            TF_CODING_ERROR("Error (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex) !");
        }
        
        //Create maya usd proxy shape scene index, since this scene index contains maya data, it cannot be added by the flow viewport API
        auto mayaUsdProxyShapeSceneIndex = MAYAHYDRA_NS_DEF::MayaUsdProxyShapeSceneIndex::New(proxyStage, finalSceneIndex, stageSceneIndex, MObjectHandle(dagNode));
        registration->pluginSceneIndex = mayaUsdProxyShapeSceneIndex;
        registration->interpretRprimPathFn = &(MAYAHYDRA_NS_DEF::MayaUsdProxyShapeSceneIndex::InterpretRprimPath);
        mayaUsdProxyShapeSceneIndex->Populate();

        registration->rootSceneIndex = registration->pluginSceneIndex;

        //Add the PathInterfaceSceneIndex which must be the last scene index, it is used for selection highlighting
        registration->rootSceneIndex = PathInterfaceSceneIndex::New(
                registration->rootSceneIndex,
                registration->sceneIndexPathPrefix);

        //Set the chain back into the dataProducerSceneIndexData in both members
        dataProducerSceneIndexData->SetDataProducerSceneIndex(registration->rootSceneIndex);
        dataProducerSceneIndexData->SetDataProducerLastSceneIndexChain(registration->rootSceneIndex);

        //Add this chain scene index to the render index proxy from all viewports
        const bool bRes = Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageDataProducerSceneIndexDataBaseToAllViewports(dataProducerSceneIndexData);
        if (false == bRes){
            TF_CODING_ERROR("Fvp::DataProducerSceneIndexInterfaceImp::get().addDataProducerSceneIndex returned false !");
        }

        // Add registration record if everything succeeded
        _registrations.insert({ registration->sceneIndexPathPrefix, registration });
        _registrationsByObjectHandle.insert({ registration->dagNode, registration });
    }
}
#else
namespace
{
    constexpr char kSceneIndexPluginSuffix[] = {"MayaNodeSceneIndexPlugin"};
}
void MayaHydraSceneIndexRegistry::_AddSceneIndexForNode(MObject& dagNode)
{
    MFnDependencyNode dependNodeFn(dagNode);
    // Name must match Plugin TfType registration thus must begin with upper case
    std::string sceneIndexPluginName(dependNodeFn.typeName().asChar());
    sceneIndexPluginName[0] = toupper(sceneIndexPluginName[0]);
    sceneIndexPluginName += kSceneIndexPluginSuffix;
    TfToken sceneIndexPluginId(sceneIndexPluginName);

    static HdSceneIndexPluginRegistry& sceneIndexPluginRegistry
        = HdSceneIndexPluginRegistry::GetInstance();
    if (sceneIndexPluginRegistry.IsRegisteredPlugin(sceneIndexPluginId)) {
        using MayaHydraMObjectDataSource = HdRetainedTypedSampledDataSource<MObject>;
        using MayaHydraVersionDataSource = HdRetainedTypedSampledDataSource<int>;
        // Functions retrieved from the scene index plugin
        using MayaHydraInterpretRprimPathDataSource
            = HdRetainedTypedSampledDataSource<MayaHydraInterpretRprimPath&>;

        // Create the registration record which is then added into the registry if everything
        // succeeds
        static TfToken sDataSourceEntryNames[] { TfToken("object"),
                                                 TfToken("version"),
                                                 TfToken("interpretRprimPath") };
        constexpr int  kDataSourceNumEntries = sizeof(sDataSourceEntryNames) / sizeof(TfToken);
        MayaHydraSceneIndexRegistrationPtr registration(new MayaHydraSceneIndexRegistration());
        HdDataSourceBaseHandle             values[] { MayaHydraMObjectDataSource::New(dagNode),
                                          MayaHydraVersionDataSource::New(MAYAHYDRA_API_VERSION),
                                          MayaHydraInterpretRprimPathDataSource::New(
                                              registration->interpretRprimPathFn) };
        static_assert(
            sizeof(values) / sizeof(HdDataSourceBaseHandle) == kDataSourceNumEntries,
            "Incorrect number of data source entries");
        registration->pluginSceneIndex = sceneIndexPluginRegistry.AppendSceneIndex(
            sceneIndexPluginId,
            nullptr,
            HdRetainedContainerDataSource::New(kDataSourceNumEntries, sDataSourceEntryNames, values));
        if (TF_VERIFY(
                registration->pluginSceneIndex,
                "MayaHydraSceneIndexRegistry::_AddSceneIndexForNode failed to create %s scene index from given "
                "node type.",
                sceneIndexPluginName.c_str())) {

            MStatus  status;
            MDagPath dagPath(MDagPath::getAPathTo(dagNode, &status));
            if (TF_VERIFY(status == MS::kSuccess, "Incapable of finding dag path to given node")) {
                registration->dagNode = MObjectHandle(dagNode);
                // Construct the scene index path prefix appended to each rprim created by it.
                // It is composed of the "scene index plugin's name" + "dag node name" +
                // "disambiguator" The dag node name disambiguator is necessary in situation
                // where node name isn't unique and may clash with other node defined by the
                // same plugin.
                std::string dependNodeNameString (dependNodeFn.name().asChar());
                SanitizeNameForSdfPath(dependNodeNameString);
                
                registration->sceneIndexPathPrefix = 
                          SdfPath::AbsoluteRootPath()
                          .AppendPath(SdfPath(sceneIndexPluginName))
                          .AppendPath(SdfPath(dependNodeNameString
                              + (dependNodeFn.hasUniqueName()
                                     ? ""
                                     : "__" + std::to_string(_incrementedCounterDisambiguator++))));

                registration->rootSceneIndex = registration->pluginSceneIndex;
                
                // Because the path interface scene index must be the last one
                // in the chain, add the prefixing scene index here, instead of
                // relying on the render index proxy doing it for us.
                registration->rootSceneIndex = HdPrefixingSceneIndex::New(
                    registration->rootSceneIndex,
                    registration->sceneIndexPathPrefix);

                registration->rootSceneIndex = PathInterfaceSceneIndex::New(
                    registration->rootSceneIndex,
                    registration->sceneIndexPathPrefix);

                // By inserting the scene index was inserted into the render index using a custom
                // prefix, the chosen prefix will be prepended to rprims tied to that scene index
                // automatically.
                constexpr bool needsPrefixing = false;
                _renderIndexProxy->InsertSceneIndex(
                    registration->rootSceneIndex, 
                    registration->sceneIndexPathPrefix, needsPrefixing);
                static SdfPath maya126790Workaround("maya126790Workaround");
                registration->pluginSceneIndex->GetPrim(maya126790Workaround);

                // Add registration record if everything succeeded
                _registrations.insert({ registration->sceneIndexPathPrefix, registration });
                _registrationsByObjectHandle.insert({ registration->dagNode, registration });
            }
        }
    }
}
#endif //MAYAHYDRALIB_MAYAUSDAPI_ENABLED

void MayaHydraSceneIndexRegistry::_SceneIndexNodeAddedCallback(MObject& dagNode, void* clientData)
{
    if (dagNode.isNull() || dagNode.apiType() != MFn::kPluginShape)
        return;
    auto renderOverride = static_cast<MayaHydraSceneIndexRegistry*>(clientData);
    renderOverride->_AddSceneIndexForNode(dagNode);
}

void MayaHydraSceneIndexRegistry::_SceneIndexNodeRemovedCallback(MObject& dagNode, void* clientData)
{
    if (dagNode.isNull() || dagNode.apiType() != MFn::kPluginShape)
        return;
    auto renderOverride = static_cast<MayaHydraSceneIndexRegistry*>(clientData);
    renderOverride->_RemoveSceneIndexForNode(dagNode);
}

PXR_NAMESPACE_CLOSE_SCOPE
