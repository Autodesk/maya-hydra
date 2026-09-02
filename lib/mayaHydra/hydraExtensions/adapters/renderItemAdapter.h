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

#ifndef MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H
#define MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/pxr.h>

#include <maya/MDagPath.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MMatrix.h>

#include <functional>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

namespace {
std::string kRenderItemTypeName = "renderItem";

static constexpr const char* kPointSize = "pointSize";

static const SdfPath kInvalidMaterial = SdfPath("InvalidMaterial");

#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
// Extract doubleSided attribute from CullMode as MRenderItem uses CullNone to denote doubleSided.
static bool IsDoubleSided(MRenderItem::CullMode cullMode) { return cullMode == MRenderItem::CullNone; }
#endif
} // namespace

using MayaHydraRenderItemAdapterPtr = std::shared_ptr<class MayaHydraRenderItemAdapter>;

/**
 * \brief MayaHydraRenderItemAdapter is used to translate from a render item to hydra.
 * This is where we translate from Maya shapes (such as meshes) to hydra.
 */
class MayaHydraRenderItemAdapter : public MayaHydraAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraRenderItemAdapter(
        const MDagPath&       dagPath,
        const SdfPath&        slowId,
        int                   fastId,
        MayaHydraSceneIndex*  mayaHydraSceneIndex,
        const MRenderItem&    ri,
        TfToken               purposeRenderTag);

    MAYAHYDRALIB_API
    virtual ~MayaHydraRenderItemAdapter();

    MAYAHYDRALIB_API
    virtual void RemovePrim() override { }

    MAYAHYDRALIB_API
    virtual void Populate() override;

    MAYAHYDRALIB_API
    bool HasType(const TfToken& typeId) const override { return typeId == HdPrimTypeTokens->mesh; }

    MAYAHYDRALIB_API
    virtual bool IsSupported() const override;

    MAYAHYDRALIB_API
    bool GetDoubleSided() const override { 
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
        return IsDoubleSided(_cullMode);
#else
        return false;
#endif
    };

    MAYAHYDRALIB_API
    GfBBox3d GetBoundingBox() override { return _bounds; }

    MAYAHYDRALIB_API
    GfVec4f GetDisplayColor() const override { return {_wireframeColor.r, _wireframeColor.g, _wireframeColor.b, _wireframeColor.a}; }

    MAYAHYDRALIB_API
    HdCullStyle GetCullStyle() const override;

    MAYAHYDRALIB_API
    VtValue Get(const TfToken& key) override;

    MAYAHYDRALIB_API
    VtValue GetMaterialResource();

    MAYAHYDRALIB_API
    void SetPlaybackState(bool isPlaybackRunning);

    MAYAHYDRALIB_API
    bool GetVisible() override;

    MAYAHYDRALIB_API
    const MColor& GetWireframeColor() const { return _wireframeColor; }

    MAYAHYDRALIB_API
    void SetWireframeColor(const MColor& color) { _wireframeColor = color; }

    MAYAHYDRALIB_API
    GfMatrix4d GetTransform() override { return _transform[0]; }

    /// Shutter-open / shutter-close transform keys captured in UpdateTransform when
    /// motion samples are enabled. Both equal GetTransform() otherwise, which the
    /// transform matrix data source reads as "no motion" and skips publishing.
    MAYAHYDRALIB_API
    GfMatrix4d GetOpenTransform() const { return _transform[2]; }

    MAYAHYDRALIB_API
    GfMatrix4d GetCloseTransform() const { return _transform[1]; }

    MAYAHYDRALIB_API
    void InvalidateTransform() { }

    MAYAHYDRALIB_API
    bool IsInstanced() const { return false; }

    MAYAHYDRALIB_API
    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override;

    MAYAHYDRALIB_API
    void UpdateTransform(const MRenderItem& ri);

    /// Class used to pass data to the UpdateFromDelta method, so we can extend the parameters in
    /// the future if needed.
    class UpdateFromDeltaData
    {
    public:
        UpdateFromDeltaData(
            MRenderItem&             ri,
            unsigned int             flags,
            const bool               wireframeColorDirty)
            : _ri(ri)
            , _flags(flags)
            , _wireframeColorDirty(wireframeColorDirty)
        {
        }

        MRenderItem&             _ri;
        unsigned int             _flags;
        const bool               _wireframeColorDirty;
    };

    /// We receive in that function the changes made in the Maya viewport between the last frame
    /// rendered and the current frame
    MAYAHYDRALIB_API
    void UpdateFromDelta(const UpdateFromDeltaData& data);

    MAYAHYDRALIB_API
    HdMeshTopology GetMeshTopology() override;

    MAYAHYDRALIB_API
    HdBasisCurvesTopology GetBasisCurvesTopology() override;

    MAYAHYDRALIB_API
    virtual TfToken GetRenderTag() const override;

    bool Illuminated() const override;

    MAYAHYDRALIB_API
    void CreateCallbacks() override;

    MAYAHYDRALIB_API
    SdfPath& GetMaterial() { return _material; }

    MAYAHYDRALIB_API
    void SetMaterial(const SdfPath& val) { _material = val; }

    MAYAHYDRALIB_API
    int GetFastID() const { return _fastId; }

    MAYAHYDRALIB_API
    const MDagPath& GetDagPath() const { return _dagPath; }

    MAYAHYDRALIB_API
    MGeometry::Primitive GetPrimitive() const { return _primitive; }

    MAYAHYDRALIB_API
    const char* Name() const { return _name.asChar(); }

    MAYAHYDRALIB_API
    void SetIsRenderITemAnaiSkydomeLightTriangleShape(bool val) {_isArnoldSkyDomeLightTriangleShape = val;}

    MAYAHYDRALIB_API
    bool GetIsRenderITemAnaiSkydomeLightTriangleShape() const {return _isArnoldSkyDomeLightTriangleShape;}

private:
    MAYAHYDRALIB_API
    void _RemoveRprim();

    MAYAHYDRALIB_API
    void _InsertRprim(MayaHydraAdapter* adapter);

    SdfPath                     _material;
    MDagPath                    _dagPath;
    std::unique_ptr<HdTopology> _topology = nullptr;
    VtVec3fArray                _positions = {};
    VtVec3fArray                _normals = {};//Are per vertex
    VtVec3fArray                _tangents = {}; //Are face varying
    VtVec2fArray                _uvs = {}; //Are face varying
    MGeometry::Primitive        _primitive;
    MString                     _name;
    // [0] = shutter centre (current frame, used for placement and GetTransform),
    // [1] = shutter close, [2] = shutter open. The open and close keys are only
    // populated when motion samples are enabled and the transform is animated
    // over the shutter; otherwise all three hold the centre transform.
    //
    // Initialised to identity because UpdateTransform only writes these when
    // MRenderItem::getMatrix() succeeds. A reader that reaches the open and
    // close keys before then would otherwise compare two uninitialised
    // matrices, find them unequal, and publish them as a motion span, which
    // transforms the geometry to an arbitrary place. Identity makes that same
    // early read report no motion instead.
    GfMatrix4d _transform[3] = { GfMatrix4d(1.0), GfMatrix4d(1.0), GfMatrix4d(1.0) };
    int                         _fastId = 0;
    bool                        _visible = false;
    MColor                      _wireframeColor = { 1.f, 1.f, 1.f, 1.f };
    bool                        _isHideOnPlayback = false;
    bool                        _isInPlayback = false;
    bool                        _isArnoldSkyDomeLightTriangleShape = false;
    GfBBox3d                    _bounds;//Bounding box
    TfToken                     _purposeRenderTag;
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    MRenderItem::CullMode       _cullMode = MRenderItem::CullNone;
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H
