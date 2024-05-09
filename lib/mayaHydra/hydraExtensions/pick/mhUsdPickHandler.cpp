//
// Copyright 2024 Autodesk
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
// See the License for the specific l anguage governing permissions and
// limitations under the License.
//

#include <mayaHydraLib/pick/mhUsdPickHandler.h>
#include <mayaHydraLib/pick/mhPickContext.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <mayaHydraLib/sceneIndex/registration.h>

#include <mayaUsdAPI/proxyStage.h>

#include <maya/MString.h>
#include <maya/MGlobal.h>

#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/modelAPI.h>

#include <ufe/path.h>
#include <ufe/hierarchy.h>
#include <ufe/selection.h>

PXR_NAMESPACE_OPEN_SCOPE

// Copy-pasted and adapted from maya-usd's
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/base/tokens.h
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/base/tokens.cpp

// Tokens that are used as picking optionVars in MayaUSD
//
// clang-format off
#define MAYAUSD_PICK_OPTIONVAR_TOKENS                   \
    /* The kind to be selected when viewport picking. */ \
    /* After resolving the picked prim, a search from */ \
    /* that prim up the USD namespace hierarchy will  */ \
    /* be performed looking for a prim that matches   */ \
    /* the kind in the optionVar. If no prim matches, */ \
    /* or if the selection kind is unspecified or     */ \
    /* empty, the exact prim picked in the viewport   */ \
    /* is selected.                                   */ \
    ((SelectionKind, "mayaUsd_SelectionKind"))           \
    /* The method to use to resolve viewport picking  */ \
    /* when the picked object is a point instance.    */ \
    /* The default behavior is "PointInstancer" which */ \
    /* will resolve to the PointInstancer prim that   */ \
    /* generated the point instance. The optionVar    */ \
    /* can also be set to "Instances" which will      */ \
    /* resolve to individual point instances, or to   */ \
    /* "Prototypes" which will resolve to the prim    */ \
    /* that is being instanced by the point instance. */ \
    ((PointInstancesPickMode, "mayaUsd_PointInstancesPickMode")) \
// clang-format on

TF_DEFINE_PRIVATE_TOKENS(MayaUsdPickOptionVars, MAYAUSD_PICK_OPTIONVAR_TOKENS);

// Copy-pasted and adapted from maya-usd's
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/render/vp2RenderDelegate/proxyRenderDelegate.h
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/render/vp2RenderDelegate/proxyRenderDelegate.cpp

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _pointInstancesPickModeTokens,

    (PointInstancer)
    (Instances)
    (Prototypes)
);
// clang-format on

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

// Copy pasted from
// https://github.com/Autodesk/maya-usd/blob/dev/lib/mayaUsd/render/vp2RenderDelegate/proxyRenderDelegate.cpp

//! \brief  Query the Kind to be selected from viewport.
//! \return A Kind token (https://graphics.pixar.com/usd/docs/api/kind_page_front.html). If the
//!         token is empty or non-existing in the hierarchy, the exact prim that gets picked
//!         in the viewport will be selected.
TfToken GetSelectionKind()
{
    static const MString kOptionVarName(MayaUsdPickOptionVars->SelectionKind.GetText());

    if (MGlobal::optionVarExists(kOptionVarName)) {
        MString optionVarValue = MGlobal::optionVarStringValue(kOptionVarName);
        return TfToken(optionVarValue.asChar());
    }
    return TfToken();
}

//! \brief  Returns the prim or an ancestor of it that is of the given kind.
//
// If neither the prim itself nor any of its ancestors above it in the
// namespace hierarchy have an authored kind that matches, an invalid null
// prim is returned.
UsdPrim GetPrimOrAncestorWithKind(const UsdPrim& prim, const TfToken& kind)
{
    UsdPrim iterPrim = prim;
    TfToken primKind;

    while (iterPrim) {
        if (UsdModelAPI(iterPrim).GetKind(&primKind) && KindRegistry::IsA(primKind, kind)) {
            break;
        }

        iterPrim = iterPrim.GetParent();
    }

    return iterPrim;
}

//! Pick resolution behavior to use when the picked object is a point instance.
enum UsdPointInstancesPickMode
{
    //! The PointInstancer prim that generated the point instance is picked. If
    //! multiple nested PointInstancers are involved, the top-level
    //! PointInstancer is the one picked. If a selection kind is specified, the
    //! traversal up the hierarchy looking for a kind match will begin at that
    //! PointInstancer.
    PointInstancer = 0,
    //! The specific point instance is picked. These are represented as
    //! UsdSceneItems with UFE paths to a PointInstancer prim and a non-negative
    //! instanceIndex for the specific point instance. In this mode, any setting
    //! for selection kind is ignored.
    Instances,
    //! The prototype being instanced by the point instance is picked. If a
    //! selection kind is specified, the traversal up the hierarchy looking for
    //! a kind match will begin at the prototype prim.
    Prototypes
};

//! \brief  Query the pick mode to use when picking point instances in the viewport.
//! \return A UsdPointInstancesPickMode enum value indicating the pick mode behavior
//!         to employ when the picked object is a point instance.
//!
//! This function retrieves the value for the point instances pick mode optionVar
//! and converts it into a UsdPointInstancesPickMode enum value. If the optionVar
//! has not been set or otherwise has an invalid value, the default pick mode of
//! PointInstancer is returned.
UsdPointInstancesPickMode GetPointInstancesPickMode()
{
    static const MString kOptionVarName(MayaUsdPickOptionVars->PointInstancesPickMode.GetText());

    auto pickMode = UsdPointInstancesPickMode::PointInstancer;

    if (MGlobal::optionVarExists(kOptionVarName)) {
        const TfToken pickModeToken(MGlobal::optionVarStringValue(kOptionVarName).asChar());

        if (pickModeToken == _pointInstancesPickModeTokens->Instances) {
            pickMode = UsdPointInstancesPickMode::Instances;
        } else if (pickModeToken == _pointInstancesPickModeTokens->Prototypes) {
            pickMode = UsdPointInstancesPickMode::Prototypes;
        }
    }

    return pickMode;
}

SdfPath instancerPrimOrigin(const HdxInstancerContext& instancerContext)
{
    // When USD prims are converted to Hydra prims (including point instancers),
    // they are given a prim origin data source which provides the information
    // as to which prim in the USD data model produced the rprim in the Hydra
    // scene index scene. This is what is used here to provide the Hydra scene
    // path to USD scene path picking to selection mapping.
    auto schema = HdPrimOriginSchema(instancerContext.instancerPrimOrigin);
    if (!schema) {
        return {};
    }

    return schema.GetOriginPath(HdPrimOriginSchemaTokens->scenePath);
}

UsdPickHandler::HitPath pickInstance(
    const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit
)
{
    // We match VP2 behavior and return the instance on the top-level instancer.
    const auto& instancerContext = primOrigin.instancerContexts.front();
    return {
        instancerPrimOrigin(instancerContext), instancerContext.instanceId};
}

UsdPickHandler::HitPath pickPrototype(
    const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit
)
{
    // The prototype path is the prim origin path in the USD data model.
    return {primOrigin.GetFullPath(), -1};
}

UsdPickHandler::HitPath pickInstancer(
    const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit
)
{
    // To return the top-level instancer, we use the first instancer context
    // prim origin.  To return the innermost instancer, we would use the last
    // instancer context prim origin.
    return {instancerPrimOrigin(primOrigin.instancerContexts.front()), -1};
}

Ufe::Path usdPathToUfePath(
    const MayaHydraSceneIndexRegistrationPtr& registration,
    const SdfPath&                            usdPath
)
{
    return registration ? registration->interpretRprimPathFn(
        registration->pluginSceneIndex, usdPath) : Ufe::Path();
}

}

namespace MAYAHYDRA_NS_DEF {

UsdPickHandler::HitPath UsdPickHandler::hitPath(const HdxPickHit& hit) const {
    auto primOrigin = HdxPrimOriginInfo::FromPickHit(
        renderIndex(), hit);

    if (hit.instancerId.IsEmpty()) {
        return {primOrigin.GetFullPath(), -1};
    }

    // If there is a Hydra instancer, distinguish between native instancing
    // (implicit USD prototype created by USD itself) and point instancing
    // (explicitly authored USD prototypes).  As per HdxInstancerContext
    // documentation:
    // 
    // [...] "exactly one of instancePrimOrigin or instancerPrimOrigin will
    // contain data depending on whether the instancing at the current
    // level was implicit or not, respectively."
    const auto& instancerContext = primOrigin.instancerContexts.front();

    if (instancerContext.instancePrimOrigin) {
        // Implicit prototype instancing (i.e. USD native instancing).
        auto schema = HdPrimOriginSchema(instancerContext.instancePrimOrigin);
        if (!TF_VERIFY(schema, "Cannot build prim origin schema for USD native instance.")) {
            return {SdfPath(), -1};
        }
        return {schema.GetOriginPath(HdPrimOriginSchemaTokens->scenePath), -1};
    }

    // Explicit prototype instancing (i.e. USD point instancing).
    std::function<HitPath(const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit)> pickFn[] = {pickInstancer, pickInstance, pickPrototype};
                        
    // Retrieve pick mode from mayaUsd optionVar, to see if we're picking
    // instances, the instancer itself, or the prototype instanced by the
    // point instance.
    return pickFn[GetPointInstancesPickMode()](primOrigin, hit);
}

bool UsdPickHandler::handlePickHit(
    const Input& pickInput, Output& pickOutput
) const
{
    if (!sceneIndexRegistry()) {
        TF_FATAL_ERROR("Picking called while no scene index registry exists");
        return false;
    }

    if (!renderIndex()) {
        TF_FATAL_ERROR("Picking called while no render index exists");
        return false;
    }

    auto registration = sceneIndexRegistry()->GetSceneIndexRegistrationForRprim(pickInput.pickHit.objectId);

    if (!registration) {
        return false;
    }

    // For the USD pick handler pick results are directly returned with USD
    // scene paths, so no need to remove scene index plugin path prefix.
    const auto& [pickedUsdPath, instanceNdx] = hitPath(pickInput.pickHit);

    const auto pickedMayaPath = usdPathToUfePath(registration, pickedUsdPath);
    const auto snMayaPath =
        // As per https://stackoverflow.com/questions/46114214
        // structured bindings cannot be captured by a lambda in C++ 17,
        // so pass in pickedUsdPath and instanceNdx as lambda arguments.
        [&pickedMayaPath, &registration](
            const SdfPath& pickedUsdPath, int instanceNdx) {

        if (instanceNdx >= 0) {
            // Point instance: add the instance index to the path.
            // Appending a numeric component to the path to identify a
            // point instance cannot be done on the picked SdfPath, as
            // numeric path components are not allowed by SdfPath.  Do so
            // here with Ufe::Path, which has no such restriction.
            return pickedMayaPath + std::to_string(instanceNdx);
        }

        // Not an instance: adjust picked path for selection kind.
        auto snKind = GetSelectionKind();
        if (snKind.IsEmpty()) {
            return pickedMayaPath;
        }

        // Get the prim from the stage and path, to access the
        // UsdModelAPI for the prim.
        auto proxyShapeObj = registration->dagNode.object();
        if (proxyShapeObj.isNull()) {
            TF_FATAL_ERROR("No mayaUsd proxy shape object corresponds to USD pick");
            return pickedMayaPath;
        }

        MayaUsdAPI::ProxyStage proxyStage{proxyShapeObj};
        auto prim = proxyStage.getUsdStage()->GetPrimAtPath(pickedUsdPath);
        prim = GetPrimOrAncestorWithKind(prim, snKind);
        const auto usdPath = prim ? prim.GetPath() : pickedUsdPath;

        return usdPathToUfePath(registration, usdPath);
    }(pickedUsdPath, instanceNdx);

    auto si = Ufe::Hierarchy::createItem(snMayaPath);
    if (!si) {
        return false;
    }

    pickOutput.ufeSelection->append(si);
    return true;
}

HdRenderIndex* UsdPickHandler::renderIndex() const
{
    return PickHandlerRegistry::Instance().GetPickContext()->renderIndex();
}

std::shared_ptr<const MayaHydraSceneIndexRegistry>
UsdPickHandler::sceneIndexRegistry() const
{
    return PickHandlerRegistry::Instance().GetPickContext()->sceneIndexRegistry();
}

}
