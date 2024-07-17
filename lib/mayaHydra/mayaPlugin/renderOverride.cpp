//
// Copyright 2019 Luma Pictures
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
// Copyright 2024 Autodesk, Inc. All rights reserved.
//


// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include "renderOverride.h"

#include "mayaColorPreferencesTranslator.h"
#include "pluginDebugCodes.h"
#include "renderOverrideUtils.h"
#include "tokens.h"

#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/sceneIndex/registration.h>
#include <mayaHydraLib/hydraUtils.h>

#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif
#include <flowViewport/tokens.h>
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <flowViewport/debugCodes.h>
#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/selection/fvpSelectionTask.h>
#include <flowViewport/selection/fvpSelection.h>
#include <flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.h>
#include <flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h>
#include <flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h>
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>
#include <flowViewport/sceneIndex/fvpRenderIndexProxy.h>
#include <flowViewport/sceneIndex/fvpBBoxSceneIndex.h>
#include <flowViewport/sceneIndex/fvpReprSelectorSceneIndex.h>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/type.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/pxr.h>

#include <ufe/hierarchy.h>
#include <ufe/selection.h>
#include <ufe/namedSelection.h>
#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/observableSelection.h>
#include <ufe/globalSelection.h>
#include <ufe/selectionNotification.h>
#include <ufe/observer.h>

#include <ufeExtensions/Global.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/instantiateSingleton.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/imaging/hd/rprim.h>
#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hdx/selectionTask.h>
#include <pxr/imaging/hdx/colorizeSelectionTask.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/imaging/hdx/tokens.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/kind/registry.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/modelAPI.h>
#include <pxr/pxr.h>

#include <mayaUsdAPI/proxyStage.h>

#include <maya/M3dView.h>
#include <maya/MConditionMessage.h>
#include <maya/MDGMessage.h>
#include <maya/MDrawContext.h>
#include <maya/MEventMessage.h>
#include <maya/MGlobal.h>
#include <maya/MNodeMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MProfiler.h>
#include <maya/MSceneMessage.h>
#include <maya/MSelectionList.h>
#include <maya/MTimerMessage.h>
#include <maya/MUiMessage.h>
#include <maya/MFnCamera.h>
#include <maya/MFileIO.h>

#include <atomic>
#include <chrono>
#include <exception>
#include <limits>

int _profilerCategory = MProfiler::addCategory(
    "MtohRenderOverride (mayaHydra)",
    "Events from mayaHydra render override");

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

namespace {

PXR_NAMESPACE_USING_DIRECTIVE

static const SdfPath MAYA_NATIVE_ROOT = SdfPath("/MayaHydraViewportRenderer");

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

TfToken GetGeomSubsetsPickMode()
{
    static const MString kOptionVarName(MayaHydraPickOptionVars->GeomSubsetsPickMode.GetText());

    if (MGlobal::optionVarExists(kOptionVarName)) {
        return TfToken(MGlobal::optionVarStringValue(kOptionVarName).asChar());
    }

    return GeomSubsetsPickModeTokens->None;
}

struct PickInput {
    PickInput(
        const HdxPickHit&                pickHitArg, 
        const MHWRender::MSelectionInfo& pickInfoArg,
        const bool                       isSolePickHit
    ) : pickHit(pickHitArg), pickInfo(pickInfoArg), isSolePickHit(isSolePickHit) {}

    const HdxPickHit&                pickHit;
    const MHWRender::MSelectionInfo& pickInfo;
    const bool                       isSolePickHit;
};

// Picking output can go either to the UFE representation of the Maya selection
// (which supports non-Maya objects), or the classic MSelectionList
// representation of the Maya selection (which only supports Maya objects). It
// is up to the implementer of the pick handler to decide which is used. If the
// Maya selection is used, there must be a world space hit point in one to one
// correspondence with each Maya selection item placed into the MSelectionList.
struct PickOutput {
    PickOutput(
        MSelectionList&                 mayaSn,
        MPointArray&                    worldSpaceHitPts,
        const Ufe::NamedSelection::Ptr& ufeSn
    ) : mayaSelection(mayaSn), mayaWorldSpaceHitPts(worldSpaceHitPts),
        ufeSelection(ufeSn) {}

    MSelectionList&                 mayaSelection;
    MPointArray&                    mayaWorldSpaceHitPts;
    const Ufe::NamedSelection::Ptr& ufeSelection;
};

// The SdfPath is in the original data model scene (USD), not in
// the scene index scene. 
using HitPath = std::tuple<SdfPath, int>;

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

HitPath pickInstance(
    const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit
)
{
    // We match VP2 behavior and return the instance on the top-level instancer.
    const auto& instancerContext = primOrigin.instancerContexts.front();
    return {
        instancerPrimOrigin(instancerContext), instancerContext.instanceId};
}

HitPath pickPrototype(
    const HdxPrimOriginInfo& primOrigin, const HdxPickHit& hit
)
{
    // The prototype path is the prim origin path in the USD data model.
    return {primOrigin.GetFullPath(), -1};
}

HitPath pickInstancer(
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

inline bool areDifferentForOneOfTheseBits(unsigned int val1, unsigned int val2, unsigned int bitsToTest)
{
    return ((val1 & bitsToTest) != (val2 & bitsToTest));
}

inline bool isInComponentsPickingMode(const MHWRender::MSelectionInfo& selectInfo)
{
    return selectInfo.selectable(MSelectionMask::kSelectMeshVerts)
        || selectInfo.selectable(MSelectionMask::kSelectMeshEdges)
        || selectInfo.selectable(MSelectionMask::kSelectMeshFreeEdges)
        || selectInfo.selectable(MSelectionMask::kSelectMeshFaces)
        || selectInfo.selectable(MSelectionMask::kSelectVertices)
        || selectInfo.selectable(MSelectionMask::kSelectEdges)
        || selectInfo.selectable(MSelectionMask::kSelectFacets);
}

}

PXR_NAMESPACE_OPEN_SCOPE

class MtohRenderOverride::PickHandlerBase {
public:

    virtual bool handlePickHit(
        const PickInput& pickInput, PickOutput& pickOutput) const = 0;

protected:

    PickHandlerBase(MtohRenderOverride& renderOverride) : 
        _renderOverride(renderOverride) {}

    MayaHydraSceneIndexRefPtr mayaSceneIndex() const {
        return _renderOverride._mayaHydraSceneIndex;
    }

    std::shared_ptr<const MayaHydraSceneIndexRegistry>
    sceneIndexRegistry() const {
        return _renderOverride._sceneIndexRegistry;
    }

    HdRenderIndex* renderIndex() const { return _renderOverride._renderIndex; }

private:

    MtohRenderOverride& _renderOverride;
};

PXR_NAMESPACE_CLOSE_SCOPE

namespace {

// Replace the builtin and fixed colorize selection and selection tasks from
// Hydra with our own Flow Viewport selection task.  The Hydra tasks are not
// configurable and cannot be replaced by plugin behavior.  Currently, the Flow
// Viewport selection task is a no-op.  PPT, 2-Oct-2023.

void replaceSelectionTask(PXR_NS::HdTaskSharedPtrVector* tasks)
{
    // For TF_WARN and TF_AXIOM macros.
    PXR_NAMESPACE_USING_DIRECTIVE

    TF_AXIOM(tasks);

    auto isSnTask = [](const HdTaskSharedPtr& task) {
        return std::dynamic_pointer_cast<HdxColorizeSelectionTask>(task) || 
            std::dynamic_pointer_cast<HdxSelectionTask>(task);
    };

    auto found = std::find_if(tasks->begin(), tasks->end(), isSnTask);

    if (found == tasks->end()) {
        TF_WARN("Fvp::SelectionTask not inserted into render task vector!");
        return;
    }

    *found = HdTaskSharedPtr(new Fvp::SelectionTask);
}

class MayaPickHandler : public MtohRenderOverride::PickHandlerBase {
public:

    MayaPickHandler(MtohRenderOverride& renderOverride) : 
        PickHandlerBase(renderOverride) {}

    bool handlePickHit(
        const PickInput& pickInput, PickOutput& pickOutput) const override
    {
        if (!mayaSceneIndex()) {
            TF_FATAL_ERROR("Picking called while no Maya scene index exists");
            return false;
        }

        // Maya does not create Hydra instances, so if the pick hit instancer
        // ID isn't empty, it's not a Maya pick hit.
        if (!pickInput.pickHit.instancerId.IsEmpty()) {
            return false;
        }

        return mayaSceneIndex()->AddPickHitToSelectionList(
            pickInput.pickHit, pickInput.pickInfo, 
            pickOutput.mayaSelection, pickOutput.mayaWorldSpaceHitPts
        );
    }
};

class UsdPickHandler : public MtohRenderOverride::PickHandlerBase {
public:

    UsdPickHandler(MtohRenderOverride& renderOverride) : 
        PickHandlerBase(renderOverride) {}
    
#if PXR_VERSION >= 2403
    std::vector<HitPath> resolveGeomSubsetsPicking(
        HdSceneIndexBaseConstRefPtr sceneIndex, 
        const SdfPath& basePrimPath, 
        const TfToken& geomSubsetType, 
        int componentIndex) const
    {
        if (componentIndex < 0 || sceneIndex->GetPrim(basePrimPath).primType != HdPrimTypeTokens->mesh) {
            return {};
        }

        std::vector<HitPath> pickedGeomSubsets;
        auto childPaths = sceneIndex->GetChildPrimPaths(basePrimPath);
        for (const auto& childPath : childPaths) {
            HdSceneIndexPrim childPrim = sceneIndex->GetPrim(childPath);
            if (childPrim.primType != HdPrimTypeTokens->geomSubset) {
                continue;
            }

            HdGeomSubsetSchema geomSubsetSchema = HdGeomSubsetSchema(childPrim.dataSource);
            if (!geomSubsetSchema.IsDefined() || geomSubsetSchema.GetType()->GetTypedValue(0) != geomSubsetType) {
                continue;
            }

            auto geomSubsetIndices = geomSubsetSchema.GetIndices()->GetTypedValue(0);
            for (const auto& index : geomSubsetIndices) {
                if (index == componentIndex) {
                    HdPrimOriginSchema primOriginSchema = HdPrimOriginSchema::GetFromParent(childPrim.dataSource);
                    if (primOriginSchema.IsDefined()) {
                        auto usdPath = primOriginSchema.GetOriginPath(HdPrimOriginSchemaTokens->scenePath);
                        pickedGeomSubsets.push_back({usdPath, -1});
                    }
                }
            }
        }
        return pickedGeomSubsets;
    }
#endif

    // Return the closest path and the instance index in the scene index scene
    // that corresponds to the pick hit.  If the pick hit is not an instance,
    // the instance index will be -1.
    HitPath resolveInstancePicking(HdRenderIndex& renderIndex, const HdxPickHit& pickHit) const
    {
        auto primOrigin = HdxPrimOriginInfo::FromPickHit(&renderIndex, pickHit);

        if (pickHit.instancerId.IsEmpty()) {
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
        return pickFn[GetPointInstancesPickMode()](primOrigin, pickHit);
    }

    bool handlePickHit(
        const PickInput& pickInput, PickOutput& pickOutput
    ) const override
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

        std::vector<HitPath> hitPaths;

#if PXR_VERSION >= 2403
        if (GetGeomSubsetsPickMode() == GeomSubsetsPickModeTokens->Faces) {
            auto geomSubsetsHitPaths = resolveGeomSubsetsPicking(
                renderIndex()->GetTerminalSceneIndex(), 
                pickInput.pickHit.objectId, 
                HdGeomSubsetSchemaTokens->typeFaceSet, 
                pickInput.pickHit.elementIndex);
            if (!geomSubsetsHitPaths.empty()) {
                hitPaths.insert(hitPaths.end(), geomSubsetsHitPaths.begin(), geomSubsetsHitPaths.end());
            }

            // If we did not find any geomSubset and this is the only pick hit, then fallback to selecting the base prim/instance.
            if (hitPaths.empty() && pickInput.isSolePickHit) {
                hitPaths.push_back(resolveInstancePicking(*renderIndex(), pickInput.pickHit));
            }
        } else {
            hitPaths.push_back(resolveInstancePicking(*renderIndex(), pickInput.pickHit));
        }
#else
        hitPaths.push_back(resolveInstancePicking(*renderIndex(), pickInput.pickHit));
#endif

        size_t nbSelectedUfeItems = 0;
        for (const auto& [pickedUsdPath, instanceNdx] : hitPaths) {
            // For the USD pick handler pick results are directly returned with USD
            // scene paths, so no need to remove scene index plugin path prefix.
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
                continue;
            }

            pickOutput.ufeSelection->append(si);
            nbSelectedUfeItems++;
        }
        return nbSelectedUfeItems > 0;
    }
};

}

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

namespace {

// Not sure if we actually need a mutex guarding _allInstances, but
// everywhere that uses it isn't a "frequent" operation, so the
// extra speed loss should be fine, and I'd rather be safe.
std::mutex                       _allInstancesMutex;
std::vector<MtohRenderOverride*> _allInstances;

//! \brief  Get the index of the hit nearest to a given cursor point.
int GetNearestHitIndex(
    const MHWRender::MFrameContext& frameContext,
    const HdxPickHitVector&         hits,
    int                             cursor_x,
    int                             cursor_y)
{
    int nearestHitIndex = -1;

    double dist2_min = std::numeric_limits<double>::max();
    float  depth_min = std::numeric_limits<float>::max();

    for (unsigned int i = 0; i < hits.size(); i++) {
        const HdxPickHit& hit = hits[i];
        const MPoint      worldSpaceHitPoint(
            hit.worldSpaceHitPoint[0], hit.worldSpaceHitPoint[1], hit.worldSpaceHitPoint[2]);

        // Calculate the (x, y) coordinate relative to the lower left corner of the viewport.
        double hit_x, hit_y;
        frameContext.worldToViewport(worldSpaceHitPoint, hit_x, hit_y);

        // Calculate the 2D distance between the hit and the cursor
        double dist_x = hit_x - (double)cursor_x;
        double dist_y = hit_y - (double)cursor_y;
        double dist2 = dist_x * dist_x + dist_y * dist_y;

        // Find the hit nearest to the cursor.
        if ((dist2 < dist2_min) || (dist2 == dist2_min && hit.normalizedDepth < depth_min)) {
            dist2_min = dist2;
            depth_min = hit.normalizedDepth;
            nearestHitIndex = (int)i;
        }
    }

    return nearestHitIndex;
}
} // namespace

class MtohRenderOverride::SelectionObserver : public Ufe::Observer
{
public:
    SelectionObserver(MtohRenderOverride& renderOverride)
        : Ufe::Observer(), _renderOverride(renderOverride)
    {}

    void operator()(const Ufe::Notification& notification) override 
    {
        // During Maya file read, each node will be selected in turn, so we get
        // notified for each node in the scene.  Prune this out.
        if (MFileIO::isOpeningFile()) {
            return;
        }

        _renderOverride.SelectionChanged(
            dynamic_cast<const Ufe::SelectionChanged&>(notification));
    }

private:
    MtohRenderOverride& _renderOverride;
};

// MtohRenderOverride is a rendering override class for the viewport to use Hydra instead of VP2.0.
MtohRenderOverride::MtohRenderOverride(const MtohRendererDescription& desc)
    : MHWRender::MRenderOverride(desc.overrideName.GetText())
    , _rendererDesc(desc)
    , _sceneIndexRegistry(nullptr)
    , _globals(MtohRenderGlobals::GetInstance())
    , _hgi(Hgi::CreatePlatformDefaultHgi())
    , _hgiDriver { HgiTokens->renderDriver, VtValue(_hgi.get()) }
    , _fvpSelectionTracker(new Fvp::SelectionTracker)
    , _ufeSn(Ufe::NamedSelection::get("MayaSelectTool"))
    , _mayaSelectionObserver(std::make_shared<SelectionObserver>(*this))
    , _isUsingHdSt(desc.rendererName == MtohTokens->HdStormRendererPlugin)
    // unique_ptr is not copyable, so can't use initializer_list, which copies.
    , _pickHandlers([this](){
            std::vector<std::unique_ptr<PickHandlerBase>> v;
            v.push_back(std::make_unique<MayaPickHandler>(*this));
            v.push_back(std::make_unique<UsdPickHandler>(*this));
            return v;
        }())
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "MtohRenderOverride created (%s - %s - %s)\n",
            _rendererDesc.rendererName.GetText(),
            _rendererDesc.overrideName.GetText(),
            _rendererDesc.displayName.GetText());
    _ID = MAYA_NATIVE_ROOT.AppendChild(
                  TfToken(TfStringPrintf("_MayaHydra_%s_%p", desc.rendererName.GetText(), this)));

    MStatus status;
    auto    id
        = MSceneMessage::addCallback(MSceneMessage::kBeforeNew, _ClearHydraCallback, this, &status);
    if (status) {
        _callbacks.append(id);
    }
    id = MSceneMessage::addCallback(MSceneMessage::kBeforeOpen, _ClearHydraCallback, this, &status);
    if (status) {
        _callbacks.append(id);
    }

    // Observe the UFE selection.
    auto sn = Ufe::GlobalSelection::get();
    TF_AXIOM(sn);
    sn->addObserver(_mayaSelectionObserver);

    // Setup the playblast watch.
    // _playBlasting is forced to true here so we can just use _PlayblastingChanged below
    //
    _playBlasting = true;
    MConditionMessage::addConditionCallback(
        "playblasting", &MtohRenderOverride::_PlayblastingChanged, this, &status);
    MtohRenderOverride::_PlayblastingChanged(false, this);

    _defaultLight.SetSpecular(GfVec4f(0.0f));
    _defaultLight.SetAmbient(GfVec4f(0.0f));

    {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        _allInstances.push_back(this);
    }
}

MtohRenderOverride::~MtohRenderOverride()
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg(
            "MtohRenderOverride destroyed (%s - %s - %s)\n",
            _rendererDesc.rendererName.GetText(),
            _rendererDesc.overrideName.GetText(),
            _rendererDesc.displayName.GetText());

    if (_timerCallback)
        MMessage::removeCallback(_timerCallback);

    constexpr bool fullReset = true;
    ClearHydraResources(fullReset);

    for (auto operation : _operations) {
        delete operation;
    }
    MMessage::removeCallbacks(_callbacks);
    _callbacks.clear();
    for (auto& panelAndCallbacks : _renderPanelCallbacks) {
        MMessage::removeCallbacks(panelAndCallbacks.second);
    }

    if (!_allInstances.empty()) {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        _allInstances.erase(
            std::remove(_allInstances.begin(), _allInstances.end(), this), _allInstances.end());
    }
}

HdRenderDelegate* MtohRenderOverride::_GetRenderDelegate()
{
    return _renderIndex ? _renderIndex->GetRenderDelegate() : nullptr;
}

void MtohRenderOverride::UpdateRenderGlobals(
    const MtohRenderGlobals& globals,
    const TfToken&           attrName)
{
    // If no attribute or attribute starts with 'mayaHydra', these setting wil be applied on the
    // next call to MtohRenderOverride::Render, so just force an invalidation
    // XXX: This will need to change if mayaHydra settings should ever make it to the delegate
    // itself.
    if (attrName.GetString().find("mayaHydra") != 0) {
        std::lock_guard<std::mutex> lock(_allInstancesMutex);
        for (auto* instance : _allInstances) {
            const auto& rendererName = instance->_rendererDesc.rendererName;

            // If no attrName or the attrName is the renderer, then update everything
            const size_t attrFilter = (attrName.IsEmpty() || attrName == rendererName) ? 0 : 1;
            if (attrFilter && !instance->_globals.AffectsRenderer(attrName, rendererName)) {
                continue;
            }

            // Will be applied in _InitHydraResources later anyway
            if (auto* renderDelegate = instance->_GetRenderDelegate()) {
                instance->_globals.ApplySettings(
                    renderDelegate,
                    instance->_rendererDesc.rendererName,
                    TfTokenVector(attrFilter, attrName));
                if (attrFilter) {
                    break;
                }
            }
        }
    }

    // Less than ideal still
    MGlobal::executeCommandOnIdle("refresh -f");
}

VtValue MtohRenderOverride::_GetUsedGPUMemory() const
{
    // Currently, only Storm is the known/tested renderer that provides GPU stats
    // via the Render Delegate.
    if (_isUsingHdSt && _renderDelegate)
    {
        VtDictionary hdStRenderStat = _renderDelegate->GetRenderStats();
        return hdStRenderStat[HdPerfTokens->gpuMemoryUsed.GetString()];
    }
    return VtValue();
}

int MtohRenderOverride::GetUsedGPUMemory()
{   
    int totalGPUMemory = 0;
    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        totalGPUMemory += instance->_GetUsedGPUMemory().UncheckedGet<int>();
    }
    return totalGPUMemory / (1024*1024); 
}

std::vector<MString> MtohRenderOverride::AllActiveRendererNames()
{
    std::vector<MString> renderers;

    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        if (instance->_initializationSucceeded) {
            renderers.push_back(instance->_rendererDesc.rendererName.GetText());
        }
    }
    return renderers;
}

SdfPathVector MtohRenderOverride::RendererRprims(TfToken rendererName, bool visibleOnly)
{
    MtohRenderOverride* instance = _GetByName(rendererName);
    if (!instance) {
        return SdfPathVector();
    }

    auto* renderIndex = instance->_renderIndex;
    if (!renderIndex) {
        return SdfPathVector();
    }
    auto primIds = renderIndex->GetRprimIds();
    if (visibleOnly) {
        primIds.erase(
            std::remove_if(
                primIds.begin(),
                primIds.end(),
                [renderIndex](const SdfPath& primId) {
                    auto* rprim = renderIndex->GetRprim(primId);
                    if (!rprim)
                        return true;
                    return !rprim->IsVisible();
                }),
            primIds.end());
    }
    return primIds;
}

SdfPath MtohRenderOverride::RendererSceneDelegateId(TfToken rendererName, TfToken sceneDelegateName)
{
    MtohRenderOverride* instance = _GetByName(rendererName);
    if (!instance) {
        return SdfPath();
    }

    if (instance->_mayaHydraSceneIndex) {
        return instance->_mayaHydraSceneIndex->GetDelegateID(sceneDelegateName);
    }
    return SdfPath();
}

void MtohRenderOverride::_DetectMayaDefaultLighting(const MHWRender::MDrawContext& drawContext)
{
    constexpr auto considerAllSceneLights = MHWRender::MDrawContext::kFilteredIgnoreLightLimit;

    const auto numLights = drawContext.numberOfActiveLights(considerAllSceneLights);
    auto       foundMayaDefaultLight = false;
    if (numLights == 1) {
        auto* lightParam = drawContext.getLightParameterInformation(0, considerAllSceneLights);
        if (lightParam != nullptr && !lightParam->lightPath().isValid()) {
            // This light does not exist so it must be the
            // default maya light
            MFloatPointArray positions;
            MFloatVector     direction;
            auto             intensity = 0.0f;
            MColor           color;
            auto             hasDirection = false;
            auto             hasPosition = false;

            // Maya default light has no position, only direction
            drawContext.getLightInformation(
                0,
                positions,
                direction,
                intensity,
                color,
                hasDirection,
                hasPosition,
                considerAllSceneLights);

            if (hasDirection && !hasPosition) {

                // Note for devs : if you update more parameters in the default light, don't forget
                // to update MtohDefaultLightDelegate::SetDefaultLight and MayaHydraSceneIndex::SetDefaultLight, currently there are only 3 :
                // position, diffuse, specular
                GfVec3f position;
                GetDirectionalLightPositionFromDirectionVector(position, {direction.x, direction.y, direction.z});
                _defaultLight.SetPosition({ position.data()[0], position.data()[1], position.data()[2], 0.0f });
                _defaultLight.SetDiffuse(
                    { intensity * color.r, intensity * color.g, intensity * color.b, 1.0f });
                _defaultLight.SetSpecular(
                    { intensity * color.r, intensity * color.g, intensity * color.b, 1.0f });
                foundMayaDefaultLight = true;
            }
        }
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_DEFAULT_LIGHTING)
        .Msg(
            "MtohRenderOverride::"
            "_DetectMayaDefaultLighting() "
            "foundMayaDefaultLight=%i\n",
            foundMayaDefaultLight);

    if (foundMayaDefaultLight != _hasDefaultLighting) {
        _hasDefaultLighting = foundMayaDefaultLight;
        TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_DEFAULT_LIGHTING)
            .Msg(
                "MtohRenderOverride::"
                "_DetectMayaDefaultLighting() clearing! "
                "_hasDefaultLighting=%i\n",
                _hasDefaultLighting);
    }
}

MStatus MtohRenderOverride::Render(
    const MHWRender::MDrawContext&                         drawContext,
    const MHWRender::MDataServerOperation::MViewportScene& scene)
{
    // It would be good to clear the resources of the overrides that are
    // not in active use, but I'm not sure if we have a better way than
    // the idle time we use currently. The approach below would break if
    // two render overrides were used at the same time.
    // for (auto* override: _allInstances) {
    //     if (override != this) {
    //         override->ClearHydraResources();
    //     }
    // }
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER).Msg("MtohRenderOverride::Render()\n");
    auto renderFrame = [&](bool markTime = false) {
        HdTaskSharedPtrVector tasks = _taskController->GetRenderingTasks();

        // For playblasting, a glReadPixels is going to occur sometime after we return.
        // But if we call Execute on all of the tasks, then z-buffer fighting may occur
        // because every colorize/present task is going to be drawing a full-screen quad
        // with 'unconverged' depth.
        //
        // To work arround this (for not Storm) we pull the first task, (render/synch)
        // and continually execute it until the renderer signals converged, at which point
        // we break and call HdEngine::Execute once more to copy the aovs into OpenGL
        //
        if (_playBlasting && !_isUsingHdSt && !tasks.empty()) {
            // XXX: Is this better as user-configurable ?
            constexpr auto                 msWait = std::chrono::duration<float, std::milli>(100);
            std::shared_ptr<HdxRenderTask> renderTask
                = std::dynamic_pointer_cast<HdxRenderTask>(tasks.front());
            if (renderTask) {
                HdTaskSharedPtrVector renderOnly = { renderTask };
                _engine.Execute(_renderIndex, &renderOnly);

                while (_playBlasting && !renderTask->IsConverged()) {
                    std::this_thread::sleep_for(msWait);
                    _engine.Execute(_renderIndex, &renderOnly);
                }
            } else {
                TF_WARN("HdxProgressiveTask not found");
            }
        }

        // MAYA-114630
        // https://github.com/PixarAnimationStudios/USD/commit/fc63eaef29
        // removed backing, and restoring of GL_FRAMEBUFFER state.
        // At the same time HdxColorizeSelectionTask modifies the frame buffer state
        // Manually backup and restore the state of the frame buffer for now.
        MayaHydraGLBackup backup;
        if (_backupFrameBufferWorkaround) {
            HdTaskSharedPtr backupTask(new MayaHydraBackupGLStateTask(backup));
            HdTaskSharedPtr restoreTask(new MayaHydraRestoreGLStateTask(backup));
            tasks.reserve(tasks.size() + 2);
            for (auto it = tasks.begin(); it != tasks.end(); it++) {
                if (std::dynamic_pointer_cast<HdxColorizeSelectionTask>(*it)) {
                    tasks.insert(it, backupTask);
                    tasks.insert(it + 2, restoreTask);
                    break;
                }
            }
        }

        // Replace the existing HdxTaskController selection task (Storm) or
        // colorize selection task (non-Storm) with our selection task by
        // editing the task list, since HdxTaskController is not configurable.
        // As the existence of either task depends on AOV support, they may not
        // be present, so we may have nothing to replace.  PPT, 11-Aug-2023.
        replaceSelectionTask(&tasks);

        if (scene.changed()) {
            if (_mayaHydraSceneIndex) {
                _mayaHydraSceneIndex->HandleCompleteViewportScene(
                    scene, static_cast<MFrameContext::DisplayStyle>(drawContext.getDisplayStyle()));
            }
        }

        // Update plugin data producers
        for (auto& viewportData : Fvp::ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetAllViewportInfoAndData()) {
            for (auto& dataProducer : viewportData.GetDataProducerSceneIndicesData()) {
                dataProducer->UpdateVisibility();
                dataProducer->UpdateTransform();
            }
        }

        // Update plugin filtering scene indices
        std::string rendererNamesToUpdate;
        for (auto& sceneFilteringSceneIndexData : Fvp::FilteringSceneIndexInterfaceImp::get().getSceneFilteringSceneIndicesData()) {
            if (sceneFilteringSceneIndexData->UpdateVisibility()) {
                rendererNamesToUpdate += sceneFilteringSceneIndexData->GetClient()->getRendererNames();
            }
        }
        for (auto& selectionHighlightFilteringSceneIndexData : Fvp::FilteringSceneIndexInterfaceImp::get().getSelectionHighlightFilteringSceneIndicesData()) {
            if (selectionHighlightFilteringSceneIndexData->UpdateVisibility()) {
                rendererNamesToUpdate += selectionHighlightFilteringSceneIndexData->GetClient()->getRendererNames();
            }
        }
        if (!rendererNamesToUpdate.empty()) {
            Fvp::FilteringSceneIndicesChainManager::get().updateFilteringSceneIndicesChain(rendererNamesToUpdate);
        }

        _engine.Execute(_renderIndex, &tasks);

        // HdTaskController will query all of the tasks it can for IsConverged.
        // This includes HdRenderPass::IsConverged and HdRenderBuffer::IsConverged (via colorizer).
        //
        _isConverged = _taskController->IsConverged();
        if (markTime) {
            std::lock_guard<std::mutex> lock(_lastRenderTimeMutex);
            _lastRenderTime = std::chrono::system_clock::now();
        }
    };
    if (_initializationAttempted && !_initializationSucceeded) {
        // Initialization must have failed already, stop trying.
        return MStatus::kFailure;
    }

    _DetectMayaDefaultLighting(drawContext);
    if (_needsClear.exchange(false)) {
        constexpr bool fullReset = false;
        ClearHydraResources(fullReset);
    }

    if (!_initializationAttempted) {
        _InitHydraResources(drawContext);

        if (!_initializationSucceeded) {
            return MStatus::kFailure;
        }
    }

    //This code with strings comparison will go away when doing multi viewports
    MString panelName;
    auto framecontext = getFrameContext();
    if (framecontext){
        framecontext->renderingDestination(panelName);
        auto& manager = Fvp::ViewportInformationAndSceneIndicesPerViewportDataManager::Get();
        if (false == manager.ModelPanelIsAlreadyRegistered(panelName.asChar())){
            //Get information from viewport
            std::string cameraName;
    
            M3dView view;
	        if (M3dView::getM3dViewFromModelPanel(panelName, view)){
                MDagPath dpath;
	            view.getCamera(dpath);
	            MFnCamera viewCamera(dpath);
	            cameraName = viewCamera.name().asChar();
            }
    
            //Create a HydraViewportInformation 
            const Fvp::InformationInterface::ViewportInformation hydraViewportInformation(std::string(panelName.asChar()), cameraName);
            const bool dataProducerSceneIndicesAdded = manager.AddViewportInformation(hydraViewportInformation, _renderIndexProxy, _lastFilteringSceneIndexBeforeCustomFiltering);
            //Update the selection since we have added data producer scene indices through manager.AddViewportInformation to the merging scene index
            if (dataProducerSceneIndicesAdded && _selectionSceneIndex){
                _selectionSceneIndex->ReplaceSelection(*Ufe::GlobalSelection::get());
            }
            //Update the leadObjectTacker in case it could not find the current lead object which could be in a custom data producer scene index or a maya usd proxy shape scene index
            if (_leadObjectPathTracker){
                _leadObjectPathTracker->updatePrimPaths();
            }
        }
    }

    const unsigned int currentDisplayStyle = drawContext.getDisplayStyle();
    MayaHydraParams delegateParams = _globals.delegateParams;
    delegateParams.displaySmoothMeshes = !(currentDisplayStyle & MHWRender::MFrameContext::kFlatShaded);
    
    const bool currentUseDefaultMaterial = (drawContext.getDisplayStyle() & MHWRender::MFrameContext::kDefaultMaterial);
    const bool xRayEnabled = (drawContext.getDisplayStyle() & MHWRender::MFrameContext::kXray);

    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->SetDefaultLightEnabled(_hasDefaultLighting);
        _mayaHydraSceneIndex->SetDefaultLight(_defaultLight);
        _mayaHydraSceneIndex->SetParams(delegateParams);
        _mayaHydraSceneIndex->PreFrame(drawContext);
        
        if (_NeedToRecreateTheSceneIndicesChain(currentDisplayStyle, xRayEnabled)){
            _blockPrimRemovalPropagationSceneIndex->setPrimRemovalBlocked(true);//Prevent prim removal propagation to keep the current selection.
            //We need to recreate the filtering scene index chain after the merging scene index as there was a change such as in the BBox display style which has been turned on or off.
            _lastFilteringSceneIndexBeforeCustomFiltering = nullptr;//Release
            _CreateSceneIndicesChainAfterMergingSceneIndex(drawContext);
            auto& manager = Fvp::ViewportInformationAndSceneIndicesPerViewportDataManager::Get();
            manager.RemoveViewportInformation(std::string(panelName.asChar()));
            //Get information from viewport
            std::string cameraName;
            M3dView view;
            if (M3dView::getM3dViewFromModelPanel(panelName, view)){
                MDagPath dpath;
                view.getCamera(dpath);
                MFnCamera viewCamera(dpath);
                cameraName = viewCamera.name().asChar();
            }
            const Fvp::InformationInterface::ViewportInformation hydraViewportInformation(std::string(panelName.asChar()), cameraName);
            manager.AddViewportInformation(hydraViewportInformation, _renderIndexProxy, _lastFilteringSceneIndexBeforeCustomFiltering);
            
            _xRayEnabled = xRayEnabled;
            _blockPrimRemovalPropagationSceneIndex->setPrimRemovalBlocked(false);//Allow prim removal propagation again.
        }
    }

    if (_displayStyleSceneIndex) {
       _displayStyleSceneIndex->SetRefineLevel({true, delegateParams.refineLevel});
    }

    // Toggle textures in the material network
    const unsigned int currentDisplayMode = drawContext.getDisplayStyle();
    bool isTextured = currentDisplayMode & MHWRender::MFrameContext::kTextured;
    if (_pruneTexturesSceneIndex && 
        _currentlyTextured != isTextured) {
        _pruneTexturesSceneIndex->MarkTexturesDirty(isTextured);
        _currentlyTextured = isTextured;
    }

    if (_defaultMaterialSceneIndex && _useDefaultMaterial != currentUseDefaultMaterial){
        _defaultMaterialSceneIndex->Enable(currentUseDefaultMaterial);
        _useDefaultMaterial = currentUseDefaultMaterial;
    }

    HdxRenderTaskParams params;
    params.enableLighting = true;
    params.enableSceneMaterials = true;

    PXR_NS::GfVec4f wireframeSelectionColor;
    if (Fvp::ColorPreferences::getInstance().getColor(
            FvpColorPreferencesTokens->wireframeSelection, wireframeSelectionColor)) {
        params.wireframeColor = wireframeSelectionColor;
    }

    params.cullStyle = HdCullStyleBackUnlessDoubleSided;

    int width = 0;
    int height = 0;
    drawContext.getRenderTargetSize(width, height);

    bool vpDirty;
    if ((vpDirty = (width != _viewport[2] || height != _viewport[3]))) {
        _viewport = GfVec4d(0, 0, width, height);
        _taskController->SetRenderViewport(_viewport);
    }

    // Set Purpose tags
    SetRenderPurposeTags(delegateParams);

    // Set MSAA as per Maya AntiAliasing settings
    if (_isUsingHdSt)
    {  
        // Maya's MSAA toggle settings
        bool isMultiSampled = framecontext->getPostEffectEnabled(MHWRender::MFrameContext::kAntiAliasing);

        // Set MSAA on Color Buffer
        HdAovDescriptor colorAovDesc = _taskController->GetRenderOutputSettings(HdAovTokens->color);
        colorAovDesc.multiSampled = isMultiSampled;
        _taskController->SetRenderOutputSettings(HdAovTokens->color, colorAovDesc);

        // Set MSAA of Depth buffer
        HdAovDescriptor depthAovDesc = _taskController->GetRenderOutputSettings(HdAovTokens->depth);
        depthAovDesc.multiSampled = isMultiSampled;        
        _taskController->SetRenderOutputSettings(HdAovTokens->depth, depthAovDesc);
    }

    _taskController->SetFreeCameraMatrices(
        GetGfMatrixFromMaya(
            drawContext.getMatrix(MHWRender::MFrameContext::kViewMtx)),
        GetGfMatrixFromMaya(
            drawContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx)));

    if (delegateParams.motionSamplesEnabled()) {
        MStatus  status;
        MDagPath camPath = getFrameContext()->getCurrentCameraPath(&status);
        if (status == MStatus::kSuccess) {
            MString   ufeCameraPathString = getFrameContext()->getCurrentUfeCameraPath(&status);
            Ufe::Path ufeCameraPath = Ufe::PathString::path(ufeCameraPathString.asChar());
            bool isMayaCamera = ufeCameraPath.runTimeId() == UfeExtensions::getMayaRunTimeId();
            if (isMayaCamera) {
                if (_mayaHydraSceneIndex) {
                    params.camera = _mayaHydraSceneIndex->SetCameraViewport(camPath, _viewport);
                    if (vpDirty)
                        _mayaHydraSceneIndex->MarkSprimDirty(params.camera, HdCamera::DirtyParams);
                }
            }
        } else {
            TF_WARN(
                "MFrameContext::getCurrentCameraPath failure (%d): '%s'"
                "\nUsing viewport matrices.",
                int(status.statusCode()),
                status.errorString().asChar());
        }
    }

    _taskController->SetRenderParams(params);
    if (!params.camera.IsEmpty())
        _taskController->SetCameraPath(params.camera);

    // Default color in usdview.
    _taskController->SetSelectionColor(_globals.colorSelectionHighlightColor);
    _taskController->SetEnableSelection(_globals.colorSelectionHighlight);

    if (_globals.outlineSelectionWidth != 0.f) {
        _taskController->SetSelectionOutlineRadius(_globals.outlineSelectionWidth);
        _taskController->SetSelectionEnableOutline(true);
    } else
        _taskController->SetSelectionEnableOutline(false);

    _taskController->SetCollection(_renderCollection);

    // Update all registered plugin before render.
    for (auto& entry : _sceneIndexRegistry->GetRegistrations()) {
        entry.second->Update();
    }

    if (_isUsingHdSt) {
        auto  enableShadows = true;
        auto* lightParam = drawContext.getLightParameterInformation(
            0, MHWRender::MDrawContext::kFilteredIgnoreLightLimit);
        if (lightParam != nullptr) {
            MIntArray intVals;
            if (lightParam->getParameter(
                    MHWRender::MLightParameterInformation::kGlobalShadowOn, intVals)
                && intVals.length() > 0) {
                enableShadows = intVals[0] != 0;
            }
        }
        HdxShadowTaskParams shadowParams;
        shadowParams.cullStyle = HdCullStyleNothing;

        // The light & shadow parameters currently (19.11-20.08) are only used for tasks specific to
        // Storm
        _taskController->SetEnableShadows(enableShadows);
        _taskController->SetShadowParams(shadowParams);

#ifndef MAYAHYDRALIB_OIT_ENABLED
        // This is required for HdStorm to display transparency.
        // We should fix this upstream, so HdStorm can setup
        // all the required states.
        MayaHydraSetRenderGLState state;
#endif
        renderFrame(true);

    } else {
        renderFrame(true);
    }
    if (_mayaHydraSceneIndex) {
        _mayaHydraSceneIndex->PostFrame();
    }
    
    //Store as old display style
    _oldDisplayStyle = currentDisplayStyle;

    return MStatus::kSuccess;
}

MtohRenderOverride* MtohRenderOverride::_GetByName(TfToken rendererName)
{
    std::lock_guard<std::mutex> lock(_allInstancesMutex);
    for (auto* instance : _allInstances) {
        if (instance->_rendererDesc.rendererName == rendererName) {
            return instance;
        }
    }
    return nullptr;
}

void MtohRenderOverride::_SetRenderPurposeTags(const MayaHydraParams& delegateParams)
{
    TfTokenVector mhRenderTags = {HdRenderTagTokens->geometry};
    if (delegateParams.renderPurpose)
        mhRenderTags.push_back(HdRenderTagTokens->render);
    if (delegateParams.proxyPurpose)
        mhRenderTags.push_back(HdRenderTagTokens->proxy);
    if (delegateParams.guidePurpose)
        mhRenderTags.push_back(HdRenderTagTokens->guide);
    _taskController->SetRenderTags(mhRenderTags);
}

void MtohRenderOverride::_ClearMayaHydraSceneIndex()
{
    _renderIndexProxy->RemoveSceneIndex(_mayaHydraSceneIndex);
    _mayaHydraSceneIndex->RemoveCallbacksAndDeleteAdapters(); //This should be called before calling _sceneIndex.Reset(); which will call the destructor if the ref count reaches 0
    _mayaHydraSceneIndex.Reset();
}

void MtohRenderOverride::_InitHydraResources(const MHWRender::MDrawContext& drawContext)
{
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("MtohRenderOverride::_InitHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    _initializationAttempted = true;

    GlfContextCaps::InitInstance();
    _rendererPlugin
        = HdRendererPluginRegistry::GetInstance().GetRendererPlugin(_rendererDesc.rendererName);
    if (!_rendererPlugin)
        return;

    _renderDelegate = HdRendererPluginRegistry::GetInstance().CreateRenderDelegate(_rendererDesc.rendererName);
    if (!_renderDelegate)
        return;

    _renderIndex = HdRenderIndex::New(_renderDelegate.Get(), {&_hgiDriver});
    if (!_renderIndex)
        return;
    GetMayaHydraLibInterface().RegisterTerminalSceneIndex(_renderIndex->GetTerminalSceneIndex());

    _taskController = new HdxTaskController(
        _renderIndex,
        _ID.AppendChild(TfToken(TfStringPrintf(
            "_UsdImaging_%s_%p",
            TfMakeValidIdentifier(_rendererDesc.rendererName.GetText()).c_str(),
            this))));
    _taskController->SetEnableShadows(true);
    // Initialize the AOV system to render color for Storm
    if (_isUsingHdSt) {
        _taskController->SetRenderOutputs({ HdAovTokens->color });
    }

    MayaHydraInitData mhInitData(
        TfToken("MayaHydraSceneIndex"),
        _engine,
        _renderIndex,
        _rendererPlugin,
        _taskController,
        MAYA_NATIVE_ROOT,
        _isUsingHdSt
    );

    // Render index proxy sets up the Flow Viewport merging scene index, must
    // be created first, as it is required for:
    // - Selection scene index, which uses the Flow Viewport merging scene
    //   index as input.
    // - Maya scene producer, which needs the render index proxy to insert
    //   itself.

    _renderIndexProxy = std::make_shared<Fvp::RenderIndexProxy>(_renderIndex);

    _mayaHydraSceneIndex = MayaHydraSceneIndex::New(mhInitData, !_hasDefaultLighting);
    TF_VERIFY(_mayaHydraSceneIndex, "Maya Hydra scene index not found, check mayaHydra plugin installation.");
    
    VtValue fvpSelectionTrackerValue(_fvpSelectionTracker);
    _engine.SetTaskContextData(FvpTokens->fvpSelectionState, fvpSelectionTrackerValue);

    _mayaHydraSceneIndex->Populate();
    //Add the scene index as an input scene index of the merging scene index
    _renderIndexProxy->InsertSceneIndex(_mayaHydraSceneIndex, SdfPath::AbsoluteRootPath());
    
    if (!_sceneIndexRegistry) {
        _sceneIndexRegistry.reset(new MayaHydraSceneIndexRegistry(_renderIndexProxy));
    }
    
    //Create internal scene indices chain
    _inputSceneIndexOfFilteringSceneIndicesChain = _renderIndexProxy->GetMergingSceneIndex();

    //Put BlockPrimRemovalPropagationSceneIndex first as it can block/unblock the prim removal propagation on the whole scene indices chain
    _blockPrimRemovalPropagationSceneIndex = Fvp::BlockPrimRemovalPropagationSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _selection = std::make_shared<Fvp::Selection>();
    _selectionSceneIndex = Fvp::SelectionSceneIndex::New(_blockPrimRemovalPropagationSceneIndex, _selection);
    _selectionSceneIndex->SetDisplayName("Flow Viewport Selection Scene Index");
    _inputSceneIndexOfFilteringSceneIndicesChain = _selectionSceneIndex;

    _dirtyLeadObjectSceneIndex = MAYAHYDRA_NS_DEF::MhDirtyLeadObjectSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _inputSceneIndexOfFilteringSceneIndicesChain = _dirtyLeadObjectSceneIndex;

    // Set the initial selection onto the selection scene index.
    _selectionSceneIndex->ReplaceSelection(*Ufe::GlobalSelection::get());

    _CreateSceneIndicesChainAfterMergingSceneIndex(drawContext);
    
    if (auto* renderDelegate = _GetRenderDelegate()) {
        // Pull in any options that may have changed due file-open.
        // If the currentScene has defaultRenderGlobals we'll absorb those new settings,
        // but if not, fallback to user-defaults (current state) .
        const bool filterRenderer = true;
        const bool fallbackToUserDefaults = true;
        _globals.GlobalChanged(
            { _rendererDesc.rendererName, filterRenderer, fallbackToUserDefaults });
        _globals.ApplySettings(renderDelegate, _rendererDesc.rendererName);
    }
    auto tasks = _taskController->GetRenderingTasks();
    for (auto task : tasks) {
        if (std::dynamic_pointer_cast<HdxColorizeSelectionTask>(task)) {
            _backupFrameBufferWorkaround = true;
            break;
        }
    }

    _initializationSucceeded = true;
}

//When fullReset is true, we remove the data producer scene indices that apply to all viewports and the scene index registry where the usd stages have been loaded.
//It means you are doing a full reset of hydra such as when doing "File New".
//Use fullReset = false when you still want to see the previously registered data producer scene indices when using an hydra viewport.
void MtohRenderOverride::ClearHydraResources(bool fullReset)
{
    if (!_initializationAttempted) {
        return;
    }

    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES)
        .Msg("MtohRenderOverride::ClearHydraResources(%s)\n", _rendererDesc.rendererName.GetText());

    //We don't have any viewport using Hydra any more
    Fvp::ViewportInformationAndSceneIndicesPerViewportDataManager::Get().RemoveAllViewportsInformation();
    
    if (fullReset){
        //Remove the data producer scene indices that apply to all viewports
        Fvp::DataProducerSceneIndexInterfaceImp::get().ClearDataProducerSceneIndicesThatApplyToAllViewports();
        //Remove the scene index registry
        _sceneIndexRegistry.reset();
    }

    #ifdef CODE_COVERAGE_WORKAROUND
        // Leak the Maya scene index, as its base class HdRetainedSceneIndex
        // destructor crashes under Windows clang code coverage build.
        _mayaHydraSceneIndex->RemoveCallbacksAndDeleteAdapters();
        _mayaHydraSceneIndex.Reset();
    #else
       _ClearMayaHydraSceneIndex();
    #endif

    _displayStyleSceneIndex = nullptr;
    _pruneTexturesSceneIndex = nullptr;
    _defaultMaterialSceneIndex = nullptr;
    _currentlyTextured = false;
    _selectionSceneIndex.Reset();
    _selection.reset();
    _wireframeColorInterfaceImp.reset();
    _leadObjectPathTracker.reset();

    // Cleanup internal context data that keep references to data that is now
    // invalid.
    _engine.ClearTaskContextData();

    if (_taskController != nullptr) {
        delete _taskController;
        _taskController = nullptr;
    }

    if (_renderIndex != nullptr) {
        GetMayaHydraLibInterface().UnregisterTerminalSceneIndex(_renderIndex->GetTerminalSceneIndex());
#ifndef CODE_COVERAGE_WORKAROUND
        // Leak the render index, as its destructor crashes under Windows clang
        // code coverage build.
        delete _renderIndex;
#endif
        _renderIndex = nullptr;
    }

    if (_rendererPlugin != nullptr) {
        _renderDelegate = nullptr;
        HdRendererPluginRegistry::GetInstance().ReleasePlugin(_rendererPlugin);
        _rendererPlugin = nullptr;
    }

    //Decrease ref count on the render index proxy which owns the merging scene index at the end of this function as some previous calls may likely use it to remove some scene indices
    _renderIndexProxy.reset();

    _viewport = GfVec4d(0, 0, 0, 0);
    _initializationSucceeded = false;
    _initializationAttempted = false;
}

void MtohRenderOverride::_CreateSceneIndicesChainAfterMergingSceneIndex(const MHWRender::MDrawContext& drawContext)
{
    //This function is where happens the ordering of filtering scene indices that are after the merging scene index
    //We use as its input scene index : _inputSceneIndexOfFilteringSceneIndicesChain
    // Add display style scene index
    _lastFilteringSceneIndexBeforeCustomFiltering = _displayStyleSceneIndex =
            Fvp::DisplayStyleOverrideSceneIndex::New(_inputSceneIndexOfFilteringSceneIndicesChain);
    _displayStyleSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims don't use global refinement

    // Add texture disabling Scene Index
    _lastFilteringSceneIndexBeforeCustomFiltering = _pruneTexturesSceneIndex = 
    Fvp::PruneTexturesSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering);

    //Create default material data when switching to the default material in the viewport
    SdfPath defaultMaterialPath;
    if (_mayaHydraSceneIndex){
        defaultMaterialPath = _mayaHydraSceneIndex->GetDefaultMaterialPath();
        if (defaultMaterialPath.IsEmpty()){
            _mayaHydraSceneIndex->CreateMayaDefaultMaterialData();
            defaultMaterialPath = _mayaHydraSceneIndex->GetDefaultMaterialPath();
            TF_VERIFY(! defaultMaterialPath.IsEmpty());
        }
    }
    
    // Add default material scene index
    _lastFilteringSceneIndexBeforeCustomFiltering = _defaultMaterialSceneIndex = Fvp::DefaultMaterialSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, 
                                                                                defaultMaterialPath,
                                                                                _mayaHydraSceneIndex ? _mayaHydraSceneIndex->GetDefaultMaterialExclusionPaths(): SdfPathVector());

    const unsigned int currentDisplayStyle = drawContext.getDisplayStyle();
    const MFrameContext::WireOnShadedMode wireOnShadedMode = MFrameContext::wireOnShadedMode();//Get the user preference

    auto mergingSceneIndex = _renderIndexProxy->GetMergingSceneIndex();
    if(! _leadObjectPathTracker){
        _leadObjectPathTracker = std::make_shared<MAYAHYDRA_NS_DEF::MhLeadObjectPathTracker>(mergingSceneIndex, _dirtyLeadObjectSceneIndex);
    }

    if (! _wireframeColorInterfaceImp){
        _wireframeColorInterfaceImp = std::make_shared<MAYAHYDRA_NS_DEF::MhWireframeColorInterfaceImp>(_selection, _leadObjectPathTracker);
    }
    
    //Are we using Bounding Box display style ?
    if (currentDisplayStyle & MHWRender::MFrameContext::kBoundingBox){
        //Insert the bounding box filtering scene index which converts geometries into a bounding box using the extent attribute
        auto bboxSceneIndex  = Fvp::BboxSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, _wireframeColorInterfaceImp);
        bboxSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims are already converted by OGS
        _lastFilteringSceneIndexBeforeCustomFiltering = bboxSceneIndex;
    }
    else if (currentDisplayStyle & MHWRender::MFrameContext::kWireFrame){//Are we using wireframe somehow ?
        
        if( (currentDisplayStyle & MHWRender::MFrameContext::kGouraudShaded) || (currentDisplayStyle & MHWRender::MFrameContext::kTextured)){
            // Wireframe on top of shaded
            //Reduced quality
            if (MFrameContext::WireOnShadedMode::kWireFrameOnShadedReduced == wireOnShadedMode ){
                //Insert the reprselector filtering scene index which updates the repr selector on geometries
                auto reprSelectorSceneIndex = Fvp::ReprSelectorSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, 
                                       Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeOnSurface, _wireframeColorInterfaceImp);
                reprSelectorSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims are already converted by OGS
                _lastFilteringSceneIndexBeforeCustomFiltering = reprSelectorSceneIndex;
            } else {//Full quality
                //Should we support kWireFrameOnShadedNone and do not display any wireframe ?
                //Insert the reprselector filtering scene index which updates the repr selector on geometries
                auto reprSelectorSceneIndex = Fvp::ReprSelectorSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, 
                                       Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeOnSurfaceRefined, _wireframeColorInterfaceImp);
                reprSelectorSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims are already converted by OGS
                _lastFilteringSceneIndexBeforeCustomFiltering = reprSelectorSceneIndex;
            }
        }
        else{
                //wireframe only, not on top of shaded
                
                //Insert the reprselector filtering scene index which updates the repr selector on geometries
                auto reprSelectorSceneIndex = Fvp::ReprSelectorSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, 
                                       Fvp::ReprSelectorSceneIndex::RepSelectorType::WireframeRefined, _wireframeColorInterfaceImp);
                reprSelectorSceneIndex->addExcludedSceneRoot(MAYA_NATIVE_ROOT); // Maya native prims are already converted by OGS
                _lastFilteringSceneIndexBeforeCustomFiltering = reprSelectorSceneIndex;
            }
    }

    auto wfSi = TfDynamic_cast<Fvp::WireframeSelectionHighlightSceneIndexRefPtr>(Fvp::WireframeSelectionHighlightSceneIndex::New(_lastFilteringSceneIndexBeforeCustomFiltering, _selection, _wireframeColorInterfaceImp));
    wfSi->SetDisplayName("Flow Viewport Wireframe Selection Highlight Scene Index");
    
    // At time of writing, wireframe selection highlighting of Maya native data
    // is done by Maya at render item creation time, so avoid double wireframe
    // selection highlighting.
    wfSi->addExcludedSceneRoot(MAYA_NATIVE_ROOT);
    _lastFilteringSceneIndexBeforeCustomFiltering  = wfSi;
    
#ifdef CODE_COVERAGE_WORKAROUND
    Fvp::leakSceneIndex(_lastFilteringSceneIndexBeforeCustomFiltering);
#endif
}

void MtohRenderOverride::_RemovePanel(MString panelName)
{
    auto foundPanelCallbacks = _FindPanelCallbacks(panelName);
    if (foundPanelCallbacks != _renderPanelCallbacks.end()) {
        MMessage::removeCallbacks(foundPanelCallbacks->second);
        Fvp::ViewportInformationAndSceneIndicesPerViewportDataManager::Get().RemoveViewportInformation(std::string(panelName.asChar()));
        _renderPanelCallbacks.erase(foundPanelCallbacks);
    }

    if (_renderPanelCallbacks.empty()) {
        constexpr bool fullReset = false;
        ClearHydraResources(fullReset);
    }
}

void MtohRenderOverride::SelectionChanged(
    const Ufe::SelectionChanged& notification
)
{
    TF_DEBUG(FVP_APP_SELECTION_CHANGE)
        .Msg("MtohRenderOverride::SelectionChanged(Ufe::SelectionChanged) called.\n");

    if (!_initializationSucceeded) {
        return;
    }

    TF_AXIOM(_selectionSceneIndex);

    // Two considerations: 
    // 1) Reading from the Maya active selection list only returns
    //    Maya objects, so must read from the UFE selection.
    // 2) The UFE selection does not have Maya component selections.
    //    When we are ready to support these, must be read from the Maya
    //    selection.  A tricky aspect is that the UFE selection
    //    notification is sent before the Maya selection is ready, so
    //    reading the Maya selection must be done from the Maya selection
    //    changed callback, not the UFE selection changed callback.
    using SnOp = Ufe::SelectionCompositeNotification::Op;
    using SnSiPtr = Fvp::SelectionSceneIndexRefPtr;
    static auto appendSn = [](const SnOp& op, const SnSiPtr& si) {
        si->AddSelection(op.item->path());
    };
    static auto removeSn = [](const SnOp& op, const SnSiPtr& si) {
        si->RemoveSelection(op.item->path());
    };
    // FLOW_VIEWPORT_TODO  Support selection insert.  PPT, 19-Oct-2023
    static auto insertSn = [](const SnOp&, const SnSiPtr& si) {
        TF_WARN("Insert into selection not supported.");
    };
    static auto clearSn = [](const SnOp&, const SnSiPtr& si) {
        si->ClearSelection();
    };
    static auto replaceWithSn = [](const SnOp& op, const SnSiPtr& si) {
        si->ReplaceSelection(*Ufe::GlobalSelection::get());
    };
    static std::function<void(const SnOp&, const SnSiPtr&)> changeSn[] = {appendSn, removeSn, insertSn, clearSn, replaceWithSn};

    if (notification.opType() == 
        Ufe::SelectionChanged::SelectionCompositeNotification) {

        const auto& compositeNotification = notification.staticCast<Ufe::SelectionCompositeNotification>();

        for (const auto& op : compositeNotification) {
            changeSn[op.opType](op, _selectionSceneIndex);
        }
    }
    else {
        SnOp op(notification);

        changeSn[op.opType](op, _selectionSceneIndex);
    }


    // FLOW_VIEWPORT_TODO  Clarify new Flow Viewport selection tracker
    // architecture.  Here is where we would set the selection onto the
    // selection tracker, or trackers, if data provider plugins need to have
    // their own selection tracker.  The selection tracker makes the selection
    // and selection-derived data availabel to a selection task or selection
    // tasks through the task context data.  PPT, 18-Sep-2023
}

MHWRender::DrawAPI MtohRenderOverride::supportedDrawAPIs() const
{
    return MHWRender::kOpenGLCoreProfile | MHWRender::kOpenGL;
}

MStatus MtohRenderOverride::setup(const MString& destination)
{
    MStatus status;

    auto panelNameAndCallbacks = _FindPanelCallbacks(destination);
    if (panelNameAndCallbacks == _renderPanelCallbacks.end()) {
        // Install the panel callbacks
        MCallbackIdArray newCallbacks;

        auto id = MUiMessage::add3dViewDestroyMsgCallback(
            destination, _PanelDeletedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        id = MUiMessage::add3dViewRendererChangedCallback(
            destination, _RendererChangedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        id = MUiMessage::add3dViewRenderOverrideChangedCallback(
            destination, _RenderOverrideChangedCallback, this, &status);
        if (status) {
            newCallbacks.append(id);
        }

        _renderPanelCallbacks.emplace_back(destination, newCallbacks);
    }

    auto* renderer = MHWRender::MRenderer::theRenderer();
    if (renderer == nullptr) {
        return MStatus::kFailure;
    }

    if (_operations.empty()) {
        // Clear and draw pre scene elelments (grid not pushed into hydra)
        _operations.push_back(new MayaHydraPreRender("HydraRenderOverride_PreScene"));

        // The main hydra render
        // For the data server, This also invokes scene update then sync scene delegate after scene
        // update
        _operations.push_back(new MayaHydraRender("HydraRenderOverride_DataServer", this));

        // Draw post scene elements (cameras, CVs, shapes not pushed into hydra)
        _operations.push_back(new MayaHydraPostRender("HydraRenderOverride_PostScene"));

        // Draw HUD elements
        _operations.push_back(new MHWRender::MHUDRender());

        // Set final buffer options
        auto* presentTarget = new MHWRender::MPresentTarget("HydraRenderOverride_Present");
        presentTarget->setPresentDepth(true);
        presentTarget->setTargetBackBuffer(MHWRender::MPresentTarget::kCenterBuffer);
        _operations.push_back(presentTarget);
    }

    return MS::kSuccess;
}

MStatus MtohRenderOverride::cleanup()
{
    _currentOperation = -1;
    return MS::kSuccess;
}

bool MtohRenderOverride::startOperationIterator()
{
    _currentOperation = 0;
    return true;
}

MHWRender::MRenderOperation* MtohRenderOverride::renderOperation()
{
    if (_currentOperation >= 0 && _currentOperation < static_cast<int>(_operations.size())) {
        return _operations[_currentOperation];
    }
    return nullptr;
}

bool MtohRenderOverride::nextRenderOperation()
{
    return ++_currentOperation < static_cast<int>(_operations.size());
}

void MtohRenderOverride::_PopulateSelectionList(
    const HdxPickHitVector&          hits,
    const MHWRender::MSelectionInfo& selectInfo,
    MSelectionList&                  selectionList,
    MPointArray&                     worldSpaceHitPts, 
    bool&                            isOneMayaNodeInComponentsPickingMode)
{
    if (hits.empty() || !_mayaHydraSceneIndex || !_ufeSn) {
        return;
    }

    PickOutput pickOutput(selectionList, worldSpaceHitPts, _ufeSn);

     // Is the picked node in components selection mode ? If so it is in the hilite list
    MSelectionList hiliteList;
    MGlobal::getHiliteList(hiliteList);
    const bool hiliteListIsEmpty = hiliteList.isEmpty();

    MStatus status;
    for (const HdxPickHit& hit : hits) {
        PickInput pickInput(hit, selectInfo, hits.size() == 1u);
        auto pickHandler = _PickHandler(hit);
        if (!hiliteListIsEmpty && _IsMayaPickHandler(pickHandler)){
            // Maya does not create Hydra instances, so if the pick hit instancer
            // ID isn't empty, it's not a Maya pick hit.
            if (_mayaHydraSceneIndex && pickInput.pickHit.instancerId.IsEmpty() && _mayaHydraSceneIndex->IsPickedNodeInComponentsPickingMode(pickInput.pickHit)){
                isOneMayaNodeInComponentsPickingMode = true;
                return;
            }
        }
        pickHandler->handlePickHit(pickInput, pickOutput);
    }
}

bool MtohRenderOverride::_IsMayaPickHandler(const MtohRenderOverride::PickHandlerBase* pickHandler)const
{
    return pickHandler == _pickHandlers[0].get();
}

const MtohRenderOverride::PickHandlerBase*
MtohRenderOverride::_PickHandler(const HdxPickHit& pickHit) const
{
    // As of 19-Mar-2024, we only have two kinds of pick handlers, one for Maya
    // objects, the other for USD objects.  USD objects are generated by
    // mayaUsd proxy shape Maya nodes, which add one registration to the
    // MayaHydraSceneIndexRegistry per proxy shape node.  We use the trivial
    // strategy of choosing the USD pick handler if there is a registration
    // that matches the pick hit, otherwise the Maya pick handler is used.
    // This will need to be revised for extensibility.
    return _sceneIndexRegistry->GetSceneIndexRegistrationForRprim(pickHit.objectId) ? _pickHandlers[1].get() : _pickHandlers[0].get();
}

void MtohRenderOverride::_PickByRegion(
    HdxPickHitVector& outHits,
    const MMatrix& viewMatrix,
    const MMatrix& projMatrix,
    bool singlePick,
    const TfToken& geomSubsetsPickMode,
    bool pointSnappingActive,
    int view_x,
    int view_y,
    int view_w,
    int view_h,
    unsigned int sel_x,
    unsigned int sel_y,
    unsigned int sel_w,
    unsigned int sel_h)
{
    MMatrix adjustedProjMatrix;
    // Compute a pick matrix that, when it is post-multiplied with the projection matrix, will
    // cause the picking region to fill the entire viewport for OpenGL selection.
    {
        double center_x = sel_x + sel_w * 0.5;
        double center_y = sel_y + sel_h * 0.5;

        MMatrix pickMatrix;
        pickMatrix[0][0] = view_w / double(sel_w);
        pickMatrix[1][1] = view_h / double(sel_h);
        pickMatrix[3][0] = (view_w - 2.0 * (center_x - view_x)) / double(sel_w);
        pickMatrix[3][1] = (view_h - 2.0 * (center_y - view_y)) / double(sel_h);

        adjustedProjMatrix = projMatrix * pickMatrix;
    }

    // Set up picking params.
    HdxPickTaskContextParams pickParams;
    // Use the same size as selection region is enough to get all pick results.
    pickParams.resolution.Set(sel_w, sel_h);
    pickParams.pickTarget = HdxPickTokens->pickPrimsAndInstances;
    pickParams.resolveMode = singlePick ? HdxPickTokens->resolveNearestToCenter : HdxPickTokens->resolveUnique;
    pickParams.doUnpickablesOcclude = false;
    pickParams.viewMatrix.Set(viewMatrix.matrix);
    pickParams.projectionMatrix.Set(adjustedProjMatrix.matrix);
    pickParams.collection = _renderCollection;
    pickParams.outHits = &outHits;
    
    if (geomSubsetsPickMode == GeomSubsetsPickModeTokens->Faces) {
        pickParams.pickTarget = HdxPickTokens->pickFaces;
    }

    if (pointSnappingActive) {
        pickParams.pickTarget = HdxPickTokens->pickPoints;

        // Exclude selected Rprims to avoid self-snapping issue.
        pickParams.collection = _pointSnappingCollection;
        pickParams.collection.SetExcludePaths(_selectionSceneIndex->GetFullySelectedPaths());
    }

    // Execute picking tasks.
    HdTaskSharedPtrVector pickingTasks = _taskController->GetPickingTasks();
    VtValue               pickParamsValue(pickParams);
    _engine.SetTaskContextData(HdxPickTokens->pickParams, pickParamsValue);
    _engine.Execute(_taskController->GetRenderIndex(), &pickingTasks);
}

bool MtohRenderOverride::select(
    const MHWRender::MFrameContext&  frameContext,
    const MHWRender::MSelectionInfo& selectInfo,
    bool /*useDepth*/,
    MSelectionList& selectionList,
    MPointArray&    worldSpaceHitPts)
{
#ifdef MAYAHYDRA_PROFILERS_ENABLED
    MProfilingScope profilingScopeForEval(
        _profilerCategory,
        MProfiler::kColorD_L1,
        "MtohRenderOverride::select",
        "MtohRenderOverride::select");
#endif
    /*
    * There are 2 modes of selection picking for components in maya :
    * 1) You can be in components picking mode, this setting is global.This is detected in the function "isInComponentsPickingMode(selectInfo)"
    * 2) The second mode is when you right click on a node and choose a component to pick it (e.g : Face), this is
    * where we use the variable "isOneNodeInComponentsPickingMode" to detect that case, later in this function.
    */
    if (isInComponentsPickingMode(selectInfo)) {
        return false; //When being in components picking, returning false will use maya/OGS for components selection
    }

    MStatus status = MStatus::kFailure;

    MMatrix viewMatrix = frameContext.getMatrix(MHWRender::MFrameContext::kViewMtx, &status);
    if (status != MStatus::kSuccess)
        return false;

    MMatrix projMatrix = frameContext.getMatrix(MHWRender::MFrameContext::kProjectionMtx, &status);
    if (status != MStatus::kSuccess)
        return false;

    int view_x, view_y, view_w, view_h;
    status = frameContext.getViewportDimensions(view_x, view_y, view_w, view_h);
    if (status != MStatus::kSuccess)
        return false;

    unsigned int sel_x, sel_y, sel_w, sel_h;
    status = selectInfo.selectRect(sel_x, sel_y, sel_w, sel_h);
    if (status != MStatus::kSuccess)
        return false;

    HdxPickHitVector outHits;
    const bool singlePick = selectInfo.singleSelection();
    const TfToken geomSubsetsPickMode = GetGeomSubsetsPickMode();
    const bool pointSnappingActive = selectInfo.pointSnapping();
    if (pointSnappingActive)
    {
        int cursor_x, cursor_y;
        status = selectInfo.cursorPoint(cursor_x, cursor_y);
        if (status != MStatus::kSuccess)
            return false;

        // Performance optimization for large picking region.
        // The idea is start to do picking from a small region (width = 100), return the hit result if there's one.
        // Otherwise, increase the region size and do picking repeatly till the original region size is reached.
        static bool pickPerfOptEnabled = true;
        unsigned int curr_sel_w = 100;
        while (pickPerfOptEnabled && curr_sel_w < sel_w && outHits.empty())
        {
            unsigned int curr_sel_h = curr_sel_w * double(sel_h) / double(sel_w);

            unsigned int curr_sel_x = cursor_x > (int)curr_sel_w / 2 ? cursor_x - (int)curr_sel_w / 2 : 0;
            unsigned int curr_sel_y = cursor_y > (int)curr_sel_h / 2 ? cursor_y - (int)curr_sel_h / 2 : 0;

            _PickByRegion(outHits, viewMatrix, projMatrix, singlePick, geomSubsetsPickMode, pointSnappingActive,
                view_x, view_y, view_w, view_h, curr_sel_x, curr_sel_y, curr_sel_w, curr_sel_h);

            // Increase the size of picking region.
            curr_sel_w *= 2;
        }
    }

    // Pick from original region directly when point snapping is not active or no hit is found yet.
    if (outHits.empty())
    {
        _PickByRegion(outHits, viewMatrix, projMatrix, singlePick, geomSubsetsPickMode, pointSnappingActive,
            view_x, view_y, view_w, view_h, sel_x, sel_y, sel_w, sel_h);
    }

    if (pointSnappingActive) {
        // Find the hit nearest to the cursor point and use it for point snapping.
        int nearestHitIndex = -1;
        int cursor_x, cursor_y;
        if (selectInfo.cursorPoint(cursor_x, cursor_y)) {
            nearestHitIndex = GetNearestHitIndex(frameContext, outHits, cursor_x, cursor_y);
        }

        if (nearestHitIndex >= 0) {
            const HdxPickHit hit = outHits[nearestHitIndex];
            outHits.clear();
            outHits.push_back(hit);
        } else {
            outHits.clear();
        }
    }

    //isOneMayaNodeInComponentsPickingMode will be true if one of the picked node is in components picking mode
    bool isOneMayaNodeInComponentsPickingMode = false;
    _PopulateSelectionList(outHits, selectInfo, selectionList, worldSpaceHitPts, isOneMayaNodeInComponentsPickingMode);
    if (isOneMayaNodeInComponentsPickingMode){
        return false;//When being in components picking on a node, returning false will use maya/OGS for components selection
    }
    return true;
}

void MtohRenderOverride::_ClearHydraCallback(void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }
    constexpr bool fullReset = true;
    instance->ClearHydraResources(fullReset);
}

void MtohRenderOverride::_PlayblastingChanged(bool playBlasting, void* userData)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(userData);
    if (std::atomic_exchange(&instance->_playBlasting, playBlasting) == playBlasting)
        return;

    MStatus status;
    if (!playBlasting) {
        assert(instance->_timerCallback == 0 && "Callback exists");
        instance->_timerCallback
            = MTimerMessage::addTimerCallback(1.0f / 10.0f, _TimerCallback, instance, &status);
    } else {
        status = MMessage::removeCallback(instance->_timerCallback);
        instance->_timerCallback = 0;
    }
    CHECK_MSTATUS(status);
}

void MtohRenderOverride::_TimerCallback(float, float, void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (instance->_playBlasting || instance->_isConverged) {
        return;
    }

    std::lock_guard<std::mutex> lock(instance->_lastRenderTimeMutex);
    if ((std::chrono::system_clock::now() - instance->_lastRenderTime) < std::chrono::seconds(5)) {
        MGlobal::executeCommandOnIdle("refresh -f");
    }
}

void MtohRenderOverride::_PanelDeletedCallback(const MString& panelName, void* data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    instance->_RemovePanel(panelName);
}

void MtohRenderOverride::_RendererChangedCallback(
    const MString& panelName,
    const MString& oldRenderer,
    const MString& newRenderer,
    void*          data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    if (newRenderer != oldRenderer) {
        instance->_RemovePanel(panelName);
    }
}

void MtohRenderOverride::_RenderOverrideChangedCallback(
    const MString& panelName,
    const MString& oldOverride,
    const MString& newOverride,
    void*          data)
{
    auto* instance = reinterpret_cast<MtohRenderOverride*>(data);
    if (!TF_VERIFY(instance)) {
        return;
    }

    if (newOverride != instance->name()) {
        instance->_RemovePanel(panelName);
    }
}

// return true if we need to recreate the filtering scene indices chain because of a change, false otherwise.
bool MtohRenderOverride::_NeedToRecreateTheSceneIndicesChain(unsigned int currentDisplayStyle, bool xRayEnabled)
{
    if (areDifferentForOneOfTheseBits(currentDisplayStyle, _oldDisplayStyle, 
            MHWRender::MFrameContext::kGouraudShaded    | 
            MHWRender::MFrameContext::kWireFrame        | 
            MHWRender::MFrameContext::kBoundingBox      )
        ){
        return true;
    }
    
    if (_xRayEnabled != xRayEnabled){
        return true;
    }

    return false;
}

PXR_NAMESPACE_CLOSE_SCOPE
