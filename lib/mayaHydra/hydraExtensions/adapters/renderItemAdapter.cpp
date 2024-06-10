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
#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MHWGeometry.h>
#include <maya/MShaderManager.h>
#include <maya/MViewport2Renderer.h>

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
    const MRenderItem&    ri)
    : MayaHydraAdapter(MObject(), slowId, mayaHydraSceneIndex)
    , _dagPath(dagPath)
    , _primitive(ri.primitive())
    , _name(ri.name())
    , _fastId(fastId)
{
    _InsertRprim(this);
}

MayaHydraRenderItemAdapter::~MayaHydraRenderItemAdapter() { _RemoveRprim(); }

TfToken MayaHydraRenderItemAdapter::GetRenderTag() const { return HdRenderTagTokens->geometry; }

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
        return GetMayaHydraSceneIndex()->GetRenderIndex().IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        return GetMayaHydraSceneIndex()->GetRenderIndex().IsRprimTypeSupported(HdPrimTypeTokens->basisCurves);
    case MHWRender::MGeometry::Primitive::kPoints:
        return GetMayaHydraSceneIndex()->GetRenderIndex().IsRprimTypeSupported(HdPrimTypeTokens->points);
    default: return false;
    }
}

void MayaHydraRenderItemAdapter::_InsertRprim(MayaHydraAdapter* adapter)
{
    switch (GetPrimitive()) {
    case MHWRender::MGeometry::Primitive::kTriangles:
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
        assert(false); // unexpected/unsupported primitive type
        break;
    }
}

void MayaHydraRenderItemAdapter::_RemoveRprim() { GetMayaHydraSceneIndex()->RemovePrim(GetID());} 

// We receive in that function the changes made in the Maya viewport between the last frame rendered
// and the current frame
void MayaHydraRenderItemAdapter::UpdateFromDelta(const UpdateFromDeltaData& data)
{
    if (_primitive != MHWRender::MGeometry::Primitive::kTriangles
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
    const bool geomChanged      = (data._flags & MVS::MVS_changedGeometry) || positionsHaveBeenReset;
    const bool topoChanged      = (data._flags & MVS::MVS_changedTopo) || positionsHaveBeenReset;
    const bool visibChanged     = data._flags & MVS::MVS_changedVisibility;
    const bool effectChanged    = data._flags & MVS::MVS_changedEffect;

    HdDirtyBits dirtyBits = 0;

    if (data._wireframeColor != _wireframeColor) {
        _wireframeColor = data._wireframeColor;
        dirtyBits |= HdChangeTracker::DirtyPrimvar; // displayColor primVar
    }

    const bool hideOnPlayback = data._ri.isHideOnPlayback();
    if (hideOnPlayback != _isHideOnPlayback) {
        _isHideOnPlayback = hideOnPlayback;
        dirtyBits |= HdChangeTracker::DirtyVisibility;
    }

    if (visibChanged) {
        SetVisible(visible);
        dirtyBits |= HdChangeTracker::DirtyVisibility;
    }

    if (effectChanged) {
        dirtyBits |= HdChangeTracker::DirtyMaterialId;
    }
    if (matrixChanged) {
        dirtyBits |= HdChangeTracker::DirtyTransform;
    }
    if (geomChanged) {
        dirtyBits |= (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyExtent | HdChangeTracker::DirtyNormals);
    }
    if (topoChanged) {
        dirtyBits |= (HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyPrimvar | HdChangeTracker::DirtyExtent);
    }

    MGeometry* geom = nullptr;
    if (geomChanged || topoChanged) {
        geom = data._ri.geometry();
        auto bbox = data._ri.boundingBox();
        const MPoint& min = bbox.min();
        const MPoint& max = bbox.max();
        _bounds.SetRange(GfRange3d({min.x, min.y, min.z}, {max.x, max.y, max.z}));
    }
    VtIntArray vertexIndices;
    VtIntArray vertexCounts;

    static const bool passNormalsToHydra = MayaHydraSceneIndex::passNormalsToHydra();
        
    const int vertexBuffercount = geom ? geom->vertexBufferCount() : 0;
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
                    if (passNormalsToHydra){
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
                default:
                break;
            }
        }
    }

    // Indices
    if (topoChanged && vertexBuffercount) {
        // Assume first stream contains the positions.
        MIndexBuffer* indices = geom->indexBuffer(0);
        if (indices) {
            int indexCount = indices->size();
            vertexIndices.resize(indexCount);
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

            switch (GetPrimitive()) {
            case MHWRender::MGeometry::Primitive::kTriangles:
                vertexCounts.resize(indexCount / 3);
                vertexCounts.assign(indexCount / 3, 3);

                // Get face varying data from Maya like uvs
                if (indexCount > 0) {
                    
                    MVertexBuffer* mvb = nullptr;

                    for (int vbIdx = 0; vbIdx < vertexBuffercount; vbIdx++) {
                        mvb = geom->vertexBuffer(vbIdx);
                        if (!mvb)
                            continue;

                        const MVertexBufferDescriptor& desc = mvb->descriptor();
                        const auto semantic = desc.semantic();
                        switch(semantic){
                            case MGeometry::Semantic::kTexture: {
                                // Hydra supports a uv coordinate for each face-index (face varying), though we could use its own set of indices which should be smaller.
                                _uvs.clear();
                                _uvs.resize(indices->size());
                                float* uvs = (float*)mvb->map();
                                for (int i = 0; i < indexCount; ++i){
                                    _uvs[i].Set(&uvs[indicesData[i] * 2]);
                                }
                                mvb->unmap();
                            }
                            break;
                            case MHWRender::MGeometry::kTangent:{
                                // Hydra supports a tangent for each face-index (face varying), though we could use its own set of indices which should be smaller.
                                _tangents.clear();
                                _tangents.resize(indices->size());
                                float* tangents = (float*)mvb->map();
                                for (int i = 0; i < indexCount; ++i){
                                    _tangents[i].Set(&tangents[indicesData[i] * 2]);
                                }
                                mvb->unmap();
                            }
                            break;
                            default:
                            break;
                        }
                    }
                }
                break;
            case MHWRender::MGeometry::Primitive::kLines:
                vertexCounts.resize(indexCount);
                vertexCounts.assign(indexCount / 2, 2);
                break;

            default:
                assert(false); // unexpected/unsupported primitive type
                break;
            }
            indices->unmap();
        }
    }

    if (topoChanged) {
        switch (GetPrimitive()) {
        case MGeometry::Primitive::kTriangles:{
            static const bool passNormalsToHydra = MayaHydraSceneIndex::passNormalsToHydra();
            if (passNormalsToHydra){
                _topology.reset(new HdMeshTopology(
                    PxOsdOpenSubdivTokens->none,//For the OGS normals vertex buffer to be used, we need to use PxOsdOpenSubdivTokens->none
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            } else{
                _topology.reset(new HdMeshTopology(
                    (GetMayaHydraSceneIndex()->GetParams().displaySmoothMeshes
                     || GetDisplayStyle().refineLevel > 0)
                        ? PxOsdOpenSubdivTokens->catmullClark
                        : PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            }
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

    MarkDirty(dirtyBits);
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

    return {};
}

void MayaHydraRenderItemAdapter::MarkDirty(HdDirtyBits dirtyBits)
{
    if (dirtyBits != 0) {
        GetMayaHydraSceneIndex()->MarkRprimDirty(GetID(), dirtyBits);
    }
}

HdPrimvarDescriptorVector
MayaHydraRenderItemAdapter::GetPrimvarDescriptors(HdInterpolation interpolation)
{
    HdPrimvarDescriptor desc;

    // Vertices
    if (interpolation == HdInterpolationVertex) {
        static const bool passNormalsToHydra = MayaHydraSceneIndex::passNormalsToHydra();
        return  passNormalsToHydra ? 
        HdPrimvarDescriptorVector{
            {UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point},//Vertices
            {UsdGeomTokens->normals, interpolation, HdPrimvarRoleTokens->normal}//Normals
        } : 
        HdPrimvarDescriptorVector{
            {UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point}//Vertices only
        };
    } 
    else if (interpolation == HdInterpolationFaceVarying) {
        // UVs and tangents are face varying in maya.
        if (_primitive == MGeometry::Primitive::kTriangles) {
            return  
            HdPrimvarDescriptorVector{
                {MayaHydraAdapterTokens->st, interpolation, HdPrimvarRoleTokens->textureCoordinate},//uvs
                {MayaHydraAdapterTokens->tangents, interpolation, HdPrimvarRoleTokens->textureCoordinate},//tangents
            };
        }
    } else if (interpolation == HdInterpolationConstant) {
        switch(_primitive){
            case MGeometry::Primitive::kPoints: //Fall into
            case MGeometry::Primitive::kLines: //Fall into
            case MGeometry::Primitive::kLineStrip: //Fall into
            case MGeometry::Primitive::kAdjacentLines: //Fall into
            case MGeometry::Primitive::kAdjacentLineStrip:
            {
                desc.name           = HdTokens->displayColor;//Use display color only for lines/points (avoid triangles)
                desc.interpolation  = interpolation;
                desc.role           = HdPrimvarRoleTokens->color;
                return { desc };
            }
            break;
            default:
            break;
        }
    }

    return {};
}

VtValue MayaHydraRenderItemAdapter::GetMaterialResource() { return {}; }

bool MayaHydraRenderItemAdapter::GetVisible()
{
    // Assuming that, if the playback is in the active view only
    // (MAnimControl::kPlaybackViewActive), we are called because we are in the active view
    if (_isHideOnPlayback) {
        // MAYA-127216: Remove dependency on parent class MayaHydraAdapter. This will let us use
        // MayaHydraSceneDelegate directly
        auto mayaHydraSceneIndex = static_cast<MayaHydraSceneIndex*>(GetMayaHydraSceneIndex());
        return !mayaHydraSceneIndex->GetPlaybackRunning();
    }

    return _visible;
}

void MayaHydraRenderItemAdapter::SetPlaybackChanged()
{
    // There was a change in the playblack state, it started or stopped running so update any
    // primitive that is dependent on this
    if (_isHideOnPlayback) {
        MarkDirty(HdChangeTracker::DirtyVisibility);
    }
}

HdCullStyle MayaHydraRenderItemAdapter::GetCullStyle() const
{
    // HdCullStyleNothing means no culling, HdCullStyledontCare means : let the renderer choose
    // between back or front faces culling. We don't want culling, since we want to see the
    // backfaces being unlit with MayaHydraSceneDelegate::GetDoubleSided returning false.
    return _isArnoldSkyDomeLightTriangleShape ? HdCullStyleFront : HdCullStyleNothing;
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
