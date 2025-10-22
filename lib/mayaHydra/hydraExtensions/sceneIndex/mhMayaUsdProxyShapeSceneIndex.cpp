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

#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <mayaHydraLib/pick/mhUsdPickHandler.h>

#include <flowViewport/fvpInstruments.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

// mayaHydra headers
#include "ufeExtensions/Global.h"

#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>

#include <ufe/scene.h>
#include <ufe/sceneNotification.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const std::string digits = "0123456789";

// Pixar macros won't work without PXR_NS.
PXR_NAMESPACE_USING_DIRECTIVE
using namespace Ufe;
using namespace MayaHydra;

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
    UsdPathMapperSceneObserver(MayaUsdProxyShapeSceneIndex& psSi)
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

    MayaUsdProxyShapeSceneIndex& _psSi;
};

class UsdPathMapper : public Fvp::PathMapper
{
public:
    UsdPathMapper(const MayaUsdProxyShapeSceneIndex& psSi)
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
    const MayaUsdProxyShapeSceneIndex& _psSi;
};

} // namespace

namespace MAYAHYDRA_NS_DEF {

MayaUsdProxyShapeSceneIndex::MayaUsdProxyShapeSceneIndex(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath)
    : ParentClass(proxyStage, sceneIndexChainLastElement, usdImagingStageSceneIndex, dagNodeHandle)
    , _sceneIndexPathPrefix(sceneIndexPathPrefix)
    , _sceneIndexAppPath(sceneIndexAppPath)
    , _appSceneObserver(std::make_shared<UsdPathMapperSceneObserver>(*this))
    , _usdPathMapper(std::make_shared<UsdPathMapper>(*this))
{
    // Add our pick handler to the pick handler registry.  All USD scene indices
    // could share the same pick handler, but create a new one for simplicity.
    auto pickHandler = std::make_shared<UsdPickHandler>();
    TF_AXIOM(PickHandlerRegistry::Instance().Register(sceneIndexPathPrefix, pickHandler));

    // The gateway node (proxy shape) is a Maya node, so the scene index
    // path must be a single segment.
    TF_AXIOM(sceneIndexAppPath.nbSegments() == 1);

    // Observe the scene to be informed of path changes to the gateway node
    // (proxy shape) that corresponds to our scene index data producer.
    Scene::instance().addObserver(_appSceneObserver);

    // Register a mapper in the path mapper registry.
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Register(_sceneIndexAppPath, _usdPathMapper));
}

MayaUsdProxyShapeSceneIndex::~MayaUsdProxyShapeSceneIndex()
{
    _DestroyDerived(); // Base class will take care of its _Destroy()
}

void MayaUsdProxyShapeSceneIndex::_Destroy()
{
    _DestroyDerived();
    ParentClass::_Destroy();
}

void MayaUsdProxyShapeSceneIndex::_DestroyDerived()
{
    TF_AXIOM(PickHandlerRegistry::Instance().Unregister(_sceneIndexPathPrefix));

    // Unregister our path mapper.
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Unregister(_sceneIndexAppPath));

    // Ufe::Subject has automatic cleanup of stale observers, but this can
    // be problematic on application exit if the library of the observer is
    // cleaned up before that of the subject, so simply stop observing.
    Scene::instance().removeObserver(_appSceneObserver);
}

MayaUsdProxyShapeSceneIndexRefPtr MayaUsdProxyShapeSceneIndex::New(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath)
{
    return TfCreateRefPtr(new MayaUsdProxyShapeSceneIndex(
        proxyStage,
        sceneIndexChainLastElement,
        usdImagingStageSceneIndex,
        dagNodeHandle,
        sceneIndexPathPrefix,
        sceneIndexAppPath));
}

Fvp::PrimSelections
MayaUsdProxyShapeSceneIndex::UfePathToPrimSelections(const Ufe::Path& appPath) const
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
                // For now (2024/05/28), this only affects selection highlighting.
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