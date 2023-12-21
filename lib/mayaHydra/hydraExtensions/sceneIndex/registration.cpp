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

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/sceneIndex/registration.h>

#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/sceneIndex/fvpPathInterfaceSceneIndex.h>

#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MItDag.h>
#include <maya/MMessage.h>
#include <maya/MNodeMessage.h>

#include <ufeExtensions/Global.h>

#include <ufe/observer.h>
#include <ufe/sceneNotification.h>
#include <ufe/scene.h>

namespace {

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

    SdfPath SceneIndexPath(const Ufe::Path& appPath) const override
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

}

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

/* To add a custom scene index, a customer plugin must:
 1. Define a Maya dag node via the MPxNode interface, and register it MFnPlugin::registerNode. This
is typically done inside a Maya plug-in initialize function.
 2. Define a HdSceneIndexPlugin which contains an _AppendSceneIndex method. The _AppendSceneIndex
method will be called for every Maya node added into the scene. A customer is responsible for type
checking the node for the one defined and also instantiate the corresponding scene index inside
_AppendSceneIndex. The scene index returned by _AppendSceneIndex is then added to the render index
by Maya.

For example here is how we do that process in the Maya USD plug-in :
// Some context :
// MayaUsdProxyShapeMayaNodeSceneIndexPlugin is a subclass of HdSceneIndexPlugin.
// MayaUsdProxyShapeBase is the base class for our Maya USD node.
// MayaUsd::MayaUsdProxyShapeSceneIndex is the scene index class for Maya USD where we load the
// stages.

HdSceneIndexBaseRefPtr MayaUsdProxyShapeMayaNodeSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr&      inputScene,
    const HdContainerDataSourceHandle& inputArgs)
{
    using HdMObjectDataSource = HdRetainedTypedSampledDataSource<MObject>;
    using HdMObjectDataSourceHandle = HdRetainedTypedSampledDataSource<MObject>::Handle;
    static TfToken         dataSourceNodePathEntry("object");
    HdDataSourceBaseHandle dataSourceEntryPathHandle = inputArgs->Get(dataSourceNodePathEntry);
    if (HdMObjectDataSourceHandle dataSourceEntryPath
        = HdMObjectDataSource::Cast(dataSourceEntryPathHandle)) {
        MObject           dagNode = dataSourceEntryPath->GetTypedValue(0.0f);
        MStatus           status;
        MFnDependencyNode dependNodeFn(dagNode, &status);
        if (!TF_VERIFY(status, "Error getting MFnDependencyNode")) {
            return nullptr;
        }

        // Check that this node is a MayaUsdProxyShapeBase which is the base class for our Maya USD
        // node
        auto proxyShape = dynamic_cast<MayaUsdProxyShapeBase*>(dependNodeFn.userNode());
        if (TF_VERIFY(proxyShape, "Error getting MayaUsdProxyShapeBase")) {
            auto psSceneIndex = MayaUsd::MayaUsdProxyShapeSceneIndex::New(proxyShape);
            // Flatten transforms, visibility, purpose, model, and material
            // bindings over hierarchies.
            return HdFlatteningSceneIndex::New(psSceneIndex);
        }
    }

    return nullptr;
}

You may want to see the full source code in the Maya USD open source repository :
https://github.com/Autodesk/maya-usd/tree/dev/lib/mayaUsd/sceneIndex
*/

namespace {
constexpr char kDagNodeMessageName[] = { "dagNode" };
}
// MayaHydraSceneIndexRegistration is used to register a scene index for a given dag node type.
MayaHydraSceneIndexRegistry::MayaHydraSceneIndexRegistry(const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy)
    : _renderIndexProxy(renderIndexProxy)
{
    // Begin registering of custom scene indices for given node types
    HdSceneIndexPluginRegistry& sceneIndexPluginRegistry
        = HdSceneIndexPluginRegistry::GetInstance();
    // Ensure scene index plugin registration
    std::vector<HfPluginDesc> customSceneIndexDescs;
    sceneIndexPluginRegistry.GetPluginDescs(&customSceneIndexDescs);
    if (customSceneIndexDescs.size() != 0) {
        MCallbackId id;
        MStatus     status;
        id = MDGMessage::addNodeAddedCallback(
            _SceneIndexNodeAddedCallback, kDagNodeMessageName, this, &status);
        if (TF_VERIFY(status == MS::kSuccess, "NodeAdded callback registration failed."))
            _sceneIndexDagNodeMessageCallbacks.append(id);
        id = MDGMessage::addNodeRemovedCallback(
            _SceneIndexNodeRemovedCallback, kDagNodeMessageName, this, &status);
        if (TF_VERIFY(status == MS::kSuccess, "NodeRemoved callback registration failed."))
            _sceneIndexDagNodeMessageCallbacks.append(id);

        // Iterate over scene to find out existing node which will miss eventual dagNode added
        // callbacks
        MItDag nodesDagIt(MItDag::kDepthFirst, MFn::kInvalid);
        for (; !nodesDagIt.isDone(); nodesDagIt.next()) {
            MStatus status;
            MObject dagNode(nodesDagIt.item(&status));
            if (TF_VERIFY(status == MS::kSuccess)) {
                _AddSceneIndexForNode(dagNode);
            }
        }
    }
}

// Retrieve information relevant to registration such as UFE compatibility of a particular scene
// index
MayaHydraSceneIndexRegistrationPtr
MayaHydraSceneIndexRegistry::GetSceneIndexRegistrationForRprim(const SdfPath& rprimPath) const
{
    // Retrieve the rprimPath prefix. Composed of the scene index plugin path, and a name used as a
    // disambiguator. MAYA-128179: Revisit this operation. SdfPath operation is slow. No way to get
    // first component only
    SdfPath sceneIndexPluginPath = rprimPath.GetParentPath();
    while (sceneIndexPluginPath.GetPathElementCount() > 2)
        sceneIndexPluginPath = sceneIndexPluginPath.GetParentPath();
    auto it = _registrations.find(sceneIndexPluginPath);
    if (it != _registrations.end()) {
        return it->second;
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
    auto          it = _registrationsByObjectHandle.find(dagNodeHandle);
    if (it != _registrationsByObjectHandle.end()) {
        MayaHydraSceneIndexRegistrationPtr registration(it->second);
        _renderIndexProxy->RemoveSceneIndex(registration->rootSceneIndex);
        _registrationsByObjectHandle.erase(dagNodeHandle);
        _registrations.erase(registration->sceneIndexPathPrefix);
        return true;
    }
    return false;
}

constexpr char kSceneIndexPluginSuffix[] = {
    "MayaNodeSceneIndexPlugin"
}; // every scene index plugin compatible with the hydra viewport requires this suffix

void MayaHydraSceneIndexRegistry::_AddSceneIndexForNode(MObject& dagNode)
{
    MFnDependencyNode dependNodeFn(dagNode);
    // To match plugin TfType registration, name must begin with upper case.
    const std::string sceneIndexPluginName([&](){
            std::string name = dependNodeFn.typeName().asChar();
            name[0] = toupper(name[0]);
            name += kSceneIndexPluginSuffix;
            return name;}());
    const TfToken sceneIndexPluginId(sceneIndexPluginName);

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
    MayaHydraSceneIndexRegistrationPtr registration(new MayaHydraSceneIndexRegistration());
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
    if (TF_VERIFY(
            registration->pluginSceneIndex,
            "HdSceneIndexBase::AppendSceneIndex failed to create %s scene index from given "
            "node type.",
            sceneIndexPluginName.c_str())) {

        MStatus  status;
        MDagPath dagPath(MDagPath::getAPathTo(dagNode, &status));
        if (TF_VERIFY(status == MS::kSuccess, "Unable of finding Dag path to given node")) {
            registration->dagNode = MObjectHandle(dagNode);

            // Create a unique scene index path prefix by starting with the
            // Dag node name, and checking for uniqueness under the scene
            // index plugin parent rprim.  If not unique, add an
            // incrementing numerical suffix until it is.
            const auto sceneIndexPluginPath = SdfPath::AbsoluteRootPath().AppendChild(sceneIndexPluginId);
            const auto newName = uniqueChildName(
                _renderIndexProxy->GetMergingSceneIndex(),
                sceneIndexPluginPath,
                SanitizeNameForSdfPath(dependNodeFn.name().asChar())
            );

            registration->sceneIndexPathPrefix = sceneIndexPluginPath.AppendChild(newName);

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

            // Add registration record if everything succeeded
            _registrations.insert({ registration->sceneIndexPathPrefix, registration });
            _registrationsByObjectHandle.insert({ registration->dagNode, registration });
        }
    }
}

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
