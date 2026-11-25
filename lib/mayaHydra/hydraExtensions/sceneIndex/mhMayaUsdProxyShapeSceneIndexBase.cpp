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
#include <flowViewport/selection/fvpPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

//mayaHydra headers
#include "ufeExtensions/Global.h"

#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>

#include <ufe/scene.h>
#include <ufe/sceneNotification.h>

#include <algorithm>
#include <memory>
#include <unordered_map>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const std::string digits = "0123456789";

SdfPath append(const SdfPath& prefix, const SdfPath& src)
{
    return prefix.AppendPath(src.MakeRelativePath(SdfPath::AbsoluteRootPath()));
}

Fvp::PrimSelections addPrefix(const SdfPath& sceneIndexPathPrefix, const Fvp::PrimSelections& src)
{
    // Copy the source into the destination, and patch up the destination.
    Fvp::PrimSelections dst { src };

    for (auto& d : dst) {
        d.primPath = append(sceneIndexPathPrefix, d.primPath);

        for (auto& dis : d.nestedInstanceIndices) {
            dis.instancerPath = append(sceneIndexPathPrefix, dis.instancerPath);
        }
    }
    return dst;
}

// Maps the UFE paths of one mayaUsdProxyShape node through one of the node's
// MayaUsdProxyShapeSceneIndexBase instances. Each scene index registry (one per render override,
// one per batch render) creates its own instance for the node. All of them image the same stage
// at the node's prefix (MayaHydraNodePrefixReservation), and are in every view, so any of them
// gives the same Hydra paths. Not a plain Fvp::PrefixPathMapper: native instances are resolved
// through the prims of the instance.
class UsdPathMapper : public Fvp::PathMapper
{
public:
    using SceneIndexWeakPtr = TfWeakPtr<MayaHydra::MayaUsdProxyShapeSceneIndexBase>;

    void Attach(const SceneIndexWeakPtr& sceneIndex) { _sceneIndices.push_back(sceneIndex); }

    void Detach(const MayaHydra::MayaUsdProxyShapeSceneIndexBase* sceneIndex)
    {
        _sceneIndices.erase(
            std::remove_if(
                _sceneIndices.begin(),
                _sceneIndices.end(),
                [sceneIndex](const SceneIndexWeakPtr& attached) {
                    return !attached || get_pointer(attached) == sceneIndex;
                }),
            _sceneIndices.end());
    }

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override
    {
        const auto sceneIndex = _GetSceneIndex();
        return sceneIndex ? sceneIndex->UfePathToPrimSelections(appPath) : Fvp::PrimSelections {};
    }

    std::string Name() const override { return "UsdPathMapper"; }

private:
    // The newest instance not waiting for a rebuild (NeedsRebuild()): one waiting maps against
    // the previous stage, while a rebuilt one maps against the current one. If all of them wait,
    // the newest, whose data producer still draws the previous stage until its rebuild.
    SceneIndexWeakPtr _GetSceneIndex() const
    {
        SceneIndexWeakPtr newest;
        for (auto it = _sceneIndices.rbegin(); it != _sceneIndices.rend(); ++it) {
            if (!*it) {
                continue;
            }
            if (!(*it)->NeedsRebuild()) {
                return *it;
            }
            if (!newest) {
                newest = *it;
            }
        }
        return newest;
    }

    // The node's MayaUsdProxyShapeSceneIndexBase instances, oldest first. Weak: the instances
    // detach themselves in _Destroy().
    std::vector<SceneIndexWeakPtr> _sceneIndices;
};

struct HashObjectHandle
{
    size_t operator()(const MObjectHandle& handle) const { return handle.hashCode(); }
};

} // namespace

namespace MAYAHYDRA_NS_DEF {

// The path mapper registration of one mayaUsdProxyShape node, shared by the node's
// MayaUsdProxyShapeSceneIndexBase instances, as the path mapper registry takes one mapper per UFE
// path. The node's first instance registers the mapper, and the last one to release the
// registration unregisters it, as for PickRegistration. Main thread only.
class MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration
{
public:
    using Ptr = std::shared_ptr<_PathMapperRegistration>;

    // The registration for the node held by another of its instances, or a new one.
    static Ptr Acquire(const MObjectHandle& node, const Ufe::Path& appPath);

    _PathMapperRegistration(const MObjectHandle& node, const Ufe::Path& appPath)
        : _node(node)
        , _appPath(appPath)
        , _mapper(std::make_shared<UsdPathMapper>())
        // False if another client maps the UFE path: then there is nothing of ours to move or
        // unregister.
        , _registered(Fvp::PathMapperRegistry::Instance().Register(appPath, _mapper))
    {
    }

    ~_PathMapperRegistration();

    _PathMapperRegistration(const _PathMapperRegistration&) = delete;
    _PathMapperRegistration& operator=(const _PathMapperRegistration&) = delete;

    void Attach(const UsdPathMapper::SceneIndexWeakPtr& sceneIndex) { _mapper->Attach(sceneIndex); }
    void Detach(const MayaUsdProxyShapeSceneIndexBase* sceneIndex) { _mapper->Detach(sceneIndex); }

    // Every instance for the node observes a rename of the node and reports it: the first
    // report moves the registration, the others find it moved.
    void Rename(const Ufe::Path& oldPath, const Ufe::Path& newPath)
    {
        if (_appPath != oldPath) {
            return;
        }
        _appPath = newPath;
        if (_registered) {
            TF_AXIOM(Fvp::PathMapperRegistry::Instance().Update(oldPath, newPath));
        }
    }

private:
    // Not owning: the instances own the registrations.
    using Registrations = std::
        unordered_map<MObjectHandle, std::weak_ptr<_PathMapperRegistration>, HashObjectHandle>;
    static Registrations _registrations;

    const MObjectHandle                  _node;
    Ufe::Path                            _appPath;
    const std::shared_ptr<UsdPathMapper> _mapper;
    const bool                           _registered;
};

MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration::Registrations
    MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration::_registrations;

MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration::Ptr
MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration::Acquire(
    const MObjectHandle& node,
    const Ufe::Path&     appPath)
{
    auto found = _registrations.find(node);
    if (found != _registrations.end()) {
        if (auto registration = found->second.lock()) {
            TF_VERIFY(
                registration->_appPath == appPath,
                "Proxy shape path %s differs from the registered %s.",
                appPath.string().c_str(),
                registration->_appPath.string().c_str());
            return registration;
        }
    }

    auto registration = std::make_shared<_PathMapperRegistration>(node, appPath);
    _registrations[node] = registration;
    return registration;
}

MayaUsdProxyShapeSceneIndexBase::_PathMapperRegistration::~_PathMapperRegistration()
{
    if (_registered) {
        Fvp::PathMapperRegistry::Instance().Unregister(_appPath);
    }

    // The entry may already point to a replacement registration for the node.
    auto found = _registrations.find(_node);
    if (found != _registrations.end() && found->second.expired()) {
        _registrations.erase(found);
    }
}

} // namespace MAYAHYDRA_NS_DEF

namespace {

using namespace Ufe;
using namespace MayaHydra;

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
            for (const auto& op : compNotification) {
                handleOp(op);
            }
        } else {
            handleOp(sceneChanged);
        }
    }
};

class UsdPathMapperSceneObserver : public SceneObserver
{
public:
    UsdPathMapperSceneObserver(MayaUsdProxyShapeSceneIndexBase& psSi)
        : SceneObserver()
        , _psSi(psSi)
    {
    }

private:
    // If the proxy shape's app path changes, update the app path key in the
    // path mapper registry.
    void handleOp(const SceneCompositeNotification::Op& op) override
    {
        if (op.opType == SceneChanged::ObjectPathChange
            && ((op.subOpType == ObjectPathChange::ObjectReparent)
                || (op.subOpType == ObjectPathChange::ObjectRename))) {
            const auto& siPath = _psSi.GetSceneIndexAppPath();
            if (siPath.startsWith(op.path)) {
                const auto oldPath = siPath;
                auto       newPath = oldPath.reparent(op.path, op.item->path());
                // Also moves the node's entry in the path mapper registry.
                _psSi.SetSceneIndexAppPath(newPath);
            }
        }
    }

    MayaUsdProxyShapeSceneIndexBase& _psSi;
};

} // namespace

namespace MAYAHYDRA_NS_DEF {

long int MayaUsdProxyShapeSceneIndexBase::_nbPopulateCalls = 0;

MayaUsdProxyShapeSceneIndexBase::MayaUsdProxyShapeSceneIndexBase(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath
)
    : ParentClass(sceneIndexChainLastElement)
    , InputSceneIndexUtils(sceneIndexChainLastElement)
    , _sceneIndexPathPrefix(sceneIndexPathPrefix)
    , _usdImagingStageSceneIndex(usdImagingStageSceneIndex)
    , _proxyStage(proxyStage)
    , _dagNodeHandle(dagNodeHandle)
    , _sceneIndexAppPath(sceneIndexAppPath)
    , _appSceneObserver(std::make_shared<UsdPathMapperSceneObserver>(*this))
{
    // The gateway node (proxy shape) is a Maya node, so the scene index
    // path must be a single segment.
    TF_AXIOM(sceneIndexAppPath.nbSegments() == 1);

    TfWeakPtr<MayaUsdProxyShapeSceneIndexBase> ptr(this);
    _stageSetNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_StageSet);
    _stageInvalidateNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_StageInvalidate);
    _objectsChangedNoticeKey = TfNotice::Register(ptr, &MayaUsdProxyShapeSceneIndexBase::_ObjectsChanged);

    // Observe the scene to be informed of path changes to the gateway node
    // (proxy shape) that corresponds to our scene index data producer.
    Scene::instance().addObserver(_appSceneObserver);

    // Map the node's UFE paths, through the path mapper shared with the node's instances in the
    // other scene index registries, if any.
    _pathMapperRegistration = _PathMapperRegistration::Acquire(dagNodeHandle, _sceneIndexAppPath);
    _pathMapperRegistration->Attach(ptr);

    Fvp::Instruments::instance().set(kNbPopulateCalls, VtValue(_nbPopulateCalls));
}

MayaUsdProxyShapeSceneIndexBase::~MayaUsdProxyShapeSceneIndexBase()
{
    _Destroy();                 // In a destructor calls are not virtual
}

void MayaUsdProxyShapeSceneIndexBase::SetSceneIndexAppPath(const Ufe::Path& sceneIndexAppPath)
{
    const Ufe::Path oldPath = _sceneIndexAppPath;
    _sceneIndexAppPath = sceneIndexAppPath;

    if (_pathMapperRegistration) {
        _pathMapperRegistration->Rename(oldPath, sceneIndexAppPath);
    }
}

void MayaUsdProxyShapeSceneIndexBase::_Destroy()
{
    // Detach() and the destructor both call this.
    if (_destroyed) {
        return;
    }
    _destroyed = true;

    // The path mapper stops mapping through this instance, and is unregistered if no other
    // instance for the node uses it.
    if (_pathMapperRegistration) {
        _pathMapperRegistration->Detach(this);
        _pathMapperRegistration.reset();
    }

    // Ufe::Subject has automatic cleanup of stale observers, but this can
    // be problematic on application exit if the library of the observer is
    // cleaned up before that of the subject, so simply stop observing.
    Scene::instance().removeObserver(_appSceneObserver);

    TfNotice::Revoke(_stageSetNoticeKey);
    TfNotice::Revoke(_stageInvalidateNoticeKey);
    TfNotice::Revoke(_objectsChangedNoticeKey);
}

MayaUsdProxyShapeSceneIndexBaseRefPtr MayaUsdProxyShapeSceneIndexBase::New(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath
)
{
    return TfCreateRefPtr(new MayaUsdProxyShapeSceneIndexBase(
        proxyStage, sceneIndexChainLastElement, usdImagingStageSceneIndex, dagNodeHandle,
        sceneIndexPathPrefix, sceneIndexAppPath));
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

    if (_rebuildOnStageChange && _stage) {
        // No SetStage() once populated (see SetRebuildOnStageChange()): the owner replaces
        // this scene index at the next render.
        _needsRebuild = true;
        return;
    }

    _usdImagingStageSceneIndex->SetStage(nullptr);
    _stage = nullptr;
    _InvalidateMayHaveUnloadedPrims();
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
    if (_needsRebuild) {
        // Populate() can set it too. The replacement is built from the current stage, edits
        // included.
        return;
    }
    _usdImagingStageSceneIndex->ApplyPendingUpdates();
}

void MayaUsdProxyShapeSceneIndexBase::Populate()
{
    if (_needsRebuild) {
        return;
    }
    if (!_populated) {
        auto stage = _proxyStage.getUsdStage();
        if (_rebuildOnStageChange && _stage && _stage != stage) {
            // A new stage after the first population, even an empty or null one: see
            // SetRebuildOnStageChange().
            _needsRebuild = true;
            return;
        }
        // Check whether the pseudo-root has children
        if (HasRootPrims(stage)) {
            ++_nbPopulateCalls;
            Fvp::Instruments::instance().set(kNbPopulateCalls, VtValue(_nbPopulateCalls));
            // Set _stage before calling SetStage(), as the latter's PrimsAdded handlers call MayHaveUnloadedPrims().
            _stage = stage;
            _InvalidateMayHaveUnloadedPrims();
            _usdImagingStageSceneIndex->SetStage(stage);
            // Set the initial time
            UpdateTime();
            _populated = true;
        }
    }
}

bool MayaUsdProxyShapeSceneIndexBase::HasRootPrims(const UsdStageRefPtr& stage)
{
    // Unlike GetChildren(), which uses UsdPrimDefaultPredicate, don't skip unloaded prims.
    return stage
        && !stage->GetPseudoRoot()
                .GetFilteredChildren(UsdPrimIsActive && UsdPrimIsDefined && !UsdPrimIsAbstract)
                .empty();
}

bool MayaUsdProxyShapeSceneIndexBase::MayHaveUnloadedPrims() const
{
    // UsdPrim::IsLoaded follows the load rules, so a stage whose rules load the pseudo-root with
    // all its descendants has no unloaded prims.
    if (!_stage) {
        return false;
    }
    if (!_mayHaveUnloadedPrimsCache.has_value()) {
        _mayHaveUnloadedPrimsCache
            = !_stage->GetLoadRules().IsLoadedWithAllDescendants(SdfPath::AbsoluteRootPath());
    }
    return _mayHaveUnloadedPrimsCache.value();
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
    //We need to re-populate to see the changes, or be replaced (NeedsRebuild())
    return (false == _populated) || _needsRebuild;
}

Fvp::PrimSelections
MayaUsdProxyShapeSceneIndexBase::UfePathToPrimSelections(const Ufe::Path& appPath) const
{
    // If the data model object application path does not match the path we
    // translate, return an empty path.
    if (!appPath.startsWith(_sceneIndexAppPath)) {
        return {};
    }

    // If the application path is our prefix, just return the
    // corresponding scene index path.
    if (appPath == _sceneIndexAppPath) {
        return Fvp::PrimSelections { Fvp::PrimSelection { _sceneIndexPathPrefix } };
    }

    // The scene index path is composed of 2 parts, in order:
    // 1) The scene index path prefix, which is fixed on construction.
    // 2) The second segment of the UFE path, with each UFE path component
    //    becoming an SdfPath component. If the last component is a number,
    //    then we are dealing with an instance selection.
    TF_AXIOM(appPath.nbSegments() == 2);
    SdfPath primPath = SdfPath::AbsoluteRootPath();
    // One entry per instancing level crossed by the path, outermost first, as in
    // HdSelectionSchema::nestedInstanceIndices. A point instance under a native instance, or a
    // native instance inside the prototype of another, crosses two levels.
    std::vector<Fvp::InstancesSelection> instanceSelections;

    auto       secondSegment = appPath.getSegments()[1];
    const auto lastComponentString = secondSegment.components().back().string();
    const bool lastComponentIsNumeric
        = lastComponentString.find_first_not_of(digits) == std::string::npos;
    const size_t lastComponentIndex = secondSegment.size() - 1;

    for (size_t iComponent = 0; iComponent < secondSegment.size(); iComponent++) {
        // Native instancing : if the current prim path points to a native instance, repath to the
        // prototype before appending the following UFE components
        HdSceneIndexPrim prim = GetPrim(primPath);
        HdInstanceSchema instanceSchema = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instanceSchema.IsDefined()) {
            auto             instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);
            HdSceneIndexPrim instancerPrim = GetPrim(instancerPath);
            HdInstancerTopologySchema instancerTopologySchema
                = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
            auto prototypes = instancerTopologySchema.GetPrototypes()->GetTypedValue(0);
            auto prototypeIndex = instanceSchema.GetPrototypeIndex()->GetTypedValue(0);
            primPath = prototypes[prototypeIndex];
            instanceSelections.push_back(
                { instancerPath,
                  prototypeIndex,
                  { instanceSchema.GetInstanceIndex()->GetTypedValue(0) } });
        }

        // SdfPath components cannot be numeric.  This happens with point instance selections.
        auto targetChildPath = ((iComponent == lastComponentIndex) && lastComponentIsNumeric)
            ? SdfPath()
            : primPath.AppendChild(TfToken(secondSegment.components()[iComponent].string()));
        auto actualChildPaths = GetChildPrimPaths(primPath);
        if (!targetChildPath.IsEmpty()
            && std::find(actualChildPaths.begin(), actualChildPaths.end(), targetChildPath)
                != actualChildPaths.end()) {
            // Append if the new path is valid
            primPath = targetChildPath;
        } else if (iComponent == lastComponentIndex) {
            // If the last component is a number, we are dealing with an instance selection.
            // But there are other cases like when you assign a USD Preview surface material to a
            // usd prim, it has a shader prim in the material which doesn't appear in the hydra
            // hierarchy but is actually present and we end up in this case as well.
            if (lastComponentIsNumeric) {
                // Point instancing : instance selection. The path should end with a number
                // corresponding to the selected instance,
                // and the remainder of the path points to the point instancer.
                HdSceneIndexPrim          instancerPrim = GetPrim(primPath);
                HdInstancerTopologySchema instancerTopologySchema
                    = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
                auto instanceIndicesByPrototype = instancerTopologySchema.GetInstanceIndices();
                for (int iInstanceIndices = 0; static_cast<size_t>(iInstanceIndices)
                     < instanceIndicesByPrototype.GetNumElements();
                     iInstanceIndices++) {
                    auto instanceIndices
                        = instanceIndicesByPrototype.GetElement(iInstanceIndices)->GetTypedValue(0);
                    if (std::find(
                            instanceIndices.begin(),
                            instanceIndices.end(),
                            std::stoi(lastComponentString))
                        != instanceIndices.end()) {
                        instanceSelections.push_back(
                            { primPath, iInstanceIndices, { std::stoi(lastComponentString) } });
                        break;
                    }
                }
            }
        } else {
            // There is no prim corresponding to the converted path
            TF_WARN("Could not convert UFE path %s to Hydra prims.", appPath.string().data());
            return {};
        }
    }

    Fvp::PrimSelections primSelections({ Fvp::PrimSelection { primPath, instanceSelections } });

    // Point instancing : propagate selection to propagated prototypes
    // A point instancer rooted under another point instancer (a nested instancer) is made
    // unrenderable at its original path by UsdImaging (its prim type is emptied), and is only
    // drawn through its propagated copies.
    const bool selectedPrimIsDrawnInstancer
        = GetPrim(primPath).primType == HdPrimTypeTokens->instancer;
    auto ancestorsRange = primPath.GetAncestorsRange();
    for (const auto& ancestorPath : ancestorsRange) {
        HdSceneIndexPrim            currPrim = GetPrim(ancestorPath);
        UsdImagingUsdPrimInfoSchema usdPrimInfo
            = UsdImagingUsdPrimInfoSchema::GetFromParent(currPrim.dataSource);
        if (!usdPrimInfo.IsDefined()) {
            continue;
        }
        auto propagatedProtosDataSource = usdPrimInfo.GetPiPropagatedPrototypes();
        if (!propagatedProtosDataSource) {
            continue;
        }
        auto propagatedProtoNames = propagatedProtosDataSource->GetNames();
        for (const auto& propagatedProtoName : propagatedProtoNames) {
            auto propagatedProtoPathDataSource = HdTypedSampledDataSource<SdfPath>::Cast(
                propagatedProtosDataSource->Get(propagatedProtoName));
            if (propagatedProtoPathDataSource) {
                SdfPath propagatedProtoPath = propagatedProtoPathDataSource->GetTypedValue(0);
                SdfPath propagatedPrimPath
                    = primPath.ReplacePrefix(ancestorPath, propagatedProtoPath);
                HdSceneIndexPrim propagatedPrim = GetPrim(propagatedPrimPath);
                // This check controls which types of prims have their selection data source
                // propagated. We skip propagated instancers when the selected prim is itself a
                // drawn instancer, so that selecting an instancer A that is both drawing geometry
                // but also prototyped and propagated for another instancer B will only mark the
                // geometry-drawing instancer A as selected. A nested instancer does not draw at
                // its original path, so its propagated copies must be marked as selected,
                // otherwise nothing gets highlighted (HYDRA-2588).
                // For now, this only affects selection highlighting.
                if (propagatedPrim.primType != HdPrimTypeTokens->instancer
                    || !selectedPrimIsDrawnInstancer) {
                    // The instance indices of an instancer under the propagated ancestor belong
                    // to its propagated copy, which is the one drawing them: repath it like the
                    // prim. Instancers above the ancestor are left unchanged by ReplacePrefix.
                    auto propagatedInstanceIndices = primSelections.front().nestedInstanceIndices;
                    for (auto& instancesSelection : propagatedInstanceIndices) {
                        instancesSelection.instancerPath
                            = instancesSelection.instancerPath.ReplacePrefix(
                                ancestorPath, propagatedProtoPath);
                    }
                    primSelections.push_back({ propagatedPrimPath, propagatedInstanceIndices });
                    // UsdImaging inserts the prototype copies drawn by a propagated instancer
                    // under the original prototype paths, so the rprims it draws are under the
                    // original selected path, not under the propagated one. Also select them
                    // through the propagated instancer, which is in their instancer chain, so
                    // that the outline keeps the selected instances.
                    if (propagatedInstanceIndices
                        != primSelections.front().nestedInstanceIndices) {
                        primSelections.push_back({ primPath, propagatedInstanceIndices });
                    }
                }
            }
        }
        break; // We found propagated prototypes, exit now to avoid propagating selection to
               // prototypes of other parents
    }

    // Now have primSelections in the namespace of this scene index.  Need to
    // account for the prefix, which is added downstream of this scene index.
    return addPrefix(_sceneIndexPathPrefix, primSelections);
}

} // namespace MAYAHYDRA_NS_DEF
