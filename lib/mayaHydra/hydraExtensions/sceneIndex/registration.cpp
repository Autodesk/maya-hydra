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

#include "mayaHydraLib/mixedUtils.h"
#include "mayaHydraLib/sceneIndex/registration.h"
#include "mayaHydraLib/sceneIndex/mhMayaUsdProxyShapeSceneIndex.h"

#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/sceneIndex/fvpPathInterfaceSceneIndex.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif

#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>
#include <pxr/usd/sdf/path.h>

#if defined(MAYAHYDRALIB_MAYAUSDAPI_ENABLED)
    #include <mayaUsdAPI/proxyStage.h>
#endif

#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDag.h>
#include <maya/MMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/MFileIO.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>

#include <ufeExtensions/Global.h>

#include <ufe/observer.h>
#include <ufe/sceneNotification.h>
#include <ufe/scene.h>

namespace {

const std::string digits = "0123456789";

// Pixar macros won't work without PXR_NS.
PXR_NAMESPACE_USING_DIRECTIVE
using namespace Ufe;

// UFE Observer that unpacks SceneCompositeNotification's.  Belongs in UFE
// itself.
class SceneObserver : public Observer
{
public:
    SceneObserver() = default;

    virtual void handleOp(const SceneCompositeNotification::Op& op) = 0;

private:
    void operator()(const Notification& notification) override 
    {
        const auto& sceneChanged = notification.staticCast<SceneChanged>();
        
        if (SceneChanged::SceneCompositeNotification == sceneChanged.opType()) {
            const auto& compNotification = notification.staticCast<SceneCompositeNotification>();
            for(const auto& op : compNotification) {
                handleOp(op);
            }
        } else {
            handleOp(sceneChanged);
        }
    }
};

HdDataSourceBaseHandle createInstanceSelectionDataSource(const SdfPath& instancerPrimPath, int instanceIndex)
{
    HdInstanceIndicesSchema::Builder instanceIndicesBuilder;
    instanceIndicesBuilder.SetInstancer(HdRetainedTypedSampledDataSource<SdfPath>::New(instancerPrimPath));
    instanceIndicesBuilder.SetInstanceIndices(HdRetainedTypedSampledDataSource<VtArray<int>>::New({instanceIndex}));
    HdSelectionSchema::Builder selectionBuilder;
    // Instancer is expected to be marked "fully selected" even if only certain instances are selected,
    // based on USD's _AddToSelection function in selectionSceneIndexObserver.cpp :
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/imaging/hdx/selectionSceneIndexObserver.cpp#L212-L251
    selectionBuilder.SetFullySelected(HdRetainedTypedSampledDataSource<bool>::New(true));
    auto instanceIndicesDataSource = HdDataSourceBase::Cast(instanceIndicesBuilder.Build());
    selectionBuilder.SetNestedInstanceIndices(HdRetainedSmallVectorDataSource::New(1, &instanceIndicesDataSource));
    return HdDataSourceBase::Cast(selectionBuilder.Build());
}

HdDataSourceBaseHandle createFullPrimSelectionDataSource()
{
    HdSelectionSchema::Builder selectionBuilder;
    selectionBuilder.SetFullySelected(HdRetainedTypedSampledDataSource<bool>::New(true));
    return HdDataSourceBase::Cast(selectionBuilder.Build());
}

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
        const SdfPath&                sceneIndexPathPrefix,
        const Ufe::Path&              sceneIndexAppPath
    )
    {
        return TfCreateRefPtr(new PathInterfaceSceneIndex(
            inputSceneIndex, sceneIndexPathPrefix, sceneIndexAppPath));
    }

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override
    {
        // We only handle USD objects, so if the UFE path is not a USD object,
        // early out with failure.
        if (appPath.runTimeId() != UfeExtensions::getUsdRunTimeId()) {
            return {};
        }

        // If the data model object application path does not match the path we
        // translate, return an empty path.
        if (!appPath.startsWith(_sceneIndexAppPath)) {
            return {};
        }

        // The scene index path is composed of 2 parts, in order:
        // 1) The scene index path prefix, which is fixed on construction.
        // 2) The second segment of the UFE path, with each UFE path component
        //    becoming an SdfPath component. If the last component is a number,
        //    then we are dealing with an instance selection.
        SdfPath primPath = _sceneIndexPathPrefix;
        TF_AXIOM(appPath.nbSegments() == 2);

        bool lastComponentIsNumeric = false;
        const auto& secondSegment = appPath.getSegments()[1];
        for (const auto& pathComponent : secondSegment) {
            if (pathComponent.string().find_first_not_of(digits) == std::string::npos) {
                // This should only occur on the last component, when we have an instance selection
                if (TF_VERIFY(pathComponent == secondSegment.components().back())) {
                    lastComponentIsNumeric = true;
                }
                continue;
            }
            primPath = primPath.AppendChild(TfToken(pathComponent.string()));
        }

        const auto lastComponentString = secondSegment.components().back().string();
        HdDataSourceBaseHandle selectionDataSource = lastComponentIsNumeric 
            ? createInstanceSelectionDataSource(primPath, std::stoi(lastComponentString))
            : createFullPrimSelectionDataSource();
        Fvp::PrimSelections primSelections({{primPath, selectionDataSource}});

        // Propagate selection to propagated prototypes
        auto ancestorsRange = primPath.GetAncestorsRange();
        for (const auto& ancestorPath : ancestorsRange) {
            HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(ancestorPath);
            UsdImagingUsdPrimInfoSchema usdPrimInfo = UsdImagingUsdPrimInfoSchema::GetFromParent(currPrim.dataSource);
            if (!usdPrimInfo.IsDefined()) {
                continue;
            }
            auto propagatedProtosDataSource = usdPrimInfo.GetPiPropagatedPrototypes();
            if (!propagatedProtosDataSource) {
                continue;
            }
            auto propagatedProtoNames = propagatedProtosDataSource->GetNames();
            for (const auto& propagatedProtoName : propagatedProtoNames) {
                auto propagatedProtoPathDataSource = HdTypedSampledDataSource<SdfPath>::Cast(propagatedProtosDataSource->Get(propagatedProtoName));
                if (propagatedProtoPathDataSource) {
                    SdfPath propagatedProtoPath = propagatedProtoPathDataSource->GetTypedValue(0);
                    SdfPath propagatedPrimPath = primPath.ReplacePrefix(ancestorPath, propagatedProtoPath);
                    HdSceneIndexPrim propagatedPrim = GetInputSceneIndex()->GetPrim(propagatedPrimPath);
                    // This check controls which types of prims have their selection data source propagated. Currently we skip
                    // instancers so that selecting an instancer A that is both drawing geometry but also prototyped and propagated
                    // for another instancer B will only mark the geometry-drawing instancer A as selected. This can be changed.
                    // For now (2024/05/28), this only affects selection highlighting.
                    if (propagatedPrim.primType != HdPrimTypeTokens->instancer) {
                        primSelections.push_back({propagatedPrimPath, selectionDataSource});
                    }
                }
            }
            break; // We found propagated prototypes, exit now to avoid propagating selection to prototypes of other parents 
        }

        return primSelections;
    }

    const Ufe::Path& GetSceneIndexAppPath() const { return _sceneIndexAppPath; }
    void SetSceneIndexAppPath(const Ufe::Path& sceneIndexAppPath) { 
        _sceneIndexAppPath = sceneIndexAppPath;
    }

private:

    class PathInterfaceSceneObserver : public SceneObserver
    {
    public:
        PathInterfaceSceneObserver(PathInterfaceSceneIndex& pi)
        : SceneObserver(), _pi(pi)
        {}

    private:
    
        void handleOp(const SceneCompositeNotification::Op& op) override {
            if (op.opType == SceneChanged::ObjectPathChange &&
                ((op.subOpType == ObjectPathChange::ObjectReparent) ||
                 (op.subOpType == ObjectPathChange::ObjectRename))) {
                const auto& siPath = _pi.GetSceneIndexAppPath();
                if (siPath.startsWith(op.path)) {
                    _pi.SetSceneIndexAppPath(siPath.reparent(op.path, op.item->path()));
                }
            }
        }
    
        PathInterfaceSceneIndex& _pi;
    };

    PathInterfaceSceneIndex(
        const HdSceneIndexBaseRefPtr& inputSceneIndex,
        const SdfPath&                sceneIndexPathPrefix,
        const Ufe::Path&              sceneIndexAppPath

    ) : PathInterfaceSceneIndexBase(inputSceneIndex)
      , _sceneIndexPathPrefix(sceneIndexPathPrefix)
      , _appSceneObserver(std::make_shared<PathInterfaceSceneObserver>(*this))
      , _sceneIndexAppPath(sceneIndexAppPath)
    {
        // The gateway node (proxy shape) is a Maya node, so the scene index
        // path must be a single segment.
        TF_AXIOM(sceneIndexAppPath.nbSegments() == 1);

        // Observe the scene to be informed of path changes to the gateway node
        // (proxy shape) that corresponds to our scene index data producer.
        Scene::instance().addObserver(_appSceneObserver);
    }

    ~PathInterfaceSceneIndex() {
        // Ufe::Subject has automatic cleanup of stale observers, but this can
        // be problematic on application exit if the library of the observer is
        // cleaned up before that of the subject, so simply stop observing.
        Scene::instance().removeObserver(_appSceneObserver);
    }

    const SdfPath       _sceneIndexPathPrefix;
    const Observer::Ptr _appSceneObserver;
    Ufe::Path           _sceneIndexAppPath;
};

constexpr char kMayaUsdProxyShapeNode[] = { "mayaUsdProxyShape" };
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
        auto proxyShapeSceneIndex = TfDynamic_cast<MayaUsdProxyShapeSceneIndexRefPtr>(pluginSceneIndex);
        proxyShapeSceneIndex->UpdateTime();
    }
};

// MayaHydraSceneIndexRegistration is used to register a scene index for
// mayaUsdPlugin proxy shape nodes.
MayaHydraSceneIndexRegistry::MayaHydraSceneIndexRegistry(const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy)
    : _renderIndexProxy(renderIndexProxy)
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
    MDGMessage::removeCallbacks(_DGCallbackIds);
    _DGCallbackIds.clear();
    if (_AfterOpenCBId){
        MSceneMessage::removeCallback(_AfterOpenCBId);
    }
    _AfterOpenCBId = 0;
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
#ifdef CODE_COVERAGE_WORKAROUND
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
    MDagPath dagPath(MDagPath::getAPathTo(dagNode, &status));
    if (!TF_VERIFY(status == MS::kSuccess, "Unable to find Dag path to given node")) {
        return;
    }

    registration->dagNode = MObjectHandle(dagNode);
    registration->sceneIndexPathPrefix = sceneIndexPathPrefix(
        _renderIndexProxy->GetMergingSceneIndex(), dagNode);
                                                                  
#ifdef MAYAHYDRALIB_MAYAUSDAPI_ENABLED

    //We receive only dag nodes of type MayaUsdProxyShapeNode
    MAYAUSDAPI_NS::ProxyStage proxyStage(dagNode);

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
            
    //We will get the following scene indices from Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex
    HdSceneIndexBaseRefPtr finalSceneIndex = nullptr;
    UsdImagingStageSceneIndexRefPtr stageSceneIndex = nullptr;

    // We are explicitly adding a prefixing scene index just downstream (after)
    // the MayaUsdProxyShapeSceneIndex.  We don't want to automatically add an
    // additional prefixing scene index to the PathInterfaceSceneIndex (which
    // is downstream of the prefixing stream index), which would double the
    // prefix.  Therefore, set the scene index prefix here to be 'null' (the
    // absolute root path).
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
        Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageSceneIndex(createInfo, finalSceneIndex, stageSceneIndex, 
                                                                             SdfPath::AbsoluteRootPath(), (void*)&dagNode);
    if (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex){
        TF_CODING_ERROR("Error (nullptr == dataProducerSceneIndexData || nullptr == finalSceneIndex || nullptr == stageSceneIndex) !");
    }
        
    // Create Maya USD proxy shape scene index.  Since this scene index
    // contains Maya data, it cannot be added by the Flow Viewport API.
    // Pass in the scene index prefix for the proxy shape scene index, so it
    // can register a pick handler.
    auto mayaUsdProxyShapeSceneIndex = MAYAHYDRA_NS_DEF::MayaUsdProxyShapeSceneIndex::New(proxyStage, finalSceneIndex, stageSceneIndex, MObjectHandle(dagNode), registration->sceneIndexPathPrefix);
    registration->pluginSceneIndex = mayaUsdProxyShapeSceneIndex;
    registration->interpretRprimPathFn = &(MAYAHYDRA_NS_DEF::MayaUsdProxyShapeSceneIndex::InterpretRprimPath);
    mayaUsdProxyShapeSceneIndex->Populate();

    // This sets the required prefix just downstream (after) the
    // MayaUsdProxyShapeSceneIndex, as required.
    auto pfsi = HdPrefixingSceneIndex::New(
        registration->pluginSceneIndex,
        registration->sceneIndexPathPrefix);

    // Add the PathInterfaceSceneIndex which must be the last scene index, it 
    // is used by selection highlighting.  The scene index prefix is passed in
    // not to add in a prefix, which is done explicitly by the prefixing scene
    // index above.  Rather, it is so the path interface scene index can build
    // the scene index path from an application path.
    registration->rootSceneIndex = PathInterfaceSceneIndex::New(
        pfsi,
        registration->sceneIndexPathPrefix,
        Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath)));

    //Set the chain back into the dataProducerSceneIndexData in both members
    dataProducerSceneIndexData->SetDataProducerSceneIndex(registration->rootSceneIndex);
    dataProducerSceneIndexData->SetDataProducerLastSceneIndexChain(registration->rootSceneIndex);

    //Add this chain scene index to the render index proxy from all viewports
    const bool bRes = Fvp::DataProducerSceneIndexInterfaceImp::get().addUsdStageDataProducerSceneIndexDataBaseToAllViewports(dataProducerSceneIndexData);
    if (false == bRes){
        TF_CODING_ERROR("Fvp::DataProducerSceneIndexInterfaceImp::get().addDataProducerSceneIndex returned false !");
    }

#else

    static HdSceneIndexPluginRegistry& sceneIndexPluginRegistry
        = HdSceneIndexPluginRegistry::GetInstance();
    if (!sceneIndexPluginRegistry.IsRegisteredPlugin(sceneIndexPluginId)) {
        return;
    }

    using MObjectDataSource = HdRetainedTypedSampledDataSource<MObject>;
    using MayaHydraVersionDataSource = HdRetainedTypedSampledDataSource<int>;
    // Functions retrieved from the scene index plugin
    using InterpretRprimPathDataSource
        = HdRetainedTypedSampledDataSource<MayaHydraInterpretRprimPath&>;

    // Create the registration record which is then added into the registry
    // if everything succeeds.
    static TfToken sDataSourceEntryNames[] {
        TfToken("object"), TfToken("version"), TfToken("interpretRprimPath")
    };
    constexpr int  kDataSourceNumEntries = sizeof(sDataSourceEntryNames) / sizeof(TfToken);

    HdDataSourceBaseHandle             values[] {
        MObjectDataSource::New(dagNode),
        MayaHydraVersionDataSource::New(MAYAHYDRA_API_VERSION),
        InterpretRprimPathDataSource::New(registration->interpretRprimPathFn)
    };
    static_assert(
        sizeof(values) / sizeof(HdDataSourceBaseHandle) == kDataSourceNumEntries,
        "Incorrect number of data source entries");
    registration->pluginSceneIndex = sceneIndexPluginRegistry.AppendSceneIndex(
        sceneIndexPluginId,
        nullptr,
        HdRetainedContainerDataSource::New(kDataSourceNumEntries, sDataSourceEntryNames, values));

    if (!TF_VERIFY(
            registration->pluginSceneIndex,
            "MayaHydraSceneIndexRegistry::_AddSceneIndexForNode failed to create %s scene index from given "
            "node type.",
            sceneIndexPluginName.c_str())) {
        return;
    }

    // Because the path interface scene index must be the last one
    // in the chain, add the prefixing scene index here, instead of
    // relying on the render index proxy doing it for us.
    auto pfsi = HdPrefixingSceneIndex::New(
        registration->pluginSceneIndex,
        registration->sceneIndexPathPrefix);

    registration->rootSceneIndex = PathInterfaceSceneIndex::New(
        pfsi,
        registration->sceneIndexPathPrefix,
        Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath))
    );

    constexpr bool needsPrefixing = false;
    _renderIndexProxy->InsertSceneIndex(
        registration->rootSceneIndex, 
        registration->sceneIndexPathPrefix, needsPrefixing);
    static SdfPath maya126790Workaround("maya126790Workaround");
    registration->pluginSceneIndex->GetPrim(maya126790Workaround);
#endif //MAYAHYDRALIB_MAYAUSDAPI_ENABLED

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
}

PXR_NAMESPACE_CLOSE_SCOPE
