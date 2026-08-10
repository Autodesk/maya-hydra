//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#include "mayaHydraSceneIndex.h"

#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/debugCodes.h>
#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/profilingUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraDataSource.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/rprim.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <maya/MDGMessage.h>
#include <maya/MDagMessage.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnSingleIndexedComponent.h>
#include <maya/MNodeClass.h>
#include <maya/MItDag.h>
#include <maya/MMaterial.h>
#include <maya/MObjectArray.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlug.h>
#include <maya/MString.h>
#include <ufe/pathString.h>

#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <flowViewport/fvpPurposeRenderTagsForPasses.h>
#include <flowViewport/selection/fvpDataProducersNodeHashCodeToSdfPathRegistry.h>
#include <flowViewport/selection/fvpPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <ufeExtensions/Global.h>

namespace {

// For some dag paths we use the shape to translate it to an Hydra path
bool _UseTheShapeDagPath(const MDagPath& dagpath)
{
    return MayaHydra::IsDagPathAnArnoldSkyDomeLight(dagpath)
        || MayaHydra::IsDagPathACamera(dagpath);
}

// Check if this dag path is registered in Sprims, which includes lights
// (such as the Arnold sky dome light) and cameras.
bool _IsDagPathRegisteredInHydraSPrims(const MDagPath& dagpath)
{
    return MayaHydra::IsDagPathALight(dagpath)
        || MayaHydra::IsDagPathACamera(dagpath);
}

} // namespace

PXR_NAMESPACE_OPEN_SCOPE

// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

TF_DEFINE_ENV_SETTING(
    MAYA_HYDRA_USE_MESH_ADAPTER,
    false,
    "Use mesh adapter instead of MRenderItem for Maya meshes.");

TF_DEFINE_ENV_SETTING(
    MAYA_HYDRA_PASS_NORMALS_TO_HYDRA,
    true,
    "Pass the normals to Hydra (works for both render item and mesh adapters).");

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (constantLighting)
);

SdfPath MayaHydraSceneIndex::_fallbackMaterial;

namespace {

TfToken GetPurposeRenderTag(const MRenderItem& ri)
{
    // This is where we sort the maya render items
    if (ri.type() == MHWRender::MRenderItem::RenderItemType::DecorationItem) {
        // Decoration item == Viewport UI element, send to secondary graphics pass
        return Fvp::secondaryGraphicsRenderTagToken;
    }
    // Default to beauty pass
    return HdRenderTagTokens->geometry;
}

bool filterMesh(const MRenderItem& ri, bool useMeshAdapter)
{
    return useMeshAdapter ?
                            // Filter our mesh render items, and let the mesh adapter handle Maya
                            // meshes.  The MRenderItem::name() for meshes is "StandardShadedItem",
                            // their MRenderItem::type() is InternalMaterialItem, but
                            // this type can also be used for other purposes, e.g. face groups, so
                            // using the name is more appropriate.
        (ri.name() == "StandardShadedItem")
                            : false;
}

bool isRenderItem_aiSkyDomeLightTriangleShape(const MRenderItem& renderItem)
{
    static const std::string _aiSkyDomeLight("aiSkyDomeLight");

    const auto prim = renderItem.primitive();
    MDagPath   dag = renderItem.sourceDagPath();
    if (dag.isValid() && (MHWRender::MGeometry::Primitive::kTriangles == prim)
        && (MHWRender::MRenderItem::DecorationItem == renderItem.type())) {
        std::string fpName = dag.fullPathName().asChar();
        if (fpName.find(_aiSkyDomeLight) != std::string::npos) {
            // This render item is a aiSkyDomeLight
            return true;
        }
    }

    return false;
}

template <class T> SdfPath toSdfPath(const T& src);
template <> inline SdfPath toSdfPath<MDagPath>(const MDagPath& dag)
{
    return DagPathToSdfPath(dag, false, false);
}
template <> inline SdfPath toSdfPath<MRenderItem>(const MRenderItem& ri)
{
    return RenderItemToSdfPath(ri, false);
}

template <class T> SdfPath maybePrepend(const T& src, const SdfPath& inPath);
template <> inline SdfPath maybePrepend<MDagPath>(const MDagPath&, const SdfPath& inPath)
{
    return inPath;
}
template <> inline SdfPath maybePrepend<MRenderItem>(const MRenderItem& ri, const SdfPath& inPath)
{
    // Prepend Maya path, for organization and readability.
    auto sdfDagPath = DagPathToSdfPath(ri.sourceDagPath(), false, false)
                          .MakeRelativePath(SdfPath::AbsoluteRootPath());
    if (sdfDagPath.IsEmpty()) {
        return inPath;
    }
    return sdfDagPath.AppendPath(inPath);
}

template <class T> SdfPath GetMayaPrimPath(const T& src)
{
    SdfPath mayaPath = toSdfPath(src);
    if (mayaPath.IsEmpty() || mayaPath.IsAbsoluteRootPath())
        return {};

    // We cannot append an absolute path (I.e : starting with "/")
    if (mayaPath.IsAbsolutePath()) {
        mayaPath = mayaPath.MakeRelativePath(SdfPath::AbsoluteRootPath());
    }

    mayaPath = maybePrepend(src, mayaPath);

    return mayaPath;
}

bool IsTexturedSkyDomeRenderItem(const SdfPath& name)
{
    static const std::string _aiTexturedSkyDome("texturedSkyDome");
    if (name.GetString().find(_aiTexturedSkyDome) != std::string::npos) {
        // This render item is an texturedDomeLighnt
        return true;
    }
    return false;
}

SdfPath _GetPrimPath(const SdfPath& base, const MDagPath& dg)
{
    return base.AppendPath(GetMayaPrimPath(dg));
}

SdfPath GetRenderItemMayaPrimPath(const MRenderItem& ri)
{
    if (ri.InternalObjectId() == 0)
        return {};

    return GetMayaPrimPath(ri);
}

SdfPath GetRenderItemPrimPath(const SdfPath& base, const MRenderItem& ri)
{
    return base.AppendPath(GetRenderItemMayaPrimPath(ri));
}

template <typename T, typename F> inline void _MapAdapter(F)
{
    // Do nothing.
}

template <typename T, typename M0, typename F, typename... M>
inline void _MapAdapter(F f, const M0& m0, const M&... m)
{
    for (auto& it : m0) {
        f(static_cast<T*>(it.second.get()));
    }
    _MapAdapter<T>(f, m...);
}

template <typename T, typename F> inline bool _FindAdapter(const SdfPath&, F) { return false; }

template <typename T, typename M0, typename F, typename... M>
inline bool _FindAdapter(const SdfPath& id, F f, const M0& m0, const M&... m)
{
    auto* adapterPtr = TfMapLookupPtr(m0, id);
    if (adapterPtr == nullptr) {
        return _FindAdapter<T>(id, f, m...);
    } else {
        f(static_cast<T*>(adapterPtr->get()));
        return true;
    }
}

template <typename T, typename F> inline bool _RemoveAdapter(const SdfPath&, F) { return false; }

template <typename T, typename M0, typename F, typename... M>
inline bool _RemoveAdapter(const SdfPath& id, F f, M0& m0, M&... m)
{
    auto* adapterPtr = TfMapLookupPtr(m0, id);
    if (adapterPtr == nullptr) {
        return _RemoveAdapter<T>(id, f, m...);
    } else {
        f(static_cast<T*>(adapterPtr->get()));
        m0.erase(id);
        return true;
    }
}

template <typename R> inline R _GetDefaultValue() { return {}; }

template <typename T, typename R, typename F> inline R _GetValue(const SdfPath&, F)
{
    return _GetDefaultValue<R>();
}

template <typename T, typename R, typename F, typename M0, typename... M>
inline R _GetValue(const SdfPath& id, F f, const M0& m0, const M&... m)
{
    auto* adapterPtr = TfMapLookupPtr(m0, id);
    if (adapterPtr == nullptr) {
        return _GetValue<T, R>(id, f, m...);
    } else {
        return f(static_cast<T*>(adapterPtr->get()));
    }
}

void _onDagNodeAdded(MObject& obj, void* clientData)
{
    reinterpret_cast<MayaHydraSceneIndex*>(clientData)->OnDagNodeAdded(obj);
}

void _onDagNodeRemoved(MObject& obj, void* clientData)
{
    reinterpret_cast<MayaHydraSceneIndex*>(clientData)->OnDagNodeRemoved(obj);
}

// duplicateInstanced does not add DAG nodes, so it never triggers _onDagNodeAdded; this is the
// only reliable signal that a shape gained a new instance path.
void _instanceAdded(MDagPath& child, MDagPath& parent, void* clientData)
{
    TF_UNUSED(parent);
    reinterpret_cast<MayaHydraSceneIndex*>(clientData)->AddNewInstance(child);
}

const MString defaultLightSet("defaultLightSet");

void _connectionChanged(MPlug& srcPlug, MPlug& destPlug, bool made, void* clientData)
{
    TF_UNUSED(made);
    const auto srcObj = srcPlug.node();
    if (!srcObj.hasFn(MFn::kTransform)) {
        return;
    }
    const auto destObj = destPlug.node();
    if (!destObj.hasFn(MFn::kSet)) {
        return;
    }
    if (srcPlug != MayaAttrs::dagNode::instObjGroups) {
        return;
    }
    MStatus           status;
    MFnDependencyNode destNode(destObj, &status);
    if (ARCH_UNLIKELY(!status)) {
        return;
    }
    if (destNode.name() != defaultLightSet) {
        return;
    }
    auto*    index = reinterpret_cast<MayaHydraSceneIndex*>(clientData);
    MDagPath dag;
    status = MDagPath::getAPathTo(srcObj, dag);
    if (ARCH_UNLIKELY(!status)) {
        return;
    }
    unsigned int shapesBelow = 0;
    dag.numberOfShapesDirectlyBelow(shapesBelow);
    for (auto i = decltype(shapesBelow) { 0 }; i < shapesBelow; ++i) {
        auto dagCopy = dag;
        dagCopy.extendToShapeDirectlyBelow(i);
        index->UpdateLightVisibility(dagCopy);
    }
}

SdfPath _GetMaterialPath(const SdfPath& base, const MObject& obj)
{
    MStatus           status;
    MFnDependencyNode node(obj, &status);
    if (!status) {
        return {};
    }
    const auto* chr = node.name().asChar();
    if (chr == nullptr || chr[0] == '\0') {
        return {};
    }

    std::string nodeName(chr);
    SanitizeNameForSdfPath(nodeName);
    return base.AppendPath(SdfPath(nodeName));
}

bool GetShadingEngineNode(const MRenderItem& ri, MObject& shadingEngineNode)
{
    shadingEngineNode = FindShadingEngine(ri.sourceDagPath(), ri.shadingComponent());
    return !shadingEngineNode.isNull();
}

std::mutex _adaptersToRecreateMutex;
std::mutex _adaptersToRebuildMutex;

class MayaPathMapper : public Fvp::PathMapper
{
public:
    MayaPathMapper(const MayaHydraSceneIndex& piSi)
        : _piSi(piSi)
    {
    }

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override
    {
        return _piSi.UfePathToPrimSelections(appPath);
    }

    std::string Name() const override { return "MayaPathMapper"; }

private:
    // Non-owning reference to prevent ownership cycle.
    const MayaHydraSceneIndex& _piSi;
};

} // namespace

MayaHydraSceneIndex::MayaHydraSceneIndex(MayaHydraInitData& initData, bool interactive)
    : _ID(initData.delegateID.AppendChild(
          TfToken(TfStringPrintf("_Index_MayaHydraSceneIndex_%p", this))))
    , _renderIndex(&initData.renderIndex)
    , _isHdSt(initData.isHdSt)
    , _rprimPath(initData.delegateID.AppendPath(SdfPath(std::string("rprims"))))
    , _sprimPath(initData.delegateID.AppendPath(SdfPath(std::string("sprims"))))
    , _materialPath(initData.delegateID.AppendPath(SdfPath(std::string("materials"))))
    , _mayaPathMapper(std::make_shared<MayaPathMapper>(*this))
    , _interactive(interactive)
{
    static std::once_flag once;
    std::call_once(once, []() {
        _fallbackMaterial = SdfPath::EmptyPath(); // Empty path for hydra fallback material
    });

    // Register a fallback path mapper in the path mapper registry.  Non-Maya
    // data models will have a Maya path segment prefix in their UFE path.
    // Maya data will not, and will be picked up by the fallback mapper.
    Fvp::PathMapperRegistry::Instance().SetFallbackMapper(_mayaPathMapper);
}

MayaHydraSceneIndex::~MayaHydraSceneIndex() { _Destroy(); }

void MayaHydraSceneIndex::_Destroy()
{
    // All Maya callbacks fire on the main thread only (no render thread here), but
    // _Destroy() removes adapters incrementally, so removing adapter N's callbacks can
    // synchronously re-enter adapter M (not yet reached) — same-thread reentrancy, not a
    // race. E.g. node-deletion during file close, or extension-attribute callbacks during
    // file read (see ShouldSkipHydraUpdates()). Set _isTearingDown so reentrant callers
    // no-op via ShouldSkipHydraUpdates()/IsTearingDown() instead of touching Hydra state
    // mid-teardown. A null _renderIndex outside teardown remains a TF_VERIFY error.
    _isTearingDown = true;

    for (auto callback : _callbacks) {
        MMessage::removeCallback(callback);
    }
    _callbacks.clear();

    _MapAdapter<MayaHydraAdapter>(
        [](MayaHydraAdapter* a) { a->RemoveCallbacks(); },
        _renderItemsAdapters,
        _shapeAdapters,
        _lightAdapters,
        _cameraAdapters,
        _materialAdapters,
        _customAdapters);

    _renderItemsAdapters.clear();
    _shapeAdapters.clear();
    _lightAdapters.clear();
    _materialAdapters.clear();
    _cameraAdapters.clear();
    _renderItemsAdaptersFast.clear();
    _customAdapters.clear();

    // Unregister the fallback path mapper.
    Fvp::PathMapperRegistry::Instance().SetFallbackMapper(nullptr);
}

void MayaHydraSceneIndex::UpdateRenderItems(const MDataServerOperation::MViewportScene& scene)
{
    MH_PROFILE_FUNCTION();

    // First loop to get rid of removed items
    constexpr int kInvalidId = 0;
    for (size_t i = 0; i < scene.mRemovalCount; i++) {
        int fastId = scene.mRemovals[i];
        if (fastId == kInvalidId)
            continue;
        MayaHydraRenderItemAdapterPtr ria = nullptr;
        if (_GetRenderItem(fastId, ria)) {
            _RemoveRenderItem(ria);
        }
        // The removal list can contain duplicate fastIds.  After the first
        // removal succeeds, subsequent duplicates will not be found — skip
        // them.
        if (ria == nullptr) {
            continue;
        }
    }

    // Coalesce DirtyPrims notifications produced per render item.
    const Fvp::DirtyNotifier::DirtyBatchGuard dirtyBatchGuard(*this);

    // My version, does minimal update
    // This loop could, in theory, be parallelized.  Unclear how large the gains would be, but maybe
    // nothing to lose unless there is some internal contention in USD.
    for (size_t i = 0; i < scene.mCount; i++) {
        auto flags = scene.mFlags[i];
        if (flags == 0) {
            continue;
        }

        auto& ri = *scene.mItems[i];

        // ProxyGeometryItems are a special type of dummy render item created internally by Maya
        // to implement and handle MPxDrawOverride. We do not need to translate these to Hydra.
        const MString riName = ri.name();
        if (riName == "ProxyGeometryItem") {
            continue;
        }

        // VP2 image planes emit a DepthPrepass render item that uses a special
        // shader writing only to the depth buffer with alpha-tested discard.
        // Hydra has no equivalent; rendering it as a regular textured mesh
        // creates a second overlapping layer that causes visual artifacts.
        if (riName == "imagePlane_ColorImage_DepthPrepass") {
            continue;
        }

        // Meshes can optionally be handled by the mesh adapter, rather than by
        // render items.
        if (filterMesh(ri, useMeshAdapter())) {
            continue;
        }

        int                           fastId = ri.InternalObjectId();
        MayaHydraRenderItemAdapterPtr ria = nullptr;
        if (!_GetRenderItem(fastId, ria)) {
            const SdfPath slowId = _GetRenderItemPrimPath(ri);

            // Maya/MtoA adds texturedSkyDome mesh object for VP2.
            // We do not want that to be translated to Hydra
            if (slowId.IsEmpty() || IsTexturedSkyDomeRenderItem(slowId)) {
                continue;
            }
            // MAYA-128021: We do not currently support maya instances.
            MDagPath dagPath(ri.sourceDagPath());
            ria = std::make_shared<MayaHydraRenderItemAdapter>(
                dagPath, slowId, fastId, this, ri, GetPurposeRenderTag(ri));

            // HYDRA-1992 : Order is important here. Downstream scene indices
            // might do some operations based on PrimsAdded notifications, which
            // we send once we call Populate(). These operations might loop back
            // into the render item adapter and this scene index, so it needs to
            // be setup properly.
            _AddRenderItem(ria);
            ria->Populate();
            ria->CreateCallbacks(); // Handle custom attribute changes

            // Update the render item adapter if this render item is an aiSkydomeLight shape
            ria->SetIsRenderITemAnaiSkydomeLightTriangleShape(
                isRenderItem_aiSkyDomeLightTriangleShape(ri));
        }

        // _GetRenderItemMaterial is expensive: it ultimately calls
        // MFnDagNode::getConnectedSetsAndMembers, which walks the Maya DG for
        // every set the shape belongs to. That cost is wasted on frames where
        // nothing material-related has changed (the common animation case).
        if (flags & MDataServerOperation::MViewportScene::MVS_changedEffect) {
            SdfPath material;
            MObject shadingEngineNode;
            if (!_GetRenderItemMaterial(ri, material, shadingEngineNode)) {
                if (material != kInvalidMaterial) {
                    CreateMaterial(material, shadingEngineNode);
                }
            }

            ria->SetMaterial(material);
        }

        MColor wireframeColor;

        MDagPath dagPath = ri.sourceDagPath();
        if (dagPath.isValid()) {
            wireframeColor = MGeometryUtilities::wireframeColor(
                dagPath); // This is a color managed VP2 color, it will need to be unmanaged at some
            // point
        }

        // Call UpdateTransform before UpdateFromDelta, as UpdateTransform
        // updates the stored transform value, and UpdateFromDelta sends out
        // PrimsDirtied notifications. If an observer receiving the PrimsDirtied 
        // notifications pulls on the transform, it needs to be up to date.
        if (flags & MDataServerOperation::MViewportScene::MVS_changedMatrix) {
            ria->UpdateTransform(ri);
        }
        const MayaHydraRenderItemAdapter::UpdateFromDeltaData data(ri, flags, wireframeColor);
        ria->UpdateFromDelta(data);
    }
}

void MayaHydraSceneIndex::Populate()
{
    MH_PROFILE_FUNCTION();

    MayaHydraAdapterRegistry::LoadAllPlugin();

    MStatus status;
    MItDag  dagIt(MItDag::kDepthFirst);
    dagIt.traverseUnderWorld(true);
    if (useMeshAdapter()) {
        for (; !dagIt.isDone(); dagIt.next()) {
            MDagPath path;
            dagIt.getPath(path);
            InsertDag(path);
        }
    } else {
        for (; !dagIt.isDone(); dagIt.next()) {
            MObject node = dagIt.currentItem(&status);
            if (status != MS::kSuccess)
                continue;
            OnDagNodeAdded(node);
        }
    }

    auto id = MDGMessage::addNodeAddedCallback(_onDagNodeAdded, "dagNode", this, &status);
    if (status) {
        _callbacks.push_back(id);
    }
    id = MDGMessage::addNodeRemovedCallback(_onDagNodeRemoved, "dagNode", this, &status);
    if (status) {
        _callbacks.push_back(id);
    }
    id = MDGMessage::addConnectionCallback(_connectionChanged, this, &status);
    if (status) {
        _callbacks.push_back(id);
    }
    if (useMeshAdapter()) {
        // Catches instance paths added via duplicateInstanced, which getAllPathsTo()-based
        // detection previously had to poll for every frame in FlushPendingUpdates().
        id = MDagMessage::addInstanceAddedCallback(_instanceAdded, this, &status);
        if (status) {
            _callbacks.push_back(id);
        }
    }
}

VtValue MayaHydraSceneIndex::GetMaterialResource(const SdfPath& id)
{
    if (id == _fallbackMaterial) {
        return MayaHydraMaterialAdapter::GetPreviewMaterialResource(id);
    }

    auto ret = _GetValue<MayaHydraMaterialAdapter, VtValue>(
        id,
        [](MayaHydraMaterialAdapter* a) -> VtValue { return a->GetMaterialResource(); },
        _materialAdapters);

    // For PRMan lights, the material network is stored in the light adapter
    if (ret.IsEmpty()) {
        ret = _GetValue<MayaHydraLightAdapter, VtValue>(
            id,
            [](MayaHydraLightAdapter* a) -> VtValue { return a->GetLightMaterialNetwork(); },
            _lightAdapters
        );
    }

    return ret.IsEmpty() ? MayaHydraMaterialAdapter::GetPreviewMaterialResource(id) : ret;
}

Fvp::PrimSelections MayaHydraSceneIndex::UfePathToPrimSelections(const Ufe::Path& appPath) const
{
    TF_DEBUG(MAYAHYDRALIB_SCENE_INDEX)
        .Msg(
            "MayaHydraSceneIndex::UfePathToPrimSelections(const Ufe::Path& %s) called.\n",
            Ufe::PathString::string(appPath).c_str());

    // We only handle Maya objects, so if the UFE path is not a Maya object,
    // early out with failure.
    if (appPath.runTimeId() != UfeExtensions::getMayaRunTimeId()) {
        return {};
    }

    // Not the best implementation performance-wise, as ufeToDagPath converts
    // the UFE path to a string, then does a Dag path lookup with the string.

    auto       dagPath = UfeExtensions::ufeToDagPath(appPath);
    if (!dagPath.isValid()) {
        TF_WARN(
            "MayaHydraSceneIndex::UfePathToPrimSelections: Could not convert UFE path %s to a valid "
            "Maya DAG path.",
            Ufe::PathString::string(appPath).c_str());
        return {};
    }

    const bool extendToShape = _UseTheShapeDagPath(
        dagPath); // For Hydra some prims, we need to use the shape dag path not the transform, as
                  // this is what gets translated to an hydra path
    const bool isSprim = _IsDagPathRegisteredInHydraSPrims(dagPath);

    MDagPath shapeDagPath(dagPath);
    shapeDagPath.extendToShape();

    // Check if this Maya node has a special path mapper associated with it.
    const Ufe::PathSegment seg = UfeExtensions::dagPathToUfePathSegment(shapeDagPath);
    if (!seg.empty()) {
        const Ufe::Path shapeAppPath { seg };
        const auto&     pmr = Fvp::PathMapperRegistry::Instance();
        if (pmr.HasMapper(shapeAppPath)) {
            return pmr.UfePathToPrimSelections(shapeAppPath);
        }
    }

    const SdfPath primPath = GetPrimPath((extendToShape) ? shapeDagPath : dagPath, isSprim);

    TF_DEBUG(MAYAHYDRALIB_SCENE_INDEX)
        .Msg("    mapped to scene index path %s.\n", primPath.GetText());

    return Fvp::PrimSelections({ Fvp::PrimSelection { primPath } });
}

SdfPath MayaHydraSceneIndex::SetCameraViewport(const MDagPath& camPath, const GfVec4d& viewport)
{
    const SdfPath camID = GetPrimPath(camPath, true);
    auto&&        cameraAdapter = TfMapLookupPtr(_cameraAdapters, camID);
    if (cameraAdapter) {
        (*cameraAdapter)->SetViewport(viewport);
        return camID;
    }
    return {};
}

SdfPath MayaHydraSceneIndex::GetDelegateID(TfToken name) { return _ID; }

MayaHydraSceneIndex::LightDagPathMap MayaHydraSceneIndex::GetGlobalLightPaths() const
{
    LightDagPathMap allLightPaths;
    allLightPaths.reserve(_lightAdapters.size());
    // By the time this function is called _lightAdapters should already have been populated
    // with both Maya and Arnold light adapters. The adapters contain the DagPath information
    // we store it here in unordered_map for fast retrieval
    for (const auto& entry : _lightAdapters)
        allLightPaths.emplace(
            entry.second->GetDagPath().fullPathName().asChar(), entry.second->GetDagPath());
    return allLightPaths;
}

void MayaHydraSceneIndex::FlushPendingUpdates()
{
    if (_isTearingDown) {
        return;
    }

    _renderCollectionChanged = false;

    if (!_materialTagsChanged.empty()) {
        if (IsHdSt()) {
            for (const auto& id : _materialTagsChanged) {
                if (_GetValue<MayaHydraMaterialAdapter, bool>(
                        id,
                        [](MayaHydraMaterialAdapter* a) { return a->UpdateMaterialTag(); },
                        _materialAdapters)) {
                    HdRenderIndex* renderIndex = GetRenderIndexPtr();
                    for (const auto& rprimId : renderIndex->GetRprimIds()) {
                        const auto* rprim = renderIndex->GetRprim(rprimId);
                        if (rprim != nullptr && rprim->GetMaterialId() == id) {
                            RebuildAdapterOnIdle(
                                rprim->GetId(), MayaHydraSceneIndex::RebuildFlagPrim);
                        }
                    }
                }
            }
        }
        _materialTagsChanged.clear();
    }

    if (!_lightsToAdd.empty()) {
        for (auto& lightToAdd : _lightsToAdd) {
            MDagPath dag;
            MStatus  status = MDagPath::getAPathTo(lightToAdd.first, dag);
            if (!status) {
                return;
            }
            CreateLightAdapter(dag);
        }
        _lightsToAdd.clear();
    }

    if (!_camerasToAdd.empty()) {
        for (auto& cameraToAdd : _camerasToAdd) {
            MDagPath dag;
            MStatus  status = MDagPath::getAPathTo(cameraToAdd.first, dag);
            if (!status) {
                return;
            }
            CreateCameraAdapter(dag);
        }
        _camerasToAdd.clear();
    }

    if (useMeshAdapter()) {
        for (const auto& obj : _addedNodes) {
            if (obj.isNull()) {
                continue;
            }
            MDagPath dag;
            MStatus  status = MDagPath::getAPathTo(obj, dag);
            if (!status) {
                return;
            }
            // We need to check if there is an instanced shape below this dag
            // and insert it as well, because they won't be inserted.
            if (dag.hasFn(MFn::kTransform)) {
                const auto childCount = dag.childCount();
                for (auto child = decltype(childCount) { 0 }; child < childCount; ++child) {
                    auto dagCopy = dag;
                    dagCopy.push(dag.child(child));
                    if (dagCopy.isInstanced() && dagCopy.instanceNumber() > 0) {
                        AddNewInstance(dagCopy);
                    }
                }
            } else {
                InsertDag(dag);
            }
        }
        _addedNodes.clear();
    }

    if (!_customNodesToAdd.empty()) {
        for (const auto& obj : _customNodesToAdd) {
            if (obj.isNull()) {
                continue;
            }
            MDagPath dag;
            MStatus  status = MDagPath::getAPathTo(obj, dag);
            if (!status) {
                continue;
            }
            if (dag.hasFn(MFn::kTransform)) {
                continue;
            }
            MFnDagNode dagNode(dag);
            if (dagNode.isIntermediateObject()) {
                continue;
            }
            if (dag.isInstanced() && dag.instanceNumber() > 0) {
                continue;
            }
            CreateCustomAdapter(dag);
        }
        _customNodesToAdd.clear();
    }

    // We don't need to rebuild something that's already being recreated.
    // Since we have a few elements, linear search over vectors is going to
    // be okay.
    // Block for the lifetime of _adaptersToRecreateMutex and _adaptersToRebuildMutex
    {
        std::lock_guard<std::mutex> lockRecreate(_adaptersToRecreateMutex);
        std::lock_guard<std::mutex> lockRebuild(_adaptersToRebuildMutex);

        if (!_adaptersToRecreate.empty()) {
            for (const auto& it : _adaptersToRecreate) {
                RecreateAdapter(std::get<0>(it), std::get<1>(it));

                for (auto itr = _adaptersToRebuild.begin(); itr != _adaptersToRebuild.end();
                     ++itr) {
                    if (std::get<0>(it) == std::get<0>(*itr)) {
                        _adaptersToRebuild.erase(itr);
                        break;
                    }
                }
            }
            _adaptersToRecreate.clear();
        }
    }

    // Block for the lifetime of _adaptersToRebuildMutex
    {
        std::lock_guard<std::mutex> lock(_adaptersToRebuildMutex);
        if (!_adaptersToRebuild.empty()) {
            for (const auto& it : _adaptersToRebuild) {
                _FindAdapter<MayaHydraAdapter>(
                    std::get<0>(it),
                    [&](MayaHydraAdapter* a) {
                        if (std::get<1>(it) & MayaHydraSceneIndex::RebuildFlagCallbacks) {
                            a->RemoveCallbacks();
                            a->CreateCallbacks();
                        }
                        if (std::get<1>(it) & MayaHydraSceneIndex::RebuildFlagPrim) {
                            a->RemovePrim();
                            a->Populate();
                        }
                    },
                    _shapeAdapters,
                    _lightAdapters,
                    _cameraAdapters,
                    _materialAdapters,
                    _customAdapters);
            }
            _adaptersToRebuild.clear();
        }
    }
    if (!IsHdSt()) {
        return;
    }
}

void MayaHydraSceneIndex::InsertPrim(
    MayaHydraAdapter* adapter,
    const TfToken&    typeId,
    const SdfPath&    id)
{
    auto dataSource = MayaHydraDataSource::New(id, typeId, this, adapter);

    // We are a retained scene index, which uses an SdfPathTable to store
    // prims:
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/10b62439e9242a55101cf8b200f2c7e02420e1b0/pxr/imaging/hd/retainedSceneIndex.h#L107
    // And SdfPathTable inserts missing ancestors on inserting a prim:
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/10b62439e9242a55101cf8b200f2c7e02420e1b0/pxr/usd/sdf/pathTable.h#L53
    // Unfortunately, inserted ancestors are inserted with the default mapped
    // type
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/10b62439e9242a55101cf8b200f2c7e02420e1b0/pxr/usd/sdf/pathTable.h#L722
    // which for HdSceneIndexPrim is an invalid prim with a null data source.
    // Therefore, insert missing ancestors ourselves, with a non-null data
    // source and empty type.
    _AddPrimAncestors(id);
    AddPrims({ { id, typeId, dataSource } });

    _renderCollectionChanged = true;
}

void MayaHydraSceneIndex::_AddPrimAncestors(const SdfPath& path)
{
    const auto& parentPath = path.GetParentPath();
    if (!GetPrim(parentPath).dataSource) {
        // Add a parent prim with an empty type and an empty data source, and
        // recurse up to the next ancestor level.
        AddPrims({ { parentPath, TfToken(), HdRetainedContainerDataSource::New() } });
        _AddPrimAncestors(parentPath);
    }
}

void MayaHydraSceneIndex::_RemoveEmptyAncestors(const SdfPath& path)
{
    const auto& parentPath = path.GetParentPath();
    if (parentPath == _rprimPath || parentPath == _sprimPath || parentPath == _materialPath) {
        return;
    }
    auto parentPrim = GetPrim(parentPath);
    if (parentPrim.dataSource && parentPrim.primType.IsEmpty()) {
        auto childPaths = GetChildPrimPaths(parentPath);
        if (childPaths.empty()) {
            RemovePrims({ parentPath });
            _RemoveEmptyAncestors(parentPath);
        }
    }
}

void MayaHydraSceneIndex::RemovePrim(const SdfPath& id)
{
    RemovePrims({ id });

    _RemoveEmptyAncestors(id);

    _renderCollectionChanged = true;
}

void MayaHydraSceneIndex::SetParams(const MayaHydraParams& params)
{
    const auto& oldParams = GetParams();
    if (oldParams.displaySmoothMeshes != params.displaySmoothMeshes) {
        // I couldn't find any other way to turn this on / off.
        // I can't convert HdRprim to HdMesh easily and no simple way
        // to get the type of the HdRprim from the render index.
        // If we want to allow creating multiple rprims and returning an id
        // to a subtree, we need to use the HasType function and the mark dirty
        // from each adapter.
        _MapAdapter<MayaHydraRenderItemAdapter>(
            [](MayaHydraRenderItemAdapter* a) {
                if (a->HasType(HdPrimTypeTokens->mesh)) {
                    MayaHydra::DirtyNotifier(a).dirtyMeshTopology();
                }
            },
            _renderItemsAdapters);
        _MapAdapter<MayaHydraDagAdapter>(
            [](MayaHydraDagAdapter* a) {
                if (a->HasType(HdPrimTypeTokens->mesh)) {
                    MayaHydra::DirtyNotifier(a).dirtyMeshTopology();
                }
            },
            _shapeAdapters);
    }
    if (oldParams.motionSampleStart != params.motionSampleStart
        || oldParams.motionSampleEnd != params.motionSampleEnd) {
        _MapAdapter<MayaHydraRenderItemAdapter>(
            [](MayaHydraRenderItemAdapter* a) {
                if (a->HasType(HdPrimTypeTokens->mesh) || a->HasType(HdPrimTypeTokens->basisCurves)
                    || a->HasType(HdPrimTypeTokens->points)) {
                    a->InvalidateTransform();
                    MayaHydra::DirtyNotifier(a).dirtyPoints().dirtyTransform();
                }
            },
            _renderItemsAdapters);
        _MapAdapter<MayaHydraDagAdapter>(
            [](MayaHydraDagAdapter* a) {
                a->InvalidateTransform();
                {
                    MayaHydra::DirtyNotifier notifier(a);
                    if (a->HasType(HdPrimTypeTokens->mesh)) {
                        notifier.dirtyPoints();
                    } else if (a->HasType(HdPrimTypeTokens->camera)) {
                        notifier.dirtyCameraParams();
                    }
                    notifier.dirtyTransform();
                }
                if (a->IsInstanced()) {
                    Fvp::DirtyNotifier(*a->GetMayaHydraSceneIndex(), a->GetInstancerID())
                        .dirtyTransform();
                }
            },
            _shapeAdapters,
            _lightAdapters,
            _cameraAdapters,
            _customAdapters);
    }
    // We need to trigger rebuilding shaders.
    if (oldParams.textureMemoryPerTexture != params.textureMemoryPerTexture) {
        _MapAdapter<MayaHydraMaterialAdapter>(
            [](MayaHydraMaterialAdapter* a) {
                MayaHydra::DirtyNotifier(a).dirtyMaterial();
            },
            _materialAdapters);
    }
    if (oldParams.maximumShadowMapResolution != params.maximumShadowMapResolution) {
        _MapAdapter<MayaHydraLightAdapter>(
            [](MayaHydraLightAdapter* a) {
                MayaHydra::DirtyNotifier(a).dirtyLightParams();
            },
            _lightAdapters);
    }

    _params = params;
}

SdfPath MayaHydraSceneIndex::GetMaterialId(const SdfPath& id)
{
    auto result = TfMapLookupPtr(_renderItemsAdapters, id);
    if (result != nullptr) {
        auto& renderItemAdapter = *result;

        auto& material = renderItemAdapter->GetMaterial();
        // Check if this render item is a wireframe primitive
        if (MHWRender::MGeometry::Primitive::kLines == renderItemAdapter->GetPrimitive()
            || MHWRender::MGeometry::Primitive::kLineStrip == renderItemAdapter->GetPrimitive()) {
            return _fallbackMaterial;
        }

        if (material == kInvalidMaterial) {
            return _fallbackMaterial;
        }

        if (TfMapLookupPtr(_materialAdapters, material) != nullptr) {
            return material;
        }
    }

    if (useMeshAdapter()) {
        auto shapeAdapter = TfMapLookupPtr(_shapeAdapters, id);
        if (shapeAdapter == nullptr) {
            return _fallbackMaterial;
        }
        auto material = shapeAdapter->get()->GetMaterial();
        if (material == MObject::kNullObj) {
            return _fallbackMaterial;
        }
        auto materialId = GetMaterialPath(material);
        if (TfMapLookupPtr(_materialAdapters, materialId) != nullptr) {
            return materialId;
        }

        return CreateMaterial(materialId, material) ? materialId : _fallbackMaterial;
    }

    return _fallbackMaterial;
}

HdMeshTopology MayaHydraSceneIndex::GetMeshTopology(const SdfPath& id)
{
    return _GetValue<MayaHydraAdapter, HdMeshTopology>(
        id,
        [](MayaHydraAdapter* a) -> HdMeshTopology { return a->GetMeshTopology(); },
        _shapeAdapters,
        _renderItemsAdapters);
}

HdBasisCurvesTopology MayaHydraSceneIndex::GetBasisCurvesTopology(const SdfPath& id)
{
    return _GetValue<MayaHydraAdapter, HdBasisCurvesTopology>(
        id,
        [](MayaHydraAdapter* a) -> HdBasisCurvesTopology { return a->GetBasisCurvesTopology(); },
        _shapeAdapters,
        _renderItemsAdapters);
}

void MayaHydraSceneIndex::RemoveAdapter(const SdfPath& id)
{
    if (_isTearingDown) {
        return;
    }

    if (!_RemoveAdapter<MayaHydraAdapter>(
            id,
            [](MayaHydraAdapter* a) {
                a->RemoveCallbacks();
                a->RemovePrim();
            },
            _renderItemsAdapters,
            _shapeAdapters,
            _lightAdapters,
            _cameraAdapters,
            _materialAdapters,
            _customAdapters)) {
        TF_WARN("MayaHydraSceneIndex::RemoveAdapter(%s) -- Adapter does not exists", id.GetText());
    }
}

void MayaHydraSceneIndex::RecreateAdapterOnIdle(const SdfPath& id, const MObject& obj)
{
    if (_isTearingDown) {
        return;
    }

    std::lock_guard<std::mutex> lock(_adaptersToRecreateMutex);

    // We expect this to be a small number of objects, so using a simple linear
    // search and a vector is generally a good choice.
    for (auto& it : _adaptersToRecreate) {
        if (std::get<0>(it) == id) {
            std::get<1>(it) = obj;
            return;
        }
    }
    _adaptersToRecreate.emplace_back(id, obj);
}

bool MayaHydraSceneIndex::_GetRenderItem(int fastId, MayaHydraRenderItemAdapterPtr& ria)
{
    // Using SdfPath as the hash table key is extremely slow.  The cost appears to be GetPrimPath,
    // which would depend on MdagPath, which is a wrapper on TdagPath.  TdagPath is a very slow
    // class and best to avoid in any performance- critical area. Simply workaround for the
    // prototype is an additional lookup index based on InternalObjectID.  Long term goal would be
    // that the plug-in rarely, if ever, deals with TdagPath.
    MayaHydraRenderItemAdapterPtr* result = TfMapLookupPtr(_renderItemsAdaptersFast, fastId);

    if (result != nullptr) {
        // adapter already exists, return it
        ria = *result;
        return true;
    }

    return false;
}

void MayaHydraSceneIndex::_AddRenderItem(const MayaHydraRenderItemAdapterPtr& ria)
{
    const SdfPath& primPath = ria->GetID();
    _renderItemsAdaptersFast.insert({ ria->GetFastID(), ria });
    _renderItemsAdapters.insert({ primPath, ria });
}

void MayaHydraSceneIndex::_RemoveRenderItem(const MayaHydraRenderItemAdapterPtr& ria)
{
    const SdfPath& primPath = ria->GetID();
    _renderItemsAdaptersFast.erase(ria->GetFastID());
    _renderItemsAdapters.erase(primPath);
}

void MayaHydraSceneIndex::GetLightedPrimPaths(SdfPathVector& lightedPrimPaths)
{
    _MapAdapter<MayaHydraAdapter>(
        [&](MayaHydraAdapter* a) {
            if (a->Illuminated()) {
                lightedPrimPaths.emplace_back(a->GetID());
            }
        },
        _renderItemsAdapters,
        _shapeAdapters);
}

bool MayaHydraSceneIndex::_GetRenderItemMaterial(
    const MRenderItem& ri,
    SdfPath&           material,
    MObject&           shadingEngineNode)
{
    if (MHWRender::MGeometry::Primitive::kLines == ri.primitive()
        || MHWRender::MGeometry::Primitive::kLineStrip == ri.primitive()) {
        material = _fallbackMaterial; // Use fallbackMaterial + constantLighting + displayColor
        return true;
    }

    // Image planes use a dedicated material adapter that reads the imageName
    // attribute directly, rather than going through shading engine lookup.
    MDagPath dagPath = ri.sourceDagPath();
    if (dagPath.isValid() && dagPath.node().hasFn(MFn::kImagePlane)) {
        material = GetMaterialPath(dagPath.node());
        if (TfMapLookupPtr(_materialAdapters, material) != nullptr) {
            return true;
        }
        shadingEngineNode = dagPath.node();
        return false;
    }

    if (GetShadingEngineNode(ri, shadingEngineNode))
    // Else try to find associated material node if this is a material shader.
    // NOTE: The existing maya material support in hydra expects a shading engine node
    {
        material = GetMaterialPath(shadingEngineNode);
        if (TfMapLookupPtr(_materialAdapters, material) != nullptr) {
            return true;
        }
    }

    return false;
}

SdfPath MayaHydraSceneIndex::_GetRenderItemPrimPath(const MRenderItem& ri)
{
    return GetRenderItemPrimPath(_rprimPath, ri);
}

SdfPath MayaHydraSceneIndex::GetPrimPath(const MDagPath& dg, bool isSprim) const
{
    if (isSprim) {
        return _GetPrimPath(_sprimPath, dg);
    } else {
        return _GetPrimPath(_rprimPath, dg);
    }
}

GfInterval MayaHydraSceneIndex::GetCurrentTimeSamplingInterval() const
{
    return GfInterval(_params.motionSampleStart, _params.motionSampleEnd);
}

HdRenderIndex* MayaHydraSceneIndex::GetRenderIndexPtr()
{
    TF_VERIFY(!_isTearingDown, "GetRenderIndexPtr() called while tearing down.");
    TF_VERIFY(_renderIndex, "GetRenderIndexPtr() called with null render index.");
    return _renderIndex;
}

bool MayaHydraSceneIndex::HasRenderDelegate() const
{
    if (_isTearingDown) {
        return false;
    }
    TF_VERIFY(_renderIndex, "HasRenderDelegate() called with null render index.");
    return _renderIndex->GetRenderDelegate() != nullptr;
}

bool MayaHydraSceneIndex::IsRprimTypeSupported(const TfToken& typeId) const
{
    return HasRenderDelegate() && _renderIndex->IsRprimTypeSupported(typeId);
}

bool MayaHydraSceneIndex::IsSprimTypeSupported(const TfToken& typeId) const
{
    return HasRenderDelegate() && _renderIndex->IsSprimTypeSupported(typeId);
}

bool MayaHydraSceneIndex::IsBprimTypeSupported(const TfToken& typeId) const
{
    return HasRenderDelegate() && _renderIndex->IsBprimTypeSupported(typeId);
}

HdResourceRegistrySharedPtr MayaHydraSceneIndex::GetResourceRegistry() const
{
    return HasRenderDelegate() ? _renderIndex->GetResourceRegistry() : nullptr;
}

void MayaHydraSceneIndex::RemoveInstancer(const SdfPath& id)
{
    if (_isTearingDown) {
        return;
    }
    if (!TF_VERIFY(
            HasRenderDelegate(),
            "RemoveInstancer() called without a render delegate; callers must guard with "
            "ShouldSkipHydraUpdates() first.")) {
        return;
    }
    _renderIndex->RemoveInstancer(id);
}

void MayaHydraSceneIndex::RebuildAdapterOnIdle(const SdfPath& id, uint32_t flags)
{
    if (_isTearingDown) {
        return;
    }

    std::lock_guard<std::mutex> lock(_adaptersToRebuildMutex);

    // We expect this to be a small number of objects, so using a simple linear
    // search and a vector is generally a good choice.
    for (auto& it : _adaptersToRebuild) {
        if (std::get<0>(it) == id) {
            std::get<1>(it) |= flags;
            return;
        }
    }
    _adaptersToRebuild.emplace_back(id, flags);
}

void MayaHydraSceneIndex::RecreateAdapter(const SdfPath& id, const MObject& obj)
{
    if (_RemoveAdapter<MayaHydraAdapter>(
            id,
            [](MayaHydraAdapter* a) {
                a->RemoveCallbacks();
                a->RemovePrim();
            },
            _lightAdapters,
            _cameraAdapters)) {
        if (MObjectHandle(obj).isValid()) {
            OnDagNodeAdded(obj);
        }
        return;
    }

    if (useMeshAdapter()
        && _RemoveAdapter<MayaHydraAdapter>(
            id,
            [](MayaHydraAdapter* a) {
                a->RemoveCallbacks();
                a->RemovePrim();
            },
            _shapeAdapters)) {
        MFnDagNode dgNode(obj);
        MDagPath   path;
        dgNode.getPath(path);
        if (path.isValid() && MObjectHandle(obj).isValid()) {
            InsertDag(path);
        }
        return;
    }

    if (_RemoveAdapter<MayaHydraAdapter>(
            id,
            [](MayaHydraAdapter* a) {
                a->RemoveCallbacks();
                a->RemovePrim();
            },
            _customAdapters)) {
        MFnDagNode dgNode(obj);
        MDagPath   path;
        dgNode.getPath(path);
        if (path.isValid() && MObjectHandle(obj).isValid()) {
            InsertDag(path);
        }
        return;
    }

    if (_RemoveAdapter<MayaHydraMaterialAdapter>(
            id,
            [](MayaHydraMaterialAdapter* a) {
                a->RemoveCallbacks();
                a->RemovePrim();
            },
            _materialAdapters)) {
        HdRenderIndex* renderIndex = GetRenderIndexPtr();
        for (const auto& rprimId : renderIndex->GetRprimIds()) {
            const auto* rprim = renderIndex->GetRprim(rprimId);
            if (rprim != nullptr && rprim->GetMaterialId() == id) {
                Fvp::DirtyNotifier(*this, rprimId).dirtyMaterialBinding();
            }
        }
        if (MObjectHandle(obj).isValid()) {
            CreateMaterial(GetMaterialPath(obj), obj);
        }
    }
}

template <typename AdapterPtr, typename Map>
AdapterPtr MayaHydraSceneIndex::_CreateAdapter(
    const MDagPath&                                                         dag,
    const std::function<AdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>& adapterCreator,
    Map&                                                                    adapterMap,
    bool                                                                    isSprim)
{
    // Filter for whether we should even attempt to create the adapter

    if (!adapterCreator) {
        return {};
    }

    if (IsUfeItemFromMayaUsd(dag)) {
        // UFE items that have a Hydra representation will be added to Hydra by maya-usd
        return {};
    }

    // Attempt to create the adapter

    const auto id = GetPrimPath(dag, isSprim);
    if (TfMapLookupPtr(adapterMap, id) != nullptr) {
        return {};
    }
    auto adapter = adapterCreator(this, dag);
    if (adapter == nullptr || !adapter->IsSupported()) {
        return {};
    }
    // HYDRA-1992 : Order is important here. We need to add the adapter to the map before populating.
    // Why : once we send PrimsAdded notifications, a downstream scene index might query 
    // a prim that uses a MayaHydraDataSource and try to get its material. Since 
    // MayaHydraDataSource::_GetMaterialBindingDataSource() calls MayaHydraSceneIndex::GetMaterialId(), 
    // which in turn does lookups into the adapter maps, the adapter map needs to contain 
    // the desired prim's adapter.
    adapterMap.insert({ id, adapter });
    adapter->Populate();
    adapter->CreateCallbacks();
    return adapter;
}

MayaHydraLightAdapterPtr MayaHydraSceneIndex::CreateLightAdapter(const MDagPath& dagPath)
{
    auto lightCreatorFunc = MayaHydraAdapterRegistry::GetLightAdapterCreator(dagPath);
    return _CreateAdapter(dagPath, lightCreatorFunc, _lightAdapters, true);
}

MayaHydraCameraAdapterPtr MayaHydraSceneIndex::CreateCameraAdapter(const MDagPath& dagPath)
{
    auto cameraCreatorFunc = MayaHydraAdapterRegistry::GetCameraAdapterCreator(dagPath);
    return _CreateAdapter(dagPath, cameraCreatorFunc, _cameraAdapters, true);
}

MayaHydraShapeAdapterPtr MayaHydraSceneIndex::CreateShapeAdapter(const MDagPath& dagPath)
{
    auto shapeCreatorFunc = MayaHydraAdapterRegistry::GetShapeAdapterCreator(dagPath);
    return _CreateAdapter(dagPath, shapeCreatorFunc, _shapeAdapters);
}

MayaHydraCustomDagAdapterPtr MayaHydraSceneIndex::CreateCustomAdapter(const MDagPath& dagPath)
{
    MFnDependencyNode depNode(dagPath.node());
    MNodeClass nodeClass(depNode.typeName());
    if (nodeClass.pluginName().length() == 0) {
        return {};
    }

    // Skip plugin nodes that already provide their own Hydra data through
    // the Flow Viewport data producer API.  Their MObjectHandle hash code
    // is registered in DataProducersNodeHashCodeToSdfPathRegistry when
    // they call addDataProducerSceneIndex() with a dccNode pointer.
    MObjectHandle nodeHandle(dagPath.node());
    if (!Fvp::DataProducersNodeHashCodeToSdfPathRegistry::Instance()
             .GetPath(nodeHandle.hashCode()).IsEmpty()) {
        return {};
    }

    auto creator = [](MayaHydraSceneIndex* si, const MDagPath& dag)
        -> MayaHydraCustomDagAdapterPtr {
        return std::make_shared<MayaHydraCustomDagAdapter>(si, dag);
    };
    return _CreateAdapter<MayaHydraCustomDagAdapterPtr>(
        dagPath,
        std::function<MayaHydraCustomDagAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>(creator),
        _customAdapters,
        false);
}

void MayaHydraSceneIndex::OnDagNodeAdded(const MObject& obj)
{
    if (_isTearingDown) {
        return;
    }

    if (obj.isNull())
        return;

    if (IsUfeItemFromMayaUsd(obj)) {
        // UFE items that have a Hydra representation will be added to Hydra by maya-usd
        return;
    }

    // Queue newly added DAG nodes for adapter creation during the next
    // FlushPendingUpdates().  Lights and cameras always get their dedicated
    // adapters.  When the mesh adapter is active, all other shapes go through
    // it.  Otherwise, unrecognized plugin shapes are queued for custom
    // adapter creation (_customNodesToAdd).
    if (auto lightFn = MayaHydraAdapterRegistry::GetLightAdapterCreator(obj)) {
        _lightsToAdd.push_back({ obj, lightFn });
    } else if (auto cameraFn = MayaHydraAdapterRegistry::GetCameraAdapterCreator(obj)) {
        _camerasToAdd.push_back({ obj, cameraFn });
    } else if (useMeshAdapter()) {
        _addedNodes.push_back(obj);
    } else {
        _customNodesToAdd.push_back(obj);
    }
}

void MayaHydraSceneIndex::OnDagNodeRemoved(const MObject& obj)
{
    const auto it
        = std::remove_if(_lightsToAdd.begin(), _lightsToAdd.end(), [&obj](const auto& item) {
              return item.first == obj;
          });

    if (it != _lightsToAdd.end()) {
        _lightsToAdd.erase(it, _lightsToAdd.end());
        return;
    }

    const auto itCamera
        = std::remove_if(_camerasToAdd.begin(), _camerasToAdd.end(), [&obj](const auto& item) {
              return item.first == obj;
          });
    if (itCamera != _camerasToAdd.end()) {
        _camerasToAdd.erase(itCamera, _camerasToAdd.end());
        return;
    }

    if (useMeshAdapter()) {
        const auto it
            = std::remove_if(_addedNodes.begin(), _addedNodes.end(), [&obj](const auto& item) {
                  return item == obj;
              });

        if (it != _addedNodes.end()) {
            _addedNodes.erase(it, _addedNodes.end());
        }
    }

    {
        const auto itCustom
            = std::remove_if(_customNodesToAdd.begin(), _customNodesToAdd.end(), [&obj](const auto& item) {
                  return item == obj;
              });
        if (itCustom != _customNodesToAdd.end()) {
            _customNodesToAdd.erase(itCustom, _customNodesToAdd.end());
        }
    }
}

// Create the material(s) bound to a mesh and, for multi-material meshes, the
// Hydra geomSubsets that route each group of faces to its material.
//
// What: ensures a material adapter exists for every shading group assigned to
// 'dag', then - only when there is more than one assignment - emits one
// HdGeomSubset prim per assignment under 'meshPrimId', each carrying the face
// indices it covers and a material binding to the corresponding material. A mesh
// with a single (whole-object) assignment needs no subsets: the regular mesh
// material binding already covers it, so we return after creating the material.
// How: GetAllShadingAssignments enumerates the (component, shadingEngine) pairs.
// For each assignment the face set is taken either from all polygons (null
// component / whole object) or from the kMeshPolygonComponent element list, and
// is published as a faceSet HdGeomSubset with an HdMaterialBindingsSchema so the
// render delegate shades those faces with the right material.
// Subsets only apply to meshes, so non-mesh (incl. plugin) shapes are skipped.
// When a mesh has both a whole-object assignment and per-face assignments, the
// whole-object subset is skipped to avoid overlapping subsets with undefined
// material precedence; the whole-object material remains the mesh's binding.
void MayaHydraSceneIndex::_InsertGeomSubsetsForMesh(
    const MDagPath& dag, const SdfPath& meshPrimId)
{
    std::vector<ShadingAssignment> assignments;
    GetAllShadingAssignments(dag, assignments);

    for (const auto& sa : assignments) {
        const auto materialId = GetMaterialPath(sa.shadingEngine);
        if (TfMapLookupPtr(_materialAdapters, materialId) == nullptr) {
            CreateMaterial(materialId, sa.shadingEngine);
        }
    }

    if (assignments.size() <= 1) {
        return;
    }

    // geomSubsets only apply to meshes; bail out for any other (incl. plugin) shape.
    if (!dag.hasFn(MFn::kMesh)) {
        return;
    }
    MStatus meshStatus;
    MFnMesh mesh(dag, &meshStatus);
    if (!meshStatus) {
        return;
    }

    // A whole-object (null component) assignment covers every face. If the mesh
    // also has per-face assignments, emitting a full-coverage subset would overlap
    // them with undefined material precedence, so skip the whole-object subset when
    // component assignments are present.
    bool hasComponentAssignments = false;
    for (const auto& sa : assignments) {
        if (!sa.component.isNull()) {
            hasComponentAssignments = true;
            break;
        }
    }

    int subsetIndex = 0;
    static const TfToken purposes[] = { HdMaterialBindingsSchemaTokens->allPurpose };

    for (const auto& sa : assignments) {
        if (hasComponentAssignments && sa.component.isNull()) {
            continue;
        }

        VtIntArray faceIndices;

        if (sa.component.isNull()) {
            const int numFaces = mesh.numPolygons();
            faceIndices.resize(numFaces);
            for (int f = 0; f < numFaces; ++f) {
                faceIndices[f] = f;
            }
        } else if (sa.component.apiType() == MFn::kMeshPolygonComponent) {
            MFnSingleIndexedComponent fnComp(sa.component);
            MIntArray elements;
            fnComp.getElements(elements);
            faceIndices.resize(elements.length());
            for (uint32_t j = 0; j < elements.length(); ++j) {
                faceIndices[j] = elements[j];
            }
        } else {
            // Defensive: for a mesh, getConnectedSetsAndMembers only ever yields a
            // null component (whole object) or a face component
            // (kMeshPolygonComponent). Any other component type is unexpected, so
            // skip it rather than treat its element indices as face indices.
            continue;
        }

        if (faceIndices.empty()) {
            continue;
        }

        const SdfPath materialPath = GetMaterialPath(sa.shadingEngine);
        const SdfPath subsetPath = meshPrimId.AppendChild(
            TfToken("geomSubset_" + std::to_string(subsetIndex++)));

        HdDataSourceBaseHandle materialBindingSources[] = {
            HdMaterialBindingSchema::Builder()
                .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(materialPath))
                .Build()
        };

        AddPrims({{ subsetPath,
            HdPrimTypeTokens->geomSubset,
            HdRetainedContainerDataSource::New(
                HdGeomSubsetSchemaTokens->geomSubset,
                HdGeomSubsetSchema::Builder()
                    .SetType(HdGeomSubsetSchema::BuildTypeDataSource(
                        HdGeomSubsetSchemaTokens->typeFaceSet))
                    .SetIndices(HdRetainedTypedSampledDataSource<VtIntArray>::New(faceIndices))
                    .Build(),
                HdMaterialBindingsSchema::GetSchemaToken(),
                HdMaterialBindingsSchema::BuildRetained(
                    TfArraySize(purposes), purposes, materialBindingSources)) }});
    }
}

void MayaHydraSceneIndex::InsertDag(const MDagPath& dag)
{
    // We don't care about transforms.
    if (dag.hasFn(MFn::kTransform)) {
        return;
    }

    MFnDagNode dagNode(dag);
    if (dagNode.isIntermediateObject()) {
        return;
    }

    // Cameras can be invisible and still be renderable, so adapter creation
    // must occur before the visibility check.
    if (CreateCameraAdapter(dag)) {
        return;
    }

    // In batch mode (useMeshAdapter), MItDag visits every DAG node including
    // LOD meshes and corrective blend-shape targets that VP2 never makes render
    // items for.  Skip shapes that are not visible so that only the intended
    // renderable geometry reaches Hydra, matching viewport behaviour.
    if (useMeshAdapter() && !dag.isVisible()) {
        return;
    }

    // NURBS curves are handled by the render-item adapter path, which
    // correctly tracks VP2 visibility. Creating a DAG shape adapter here
    // would produce a duplicate prim.
    if (dag.hasFn(MFn::kNurbsCurve)) {
        return;
    }

    if (IsUfeItemFromMayaUsd(dag)) {
        // UFE items that have a Hydra representation will be added to Hydra by maya-usd
        return;
    }

    // Invisible lights don't contribute to the scene, so light adapter
    // creation after the visibility check above is correct.  Custom lights
    // don't have MFn::kLight.
    if (CreateLightAdapter(dag)) {
        return;
    }
    // We are inserting a single prim and
    // instancer for every instanced mesh.
    if (dag.isInstanced() && dag.instanceNumber() > 0) {
        return;
    }

    auto adapter = CreateShapeAdapter(dag);
    if (adapter) {
        _InsertGeomSubsetsForMesh(dag, adapter->GetID());
        return;
    }

    CreateCustomAdapter(dag);
}

void MayaHydraSceneIndex::UpdateLightVisibility(const MDagPath& dag)
{
    const auto id = GetPrimPath(dag, true);
    _FindAdapter<MayaHydraLightAdapter>(
        id,
        [](MayaHydraLightAdapter* a) {
            if (a->UpdateVisibility()) {
                a->RemovePrim();
                a->Populate();
                a->InvalidateTransform();
            }
        },
        _lightAdapters);
}

SdfPath MayaHydraSceneIndex::GetMaterialPath(const MObject& obj)
{
    return _GetMaterialPath(_materialPath, obj);
}

bool MayaHydraSceneIndex::CreateMaterial(const SdfPath& id, const MObject& obj)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
        .Msg("MayaHydraSceneIndex::CreateMaterial(%s)\n", id.GetText());

    auto materialCreator = MayaHydraAdapterRegistry::GetMaterialAdapterCreator(obj);
    if (materialCreator == nullptr) {
        return false;
    }
    auto materialAdapter = materialCreator(id, this, obj);
    if (materialAdapter == nullptr || !materialAdapter->IsSupported()) {
        return false;
    }

    // HYDRA-1992 : Order is important here. Downstream scene indices
    // might do some operations based on PrimsAdded notifications, which
    // we send once we call Populate(). These operations might loop back
    // into the material adapter and this scene index, so it needs to be
    // setup properly.
    _materialAdapters.insert({id, materialAdapter});
    materialAdapter->Populate();
    materialAdapter->CreateCallbacks();
    return true;
}

void MayaHydraSceneIndex::AddNewInstance(const MDagPath& dag)
{
    MDagPathArray dags;
    MDagPath::getAllPathsTo(dag.node(), dags);
    const auto dagsLength = dags.length();
    if (dagsLength == 0) {
        return;
    }
    const auto                             masterDag = dags[0];
    const auto                             id = GetPrimPath(masterDag, false);
    std::shared_ptr<MayaHydraShapeAdapter> masterAdapter;
    if (!TfMapLookup(_shapeAdapters, id, &masterAdapter) || masterAdapter == nullptr) {
        return;
    }
    if (dagsLength == 1) {
        RecreateAdapterOnIdle(id, masterDag.node());
        return;
    }
    // Node now has multiple DAG paths: rebuild callbacks (register _InstancerNodeDirty on every
    // path) and dirty instancer topology / instance primvars.
    RebuildAdapterOnIdle(id, MayaHydraSceneIndex::RebuildFlagCallbacks);
    {
        Fvp::DirtyNotifier(*this, masterAdapter->GetID()).dirtyInstancer().dirtyPrimvars();
    }
    const SdfPath instancerId = masterAdapter->GetInstancerID();
    if (!instancerId.IsEmpty()) {
        Fvp::DirtyNotifier(*this, instancerId).dirtyInstancer().dirtyPrimvars();
    }
}

void MayaHydraSceneIndex::MaterialTagChanged(const SdfPath& id)
{
    if (std::find(_materialTagsChanged.begin(), _materialTagsChanged.end(), id)
        == _materialTagsChanged.end()) {
        _materialTagsChanged.push_back(id);
    }
}

VtValue MayaHydraSceneIndex::GetShadingStyle(SdfPath const& id)
{
    if (auto&& ri = TfMapLookupPtr(_renderItemsAdapters, id)) {
        auto primitive = (*ri)->GetPrimitive();
        if (MHWRender::MGeometry::Primitive::kLines == primitive
            || MHWRender::MGeometry::Primitive::kLineStrip == primitive) {
            return VtValue(
                _tokens
                    ->constantLighting); // Use fallbackMaterial + constantLighting + displayColor
        }
    }
    return VtValue();
}

bool MayaHydraSceneIndex::useMayaNormals()
{
    static const bool val = TfGetEnvSetting(MAYA_HYDRA_PASS_NORMALS_TO_HYDRA);
    return val;
}

bool MayaHydraSceneIndex::useMeshAdapter()
{
    static const bool uma = TfGetEnvSetting(MAYA_HYDRA_USE_MESH_ADAPTER);
    return (_interactive) ? uma : true;// Batch rendering (=> !_interactive) always uses mesh adapter
}

void MayaHydraSceneIndex::UpdateLightsShadowCollection()
{
    // Mark shadowCollection as dirty if any render prim is added/removed
    if (_renderCollectionChanged && _shadowsEnabled) {
        _MapAdapter<MayaHydraLightAdapter>(
            [](MayaHydraLightAdapter* a) {
                MayaHydra::DirtyNotifier(a).dirtyCollections();
            },
            _lightAdapters);
    }
}

GfBBox3d MayaHydraSceneIndex::GetBoundingBox() const
{
    GfBBox3d bbox;
    _MapAdapter<MayaHydraAdapter>(
        [&](MayaHydraAdapter* a) { bbox = GfBBox3d::Combine(a->GetBoundingBox(), bbox); },
        _renderItemsAdapters,
        _shapeAdapters);
    return bbox;
}

PXR_NAMESPACE_CLOSE_SCOPE
