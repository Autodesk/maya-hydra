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

#include <optional>

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

class UsdPathMapper : public Fvp::PathMapper
{
public:
    UsdPathMapper(const MayaHydra::MayaUsdProxyShapeSceneIndexBase& psSi)
        : _psSi(psSi)
    {
    }

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override
    {
        return _psSi.UfePathToPrimSelections(appPath);
    }

    std::string Name() const override { return "UsdPathMapper"; }

private:
    // Non-owning reference to prevent ownership cycle.
    const MayaHydra::MayaUsdProxyShapeSceneIndexBase& _psSi;
};

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
                _psSi.SetSceneIndexAppPath(newPath);

                // Update our entry in the path mapper registry.
                TF_AXIOM(Fvp::PathMapperRegistry::Instance().Update(oldPath, newPath));
            }
        }
    }

    MayaUsdProxyShapeSceneIndexBase& _psSi;
};

} // namespace

namespace MAYAHYDRA_NS_DEF {

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
    , _usdPathMapper(std::make_shared<UsdPathMapper>(*this))
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

    // Register a mapper in the path mapper registry, if there is none
    // for this path.
    _unregisterPathMapper = Fvp::PathMapperRegistry::Instance().Register(
        _sceneIndexAppPath, _usdPathMapper);

    Fvp::Instruments::instance().set(kNbPopulateCalls, VtValue(_nbPopulateCalls));
}

MayaUsdProxyShapeSceneIndexBase::~MayaUsdProxyShapeSceneIndexBase()
{
    _Destroy();                 // In a destructor calls are not virtual
}

void MayaUsdProxyShapeSceneIndexBase::_Destroy()
{
    if (_unregisterPathMapper) {
        Fvp::PathMapperRegistry::Instance().Unregister(_sceneIndexAppPath);
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
    SdfPath                                primPath = SdfPath::AbsoluteRootPath();
    std::optional<Fvp::InstancesSelection> instanceSelection;

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
            instanceSelection = { instancerPath,
                                  prototypeIndex,
                                  { instanceSchema.GetInstanceIndex()->GetTypedValue(0) } };
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
                        instanceSelection
                            = { primPath, iInstanceIndices, { std::stoi(lastComponentString) } };
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

    Fvp::PrimSelection  baseSelection = instanceSelection.has_value()
         ? Fvp::PrimSelection { primPath, { instanceSelection.value() } }
         : Fvp::PrimSelection { primPath };
    Fvp::PrimSelections primSelections({ baseSelection });

    // Point instancing : propagate selection to propagated prototypes
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
                // propagated. Currently we skip instancers so that selecting an instancer A that is
                // both drawing geometry but also prototyped and propagated for another instancer B
                // will only mark the geometry-drawing instancer A as selected. This can be changed.
                // For now, this only affects selection highlighting.
                if (propagatedPrim.primType != HdPrimTypeTokens->instancer) {
                    primSelections.push_back(
                        { propagatedPrimPath, primSelections.front().nestedInstanceIndices });
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
