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

#include "renderItemAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/adapters/renderItemTopologyUtil.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/usd/sdr/registry.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <maya/MAnimControl.h>
#include <maya/MDGContextGuard.h>
#include <maya/MFn.h>
#include <maya/MNodeMessage.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

#define PLUG_THIS_PLUGIN \
    PlugRegistry::GetInstance().GetPluginWithName(TF_PP_STRINGIZE(MFB_PACKAGE_NAME))

namespace {

unsigned int
_GetPositionVertexCount(MGeometry* geom, int vertexBufferCount)
{
    if (!geom || vertexBufferCount <= 0) {
        return 0;
    }
    for (int vbIdx = 0; vbIdx < vertexBufferCount; ++vbIdx) {
        MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
        if (!mvb) {
            continue;
        }
        if (mvb->descriptor().semantic() == MGeometry::Semantic::kPosition) {
            return mvb->vertexCount();
        }
    }
    return 0;
}

void
_EmitRenderItemTopologyDirtyLocators(
    Fvp::DirtyNotifier& notifier,
    MHWRender::MGeometry::Primitive primitive)
{
    switch (primitive) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        notifier.dirtyMeshTopology();
        break;
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        notifier.dirtyBasisCurvesTopology();
        break;
    default:
        break;
    }
}

} // namespace

/*
 * MayaHydraRenderItemAdapter is used to translate from a render item to hydra.
 * This is where we translate from Maya shapes (such as meshes) to hydra using their vertex and
 * index buffers, look for "MVertexBuffer" and "MIndexBuffer" in this file to get more information.
 */
MayaHydraRenderItemAdapter::MayaHydraRenderItemAdapter(
    const MDagPath&       dagPath,
    const SdfPath&        slowId,
    int                   fastId,
    MayaHydraSceneIndex*  mayaHydraSceneIndex,
    const MRenderItem&    ri,
    TfToken              purposeRenderTag)
    : MayaHydraAdapter(dagPath.node(), slowId, mayaHydraSceneIndex)
    , _dagPath(dagPath)
    , _primitive(ri.primitive())
    , _name(ri.name())
    , _fastId(fastId)
    , _purposeRenderTag(purposeRenderTag)
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    , _cullMode(ri.cullMode())
#endif
{
}

MayaHydraRenderItemAdapter::~MayaHydraRenderItemAdapter() { _RemoveRprim(); }

void MayaHydraRenderItemAdapter::Populate()
{
    _InsertRprim(this);
}

TfToken MayaHydraRenderItemAdapter::GetRenderTag() const
{
    return _purposeRenderTag;
}

void MayaHydraRenderItemAdapter::UpdateTransform(const MRenderItem& ri)
{
    MMatrix matrix;
    if (ri.getMatrix(matrix) == MStatus::kSuccess) {
        _transform[0] = GetGfMatrixFromMaya(matrix);
        if (GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled()) {
            MDGContextGuard guard(MAnimControl::currentTime() + 1.0);
            _transform[1] = GetGfMatrixFromMaya(matrix);
        } else {
            _transform[1] = _transform[0];
        }
    }
}

bool MayaHydraRenderItemAdapter::IsSupported() const
{
    switch (_primitive) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->basisCurves);
    case MHWRender::MGeometry::Primitive::kPoints:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->points);
    default: return false;
    }
}

void MayaHydraRenderItemAdapter::_InsertRprim(MayaHydraAdapter* adapter)
{
    switch (GetPrimitive()) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->mesh, GetID());
        break;
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->basisCurves, GetID());
        break;
    case MHWRender::MGeometry::Primitive::kPoints:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->points, GetID());
        break;
    default:
        TF_RUNTIME_ERROR(
            "Unsupported render item primitive %d for item '%s' (prim '%s', id '%s').",
            static_cast<int>(GetPrimitive()),
            _name.asChar(),
            _dagPath.fullPathName().asChar(),
            GetID().GetText());
        break;
    }
    _isPopulated = true;
}

void MayaHydraRenderItemAdapter::_RemoveRprim()
{
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

// We receive in that function the changes made in the Maya viewport between the last frame rendered
// and the current frame
void MayaHydraRenderItemAdapter::UpdateFromDelta(const UpdateFromDeltaData& data)
{
    if (_primitive != MHWRender::MGeometry::Primitive::kTriangles
        && _primitive != MHWRender::MGeometry::Primitive::kTriangleStrip
        && _primitive != MHWRender::MGeometry::Primitive::kLines
        && _primitive != MHWRender::MGeometry::Primitive::kLineStrip) {
        return;
    }

    const bool positionsHaveBeenReset
        = (0 == _positions.size()); // when positionsHaveBeenReset is true we need to recompute the
                                    // geometry and topology as our data has been cleared
    using MVS = MDataServerOperation::MViewportScene;
    // const bool isNew = flags & MViewportScene::MVS_new;  //not used yet
    const bool visible          = data._flags & MVS::MVS_visible;
    const bool matrixChanged    = data._flags & MVS::MVS_changedMatrix;
          bool geomChanged      = (data._flags & MVS::MVS_changedGeometry) || positionsHaveBeenReset;//Non const as we may modify it later => Temp workaround for a bug in Maya MAYA-134200
    const bool topoChanged      = (data._flags & MVS::MVS_changedTopo) || positionsHaveBeenReset;
    const bool visibChanged     = data._flags & MVS::MVS_changedVisibility;
    const bool effectChanged    = data._flags & MVS::MVS_changedEffect;

    // Dirty notification policy for this function — see
    // doc/render_delegate_topology_vs_deformation.md for the full contract.
    //   Granularity: emit one locator per changed datum; never use the broad primvars locator
    //     for geometry edits — that would re-pull unchanged data in the render delegate.
    //   Topology: on genuine connectivity change emit topology locators only
    //     (mesh/topology or basisCurves/topology via _EmitRenderItemTopologyDirtyLocators).
    //     When Maya also sets MVS_changedGeometry alongside MVS_changedTopo, the separate
    //     geomChanged path may dirty granular primvars (points/st/tangents and optionally normals).
    //     The broad primvars locator is NOT emitted on the topology path — it would subsume
    //     granular locators and defeat the useMayaNormals skip.
    //     Topology locators are suppressed when Maya sets MVS_changedTopo alongside
    //     MVS_changedGeometry but both vertex count and index connectivity are unchanged
    //     (deformation-only). When connectivity changes with the same vertex count, topology
    //     locators are still emitted.
    //   Extent: dirty only when the bounding box actually changes. Maya has no bbox-changed
    //     flag, so we diff the freshly-read bbox against the stored _bounds before overwriting.
    //     Checked in the geomChanged||topoChanged block (before the vertex-count workaround below),
    //     separately from the per-primvar dirty block — this is intentional, not an oversight.
    //   Normals: skip dirtyNormals() when useMayaNormals is false — Hydra generates
    //     normals itself in that mode and a redundant notification would cause unnecessary work.
    //     The guard applies on the geomChanged path where granular primvar locators are emitted.
    //
    // Construct the notifier AFTER the early-exit guard above so an early return always leaves
    // the notifier empty on an early return.
    MayaHydra::DirtyNotifier notifier(this);

#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    MRenderItem::CullMode cullMode = data._ri.cullMode();
    if (cullMode != _cullMode) {
        //  MRenderItem uses CullNone to denote doubleSided
        if (IsDoubleSided(_cullMode) || IsDoubleSided(cullMode)) {
            notifier.dirtyDoubleSided();
        }
        notifier.dirtyCullStyle();
        _cullMode = cullMode;
    }
#endif

    if (data._wireColorDirty) {
        // Constant-interpolation displayColor (no vertex color set present on this render item).
        notifier.dirtyDisplayColor();
    }

    const bool hideOnPlayback = data._ri.isHideOnPlayback();
    if (hideOnPlayback != _isHideOnPlayback) {
        _isHideOnPlayback = hideOnPlayback;
        notifier.dirtyVisibility();
    }

    if (visibChanged) {
        _visible = visible;
        notifier.dirtyVisibility();
    }

    if (effectChanged) {
        notifier.dirtyMaterialBinding();
    }
    if (matrixChanged) {
        notifier.dirtyTransform();
    }
    // Hoisted so it is in scope for both the topology dirty block and the per-primvar
    // geomChanged block below. The static ensures the env var is read only once.
    static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();

    // Extent is checked here, under geomChanged||topoChanged, so it is always evaluated when
    // positions or topology change — including the geomChanged case. The old code always dirtied
    // extent on geomChanged; the new code diffs the actual bbox first and only emits dirtyExtent()
    // when the value changed. This is intentional: if vertices moved without changing the bbox
    // (e.g. internal vertices shuffled), there is nothing for the render delegate to re-read.
    MGeometry* geom = nullptr;
    if (geomChanged || topoChanged) {
        geom = data._ri.geometry();
        auto bbox = data._ri.boundingBox();
        const MPoint& min = bbox.min();
        const MPoint& max = bbox.max();
        const GfRange3d newRange({min.x, min.y, min.z}, {max.x, max.y, max.z});
        if (newRange != _bounds.GetRange()) {
            notifier.dirtyExtent();
            _bounds.SetRange(newRange);
        }
        // Apply the world matrix
        MMatrix matrix;
        data._ri.getMatrix(matrix);
        _bounds.SetMatrix(GetGfMatrixFromMaya(matrix));
    }
    VtIntArray vertexIndices;
    VtIntArray vertexCounts;
        
    const int vertexBuffercount = geom ? geom->vertexBufferCount() : 0;
    const size_t storedPositionCountBeforeUpdate = _positions.size();

    //Temp workaround for a bug in Maya MAYA-134200
    if ((!geomChanged && topoChanged) && vertexBuffercount) { 
        //With face components selection, we have topoChanged which is true but geomChanged is false, but this is wrong, the number of vertices may have changed.
        //We want to check here if we also need to update the geometry if the number of vertices is different from what is stored already
        for (int vbIdx = 0; (vbIdx < vertexBuffercount) && (!geomChanged); vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if (!mvb) {
                continue;
            }

            const MVertexBufferDescriptor& desc = mvb->descriptor();
            const auto                     semantic = desc.semantic();
            switch (semantic) {
            case MGeometry::Semantic::kPosition: {
                // Vertices
                MVertexBuffer*     verts = mvb;
                const unsigned int originalVertexCount = verts->vertexCount();
                if (_positions.size() != originalVertexCount) {//Is it different ?
                    geomChanged = true;//this will stop the loop
                }
            } break;
            default: break;
            }
        }
    }

    // geomChanged means vertex buffers are re-read. Dirty one locator per primvar so the render
    // delegate only re-pulls what actually changed. Normals are skipped when Hydra generates them.
    // Emitted after the vertex-count workaround below which may have promoted topoChanged -> geomChanged.
    if (geomChanged) {
        notifier.dirtyPoints();
        notifier.dirtyUVs();
        notifier.dirtyTangents();
            // .dirtyVertexColors() — uncomment once the kColor buffer read is wired in (see kColor case below).
            // Do not emit the locator before the data is actually read: a dirty signal without a
            // corresponding data update is a false promise to the render delegate.
        if (useMayaNormals) {
            notifier.dirtyNormals(); // skipped when Hydra generates normals
        }
    }

    // Vertices
    if (geomChanged && vertexBuffercount) {
        //vertexBuffercount > 0 means geom is non null
        for (int vbIdx = 0; vbIdx < vertexBuffercount; vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if ( ! mvb) {
                continue;
            }

            const MVertexBufferDescriptor& desc = mvb->descriptor();
            const auto semantic = desc.semantic();
            switch(semantic){

                case MGeometry::Semantic::kPosition: {
                    //Vertices
                    MVertexBuffer*verts = mvb;
                    int                vertCount = 0;
                    const unsigned int originalVertexCount = verts->vertexCount();
                    if (topoChanged) {
                        vertCount = originalVertexCount;
                    } else {
                        // Keep the previously-determined vertex count in case it was truncated.
                        const size_t positionSize = _positions.size();
                        if (positionSize > 0 && positionSize <= originalVertexCount) {
                            vertCount = positionSize;
                        } else {
                            vertCount = originalVertexCount;
                        }
                    }

                    _positions.clear();
                    const auto* vertexPositions = reinterpret_cast<const GfVec3f*>(verts->map());
                    if (TF_VERIFY(vertexPositions)) {
                        _positions.assign(vertexPositions, vertexPositions + vertCount);
                    }
                    verts->unmap();
                }
                break;
                case MGeometry::Semantic::kNormal: {
                    //Normals
                    if (useMayaNormals){
                        MVertexBuffer* normals = mvb;
                        int normalsCount = 0;
                        const unsigned int originalNormalsCount = normals->vertexCount();
                        if (topoChanged) {
                            normalsCount = originalNormalsCount;
                        } else {
                            // Keep the previously-determined normals count in case it was truncated.
                            const size_t normalSize = _normals.size();
                            if (normalSize > 0 && normalSize <= originalNormalsCount) {
                                normalsCount = normalSize;
                            } else {
                                normalsCount = originalNormalsCount;
                            }
                        }

                        _normals.clear();
                        const auto* vertexNormals = reinterpret_cast<const GfVec3f*>(normals->map());
                        if (TF_VERIFY(vertexNormals)) {
                            _normals.assign(vertexNormals, vertexNormals + normalsCount);
                        }
                        normals->unmap();
                    }
                }
                break;
                case MGeometry::Semantic::kTexture: {
                    // Textures:
                    if (_primitive == MGeometry::Primitive::kTriangles
                        || _primitive == MGeometry::Primitive::kTriangleStrip) {
                        int uvsCount = 0;
                        const unsigned int originalUvsCount = mvb->vertexCount();
                        if (topoChanged) {
                            uvsCount = originalUvsCount;
                        } else {
                            // Keep the previously-determined uvs count in case it was truncated.
                            const size_t uvSize = _uvs.size();
                            if (uvSize > 0 && uvSize <= originalUvsCount) {
                                uvsCount = uvSize;
                            } else {
                                uvsCount = originalUvsCount;
                            }
                        }

                        _uvs.clear();
                        const auto* uvData =
                            reinterpret_cast<const GfVec2f*>(mvb->map());
                        if (TF_VERIFY(uvData)) {
                            _uvs.assign(uvData, uvData + uvsCount);
                        }
                        mvb->unmap();
                    }
                }
                break;
                case MHWRender::MGeometry::kTangent: {
                    // Tangents
                    if (_primitive == MGeometry::Primitive::kTriangles
                        || _primitive == MGeometry::Primitive::kTriangleStrip) {
                        int tangentsCount = 0;
                        const unsigned int originalTangentsCount = mvb->vertexCount();
                        if (topoChanged) {
                            tangentsCount = originalTangentsCount;
                        } else {
                            // Keep the previously-determined tangents count in case it was truncated.
                            const size_t tangentSize = _tangents.size();
                            if (tangentSize > 0 && tangentSize <= originalTangentsCount) {
                                tangentsCount = tangentSize;
                            } else {
                                tangentsCount = originalTangentsCount;
                            }
                        }

                        _tangents.clear();
                        const auto* tangentData =
                            reinterpret_cast<const GfVec3f*>(mvb->map());
                        if (TF_VERIFY(tangentData)) {
                            _tangents.assign(tangentData, tangentData + tangentsCount);
                        }
                        mvb->unmap();
                    }
                }
                break;
                case MGeometry::Semantic::kColor:
                    // Vertex color sets (per-vertex displayColor) are not yet read from the
                    // vertex buffer. When adding support: read the buffer here and store the
                    // result, then uncomment notifier.dirtyVertexColors() in the geomChanged
                    // block above so the render delegate is notified only once data is live.
                break;
                default:
                break;
            }
        }
    }

    // Indices
    // Line strips do not make use of the index buffer, so we can skip this block.
    // See "Line strips indices are implicitly defined" comment.
    if ((topoChanged || geomChanged) && vertexBuffercount
        && GetPrimitive() != MHWRender::MGeometry::Primitive::kLineStrip) {
        // Assume first stream contains the positions.
        MIndexBuffer* indices = geom->indexBuffer(0);
        if (indices) {
            int indexCount = indices->size();
            int* indicesData = (int*)indices->map();
            // USD spamming the "topology references only upto element" message is super
            // slow.  Scanning the index array to look for an incompletely used vertex
            // buffer is innefficient, but it's better than the spammy warning. Cause of
            // the incompletely used vertex buffer is unclear.  Maya scene data just is
            // that way sometimes.
            int maxIndex = 0;
            for (int i = 0; i < indexCount; i++) {
                if (indicesData[i] > maxIndex) {
                    maxIndex = indicesData[i];
                }
            }

            vertexIndices.assign(indicesData, indicesData + indexCount);

            if (maxIndex < (int64_t)_positions.size() - 1) {
                _positions.resize(maxIndex + 1);
            }
            const size_t numNormals = _normals.size();
            if (numNormals > 0 && (maxIndex < (int64_t)numNormals - 1)) {
                _normals.resize(maxIndex + 1);
            }
            const size_t numUvs = _uvs.size();
            if (numUvs > 0 && (maxIndex < (int64_t)numUvs - 1)) {
                _uvs.resize(maxIndex + 1);
            }
            const size_t numTangents = _tangents.size();
            if (numTangents > 0 && (maxIndex < (int64_t)numTangents - 1)) {
                _tangents.resize(maxIndex + 1);
            }

            switch (GetPrimitive()) {
            case MHWRender::MGeometry::Primitive::kTriangles:
                vertexCounts.resize(indexCount / 3);
                vertexCounts.assign(indexCount / 3, 3);
                break;
            case MHWRender::MGeometry::Primitive::kTriangleStrip: {
                // Convert triangle strip indices to individual triangles.
                // For N strip indices we get N-2 triangles, with alternating
                // winding to maintain consistent face orientation.
                if (indexCount >= 3) {
                    const int numTriangles = indexCount - 2;
                    vertexCounts.assign(numTriangles, 3);
                    VtIntArray expandedIndices;
                    expandedIndices.reserve(numTriangles * 3);
                    for (int i = 0; i < numTriangles; ++i) {
                        if (i % 2 == 0) {
                            expandedIndices.push_back(indicesData[i]);
                            expandedIndices.push_back(indicesData[i + 1]);
                            expandedIndices.push_back(indicesData[i + 2]);
                        } else {
                            expandedIndices.push_back(indicesData[i + 1]);
                            expandedIndices.push_back(indicesData[i]);
                            expandedIndices.push_back(indicesData[i + 2]);
                        }
                    }
                    vertexIndices = std::move(expandedIndices);
                } else {
                    vertexCounts.clear();
                    vertexIndices.clear();
                }
                break;
            }
            case MHWRender::MGeometry::Primitive::kLines:
                vertexCounts.resize(indexCount);
                vertexCounts.assign(indexCount / 2, 2);
                break;
            default:
                TF_RUNTIME_ERROR(
                    "Unsupported render item primitive %d for item '%s' (prim '%s', id '%s').",
                    static_cast<int>(GetPrimitive()),
                    _name.asChar(),
                    _dagPath.fullPathName().asChar(),
                    GetID().GetText());
                break;
            }
            indices->unmap();
        }
    }

    // Topology dirty locators are decided after index buffers are read so we can diff connectivity,
    // not just vertex count, when Maya sets topoChanged alongside geomChanged (MAYA-134200).
    const bool emitTopologyLocators = RenderItemShouldEmitTopologyLocators(
        topoChanged,
        geomChanged,
        geom && vertexBuffercount > 0,
        storedPositionCountBeforeUpdate == 0 && _positions.empty(),
        storedPositionCountBeforeUpdate,
        _GetPositionVertexCount(geom, vertexBuffercount),
        _topology.get(),
        GetPrimitive(),
        vertexIndices,
        vertexCounts);
    if (emitTopologyLocators) {
        _EmitRenderItemTopologyDirtyLocators(notifier, GetPrimitive());
    }

    // Keep cached topology in sync whenever index buffers were read, not only when Maya sets
    // MVS_changedTopo. geomChanged-only connectivity edits (e.g. edge flip) may skip the topo flag
    // while still changing the index buffer.
    const bool indicesWereRead = (topoChanged || geomChanged) && vertexBuffercount > 0
        && GetPrimitive() != MHWRender::MGeometry::Primitive::kLineStrip;
    if (indicesWereRead && !vertexCounts.empty()) {
        switch (GetPrimitive()) {
        case MGeometry::Primitive::kTriangleStrip:
        case MGeometry::Primitive::kTriangles: {
            if (useMayaNormals) {
                _topology.reset(new HdMeshTopology(
                    PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            } else {
                _topology.reset(new HdMeshTopology(
                    (GetMayaHydraSceneIndex()->GetParams().displaySmoothMeshes
                     || GetDisplayStyle().refineLevel > 0)
                        ? PxOsdOpenSubdivTokens->catmullClark
                        : PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            }
            break;
        }
        case MGeometry::Primitive::kLines: {
            _topology.reset(new HdBasisCurvesTopology(
                HdTokens->linear,
                {},
                HdTokens->segmented,
                vertexCounts,
                vertexIndices));
            break;
        }
        default: break;
        }
    } else if (topoChanged) {
        switch (GetPrimitive()) {
        case MGeometry::Primitive::kTriangleStrip:
        case MGeometry::Primitive::kTriangles:
            if (vertexCounts.empty()) {
                _topology.reset();
            }
            break;
        case MGeometry::Primitive::kLines:
        case MGeometry::Primitive::kLineStrip: {
            TfToken curveTopoType(HdTokens->segmented);
            if (GetPrimitive() == MGeometry::Primitive::kLineStrip) {
                // Line strips indices are implicitly defined:
                // When using line strips, the GPU will draw a connected series of lines between the
                // vertices specified by the indices. When specifying indices for a line strip, you
                // only need to specify the order of the vertices that you want connected. This is
                // implicit in Hydra when specifying an empty index buffer.
                curveTopoType = HdTokens->nonperiodic;
                vertexCounts.assign(1, _positions.size());
                vertexIndices = VtIntArray();
            }
            _topology.reset(new HdBasisCurvesTopology(
                HdTokens->linear,
                // basis type is ignored, due to linear curve type
                {},
                curveTopoType,
                vertexCounts,
                vertexIndices));
            break;
        }
        default: break;
        }
    }
}

HdMeshTopology MayaHydraRenderItemAdapter::GetMeshTopology()
{
    return _topology ? *static_cast<HdMeshTopology*>(_topology.get()) : HdMeshTopology();
}

HdBasisCurvesTopology MayaHydraRenderItemAdapter::GetBasisCurvesTopology()
{
    return _topology ? *static_cast<HdBasisCurvesTopology*>(_topology.get())
                     : HdBasisCurvesTopology();
}

VtValue MayaHydraRenderItemAdapter::Get(const TfToken& key)
{
    if (key == HdTokens->points) {
        return VtValue(_positions);
    }
    if (key == HdTokens->normals) {
        return VtValue(_normals);
    }
    if (key == MayaHydraAdapterTokens->tangents){
        return VtValue(_tangents);
    }
    if (key == MayaHydraAdapterTokens->st) {
        return VtValue(_uvs);
    }
    if (key == HdTokens->displayColor) {
        return VtValue(GfVec4f(
            _wireframeColor[0], _wireframeColor[1], _wireframeColor[2], _wireframeColor[3]));
    }

    // Let base class handle other keys
    return MayaHydraAdapter::Get(key);
}

HdPrimvarDescriptorVector
MayaHydraRenderItemAdapter::GetPrimvarDescriptors(HdInterpolation interpolation)
{
    // Base descriptors
    HdPrimvarDescriptorVector descs = MayaHydraAdapter::GetPrimvarDescriptors(interpolation);

    // Local descriptors
    HdPrimvarDescriptorVector localDescs;
    if (interpolation == HdInterpolationVertex) {// Vertices
        static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();
        if(useMayaNormals) {
            localDescs = {
                { UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point },//Vertices
                { UsdGeomTokens->normals, interpolation, HdPrimvarRoleTokens->normal }//Normals
            };
        }
        else {
            localDescs = {
                { UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point }//Vertices only
            };
        }
        // Also use HdInterpolationVertex for UV/Tangent, same as Normal
        // The vertex buffers in MRenderItem was already expanded as per-face-vertex
        // E.g., A default Maya cube polygon mesh will give 24 face-vertices/normals/uvs/tangents vertex buffers
        // Note: the default cube doesn't give 36 face vertices as VP2 deduplicated them.
        if (_primitive == MGeometry::Primitive::kTriangles
            || _primitive == MGeometry::Primitive::kTriangleStrip) {
            localDescs.push_back(
                {MayaHydraAdapterTokens->st, interpolation, HdPrimvarRoleTokens->textureCoordinate}); //uvs
            localDescs.push_back(
                {MayaHydraAdapterTokens->tangents, interpolation, HdPrimvarRoleTokens->textureCoordinate}); //tangents
        }
    } else if (interpolation == HdInterpolationConstant) {
        switch(_primitive){
            case MGeometry::Primitive::kPoints: //Fall into
            case MGeometry::Primitive::kLines: //Fall into
            case MGeometry::Primitive::kLineStrip: //Fall into
            case MGeometry::Primitive::kAdjacentLines: //Fall into
            case MGeometry::Primitive::kAdjacentLineStrip:
            {
                localDescs = { { HdTokens->displayColor, interpolation, HdPrimvarRoleTokens->color } };//Use display color only for lines/points (avoid triangles)
            }
            break;
            default:
            break;
        }
    }

    // Combine descriptors
    descs.insert(descs.end(), localDescs.begin(), localDescs.end());
    return descs;
}

VtValue MayaHydraRenderItemAdapter::GetMaterialResource() { return {}; }

bool MayaHydraRenderItemAdapter::GetVisible()
{
    // Assuming that, if the playback is in the active view only
    // (MAnimControl::kPlaybackViewActive), we are called because we are in the active view
    if (_isHideOnPlayback && _isInPlayback) {
        return false;
    }

    return _visible;
}

void MayaHydraRenderItemAdapter::SetPlaybackState(bool isPlaybackRunning)
{
    // There was a change in the playblack state, it started or stopped running so update any
    // primitive that is dependent on this
    if (_isInPlayback != isPlaybackRunning) {
        _isInPlayback = isPlaybackRunning;
        if (_isHideOnPlayback) {
            MayaHydra::DirtyNotifier(this).dirtyVisibility();
        }
    }
}

HdCullStyle MayaHydraRenderItemAdapter::GetCullStyle() const
{
    if (_isArnoldSkyDomeLightTriangleShape) {
        return HdCullStyleFront;
    }
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    switch (_cullMode) {
    case MRenderItem::CullNone: return HdCullStyleNothing;
    case MRenderItem::CullFront: return HdCullStyleFront;
    case MRenderItem::CullBack: return HdCullStyleBack;
    default: return HdCullStyleNothing;
    }
#else
    return HdCullStyleNothing;
#endif
}

bool MayaHydraRenderItemAdapter::Illuminated() const
{
    // Special case to recognize the Arnold skydome light
    if ((_isArnoldSkyDomeLightTriangleShape)) {
        return false; // Don't light the sky dome light shape
    }

    return (
        MHWRender::MGeometry::Primitive::kLines != _primitive
        && MHWRender::MGeometry::Primitive::kLineStrip != _primitive
        && MHWRender::MGeometry::Primitive::kPoints != _primitive);
}

void MayaHydraRenderItemAdapter::CreateCallbacks()
{
    MStatus status;
    auto obj = GetNode();
    auto attributesChanged = MNodeMessage::addAttributeChangedCallback(
        obj,
        +[](MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
            auto* adapter = reinterpret_cast<MayaHydraRenderItemAdapter*>(clientData);
            TF_UNUSED(otherPlug);
            if (!MayaHydraAdapter::AttributeMessageAffectsExtensionPrimvars(msg)) {
                return;
            }
            MObject node = adapter->GetNode();
            // Skip extension/dynamic primvars on camera/light render items to avoid duplicate
            // dirty notifications alongside the sprim updates.
            if (node.hasFn(MFn::kCamera) || node.hasFn(MFn::kLight)) {
                return;
            }
            adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
        },
        reinterpret_cast<void*>(this),
        &status);

    if (status) {
        AddCallback(attributesChanged);
    }
}

///////////////////////////////////////////////////////////////////////
// TF_REGISTRY
///////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraRenderItemAdapter, TfType::Bases<MayaHydraAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, renderItem) { }

PXR_NAMESPACE_CLOSE_SCOPE
