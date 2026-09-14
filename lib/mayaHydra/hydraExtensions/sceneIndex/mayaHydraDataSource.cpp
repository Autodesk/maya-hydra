//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#include "mayaHydraDataSource.h"

#include <mayaHydraLib/sceneIndex/mayaHydraDisplayStyleDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraPrimvarDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraCameraDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraLightDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraCustomNodeDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexUtils.h>
#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/customDagAdapter.h>
#include <mayaHydraLib/adapters/dagAdapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/tokens.h>

#include <cmath>
#include <limits>

#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/subdivisionTagsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/volumeFieldSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/extentSchema.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Transform matrix data source for motion blur: publishes several matrix keys across
// the shutter when motion samples are enabled, and a single sample otherwise.
class MayaHydraTransformMatrixDataSource final : public HdMatrixDataSource
{
public:
    HD_DECLARE_DATASOURCE(MayaHydraTransformMatrixDataSource);

    MayaHydraTransformMatrixDataSource(
        MayaHydraSceneIndex* sceneIndex, MayaHydraAdapter* adapter)
        : _sceneIndex(sceneIndex)
        , _adapter(adapter)
    {
    }

    VtValue GetValue(Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    GfMatrix4d GetTypedValue(Time shutterOffset) override
    {
        // Offset 0 is the rendered frame: return the live transform, never a sample.
        // The samples come from the animation and so ignore interactive edits such as
        // tumbling an animated camera.
        if (shutterOffset == 0.0) {
            return _adapter->GetTransform();
        }
        _EnsureSamples();
        if (_count <= 1) {
            return _adapter->GetTransform();
        }
        // Consumers query at the contributing times reported below, so nearest-time
        // matching is exact for them.
        size_t best = 0;
        float  bestDist = std::numeric_limits<float>::max();
        for (size_t i = 0; i < _count; ++i) {
            const float d = std::abs(_times[i] - static_cast<float>(shutterOffset));
            if (d < bestDist) {
                bestDist = d;
                best = i;
            }
        }
        return _samples[best];
    }

    bool GetContributingSampleTimesForInterval(
        Time startTime, Time endTime,
        std::vector<Time>* outSampleTimes) override
    {
        _EnsureSamples();
        if (_count <= 1) {
            return false;
        }
        if (outSampleTimes) {
            outSampleTimes->clear();
            outSampleTimes->reserve(_count);
            for (size_t i = 0; i < _count; ++i) {
                outSampleTimes->push_back(_times[i]);
            }
        }
        return true;
    }

private:
    void _EnsureSamples()
    {
        if (_sampled) {
            return;
        }
        _sampled = true;

        if (!_sceneIndex || !_sceneIndex->GetParams().motionSamplesEnabled()) {
            _count = 0;
            return;
        }

        // DAG-backed prims (mesh / light / camera) re-evaluate under MDGContextGuard.
        if (MayaHydraDagAdapter* dagAdapter = dynamic_cast<MayaHydraDagAdapter*>(_adapter)) {
            _count = dagAdapter->SampleTransform(kMotionKeys, _times, _samples);
            return;
        }

        // Render items are not DAG adapters: UpdateTransform captured their shutter
        // keys, so publish those as a two-sample span when they differ.
        if (MayaHydraRenderItemAdapter* riAdapter
            = dynamic_cast<MayaHydraRenderItemAdapter*>(_adapter)) {
            const GfMatrix4d open  = riAdapter->GetOpenTransform();
            const GfMatrix4d close = riAdapter->GetCloseTransform();
            if (open == close) {
                _count = 0;
                return;
            }
            const GfInterval shutter = _sceneIndex->GetCurrentTimeSamplingInterval();
            _times[0]   = static_cast<float>(shutter.GetMin());
            _samples[0] = open;
            _times[1]   = static_cast<float>(shutter.GetMax());
            _samples[1] = close;
            _count      = 2;
            return;
        }

        _count = 0;
    }

    static constexpr size_t kMotionKeys = 3;

    MayaHydraSceneIndex* _sceneIndex { nullptr };
    MayaHydraAdapter*    _adapter { nullptr };
    bool                 _sampled { false };
    size_t               _count { 0 };
    float                _times[kMotionKeys] {};
    GfMatrix4d           _samples[kMotionKeys] {};
};

}

MayaHydraDataSource::MayaHydraDataSource(
    const SdfPath& id,
    TfToken type,
    MayaHydraSceneIndex* sceneIndex,
    MayaHydraAdapter* adapter)
    : _id(id)
    , _type(type)
    , _sceneIndex(sceneIndex)
    , _adapter(adapter)
{
}

TfTokenVector
MayaHydraDataSource::GetNames()
{
    TfTokenVector result;
    

    if (_type == HdPrimTypeTokens->mesh) {
        result.push_back(HdMeshSchemaTokens->mesh);
        result.push_back(HdExtentSchemaTokens->extent); //Add an extent attribute to support the bounding box display style
    }

    if (_type == HdPrimTypeTokens->basisCurves) {
        result.push_back(HdBasisCurvesSchemaTokens->basisCurves);
    }

    result.push_back(HdPrimvarsSchemaTokens->primvars);

    // As per
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/d3991f70df7d70ad7b7d2485c23a90ef8d05342b/pxr/imaging/hd/tokens.cpp#L68
    // the following covers meshes, basis curves, points, and volumes.
    if (HdPrimTypeIsGprim(_type)) {
        result.push_back(HdMaterialBindingsSchema::GetSchemaToken());
        result.push_back(HdLegacyDisplayStyleSchemaTokens->displayStyle);
        result.push_back(HdVisibilitySchemaTokens->visibility);
        result.push_back(HdXformSchemaTokens->xform);
        result.push_back(HdPurposeSchemaTokens->purpose); // add a purpose render tag
    }

    if (HdPrimTypeIsLight(_type)) {
        result.push_back(HdMaterialSchemaTokens->material);
        result.push_back(HdXformSchemaTokens->xform);
        result.push_back(HdLightSchemaTokens->light);
        result.push_back(HdPurposeSchemaTokens->purpose); // add a purpose render tag
    }

    if (_type == HdPrimTypeTokens->material) {
        result.push_back(HdMaterialSchemaTokens->material);
    }

    if (_type == HdPrimTypeTokens->camera) {
        result.push_back(HdCameraSchemaTokens->camera);
        result.push_back(HdXformSchemaTokens->xform);
        result.push_back(HdPurposeSchemaTokens->purpose);
    }

    if (_type == MayaHydraAdapterTokens->mayaCustomDagNode) {
        result.push_back(HdXformSchemaTokens->xform);
        result.push_back(HdVisibilitySchemaTokens->visibility);
        result.push_back(MayaHydraAdapterTokens->mayaNode);
        result.push_back(HdPurposeSchemaTokens->purpose);
    }

    return result;
}

HdDataSourceBaseHandle
MayaHydraDataSource::Get(const TfToken& name)
{
    if (name == HdMeshSchemaTokens->mesh) {
        if (_type == HdPrimTypeTokens->mesh) {
            auto topology = _adapter->GetMeshTopology();
            const auto subdivTags = _adapter->GetSubdivTags();
            return HdMeshSchema::Builder()
                .SetTopology(
                    HdMeshTopologySchema::Builder()
                        .SetFaceVertexCounts(HdRetainedTypedSampledDataSource<VtIntArray>::New(
                            topology.GetFaceVertexCounts()))
                        .SetFaceVertexIndices(HdRetainedTypedSampledDataSource<VtIntArray>::New(
                            topology.GetFaceVertexIndices()))
                        .SetOrientation(HdRetainedTypedSampledDataSource<TfToken>::New(
                            HdMeshTopologySchemaTokens->rightHanded))
                        .Build())
                .SetSubdivisionScheme(
                    HdRetainedTypedSampledDataSource<TfToken>::New(topology.GetScheme()))
                .SetSubdivisionTags(
                    HdSubdivisionTagsSchema::Builder()
                        .SetFaceVaryingLinearInterpolation(
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                subdivTags.GetFaceVaryingInterpolationRule()))
                        // PxOsdSubdivTags::VertexInterpolationRule and
                        // HdSubdivisionTagsSchema::interpolateBoundary are the same value under two
                        // different names
                        .SetInterpolateBoundary(
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                subdivTags.GetVertexInterpolationRule()))
                        .SetTriangleSubdivisionRule(
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                subdivTags.GetTriangleSubdivision()))
                        .SetCornerIndices(
                            HdRetainedTypedSampledDataSource<VtIntArray>::New(
                                subdivTags.GetCornerIndices()))
                        .SetCornerSharpnesses(
                            HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                                subdivTags.GetCornerWeights()))
                        .SetCreaseIndices(
                            HdRetainedTypedSampledDataSource<VtIntArray>::New(
                                subdivTags.GetCreaseIndices()))
                        .SetCreaseLengths(
                            HdRetainedTypedSampledDataSource<VtIntArray>::New(
                                subdivTags.GetCreaseLengths()))
                        .SetCreaseSharpnesses(
                            HdRetainedTypedSampledDataSource<VtFloatArray>::New(
                                subdivTags.GetCreaseWeights()))
                        .Build())
                .SetDoubleSided(
                    HdRetainedTypedSampledDataSource<bool>::New(_adapter->GetDoubleSided()))
                .Build();
        }
    }
    else if (name == HdBasisCurvesSchemaTokens->basisCurves) {
        if (_type == HdPrimTypeTokens->basisCurves) {
            auto topology = _adapter->GetBasisCurvesTopology();
            return HdBasisCurvesSchema::Builder()
                .SetTopology(
                    HdBasisCurvesTopologySchema::Builder()
                    .SetCurveVertexCounts(
                        HdRetainedTypedSampledDataSource<VtIntArray>::New(
                            topology.GetCurveVertexCounts()))
                    .SetCurveIndices(
                        HdRetainedTypedSampledDataSource<VtIntArray>::New(
                            topology.GetCurveIndices()))
                    .SetBasis(
                        HdRetainedTypedSampledDataSource<TfToken>::New(
                            topology.GetCurveBasis()))
                    .SetType(
                        HdRetainedTypedSampledDataSource<TfToken>::New(
                            topology.GetCurveType()))
                    .SetWrap(
                        HdRetainedTypedSampledDataSource<TfToken>::New(
                            topology.GetCurveWrap()))
                    .Build())
                .Build();
        }
    }
    else if (name == HdPrimvarsSchemaTokens->primvars) {
        return _GetPrimvarsDataSource();
    }
    else if (name ==
             HdMaterialBindingsSchema::GetSchemaToken()
             ) {
       return _GetMaterialBindingDataSource();
    }
    else if (name == HdXformSchemaTokens->xform) {
        // Fall back to a retained sample when motion blur is off, so static scenes pay
        // nothing for the sampling wrapper. SetParams re-queries this when the motion
        // params change, so toggling motion blur on still installs the sampling source.
        HdMatrixDataSourceHandle matrixDs;
        if (_sceneIndex && _sceneIndex->GetParams().motionSamplesEnabled()) {
            matrixDs = MayaHydraTransformMatrixDataSource::New(_sceneIndex, _adapter);
        }
        else {
            matrixDs = HdRetainedTypedSampledDataSource<GfMatrix4d>::New(
                _adapter->GetTransform());
        }
        return HdXformSchema::Builder().SetMatrix(matrixDs).Build();
    }
    else if (name == HdMaterialSchemaTokens->material) {
       return _GetMaterialDataSource();
    }
    else if (name == HdLegacyDisplayStyleSchemaTokens->displayStyle) {
        return MayaHydraDisplayStyleDataSource::New(_id, _type, _sceneIndex, _adapter);
    }
    else if (name == HdVisibilitySchemaTokens->visibility) {
        return _GetVisibilityDataSource();
    }
    else if (name == HdCameraSchemaTokens->camera) {
        return MayaHydraCameraDataSource::New(_id, _type, _adapter);
    }
    else if (name == HdLightSchemaTokens->light) {
        return MayaHydraLightDataSource::New(_id, _type, _adapter);
    }
    else if (name == HdExtentSchemaTokens->extent) {//Extent attribute to support the bounding box display style
        GfBBox3d bbox = _adapter->GetBoundingBox();
        return HdExtentSchema::Builder()
            .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(bbox.GetRange().GetMin()))
            .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(bbox.GetRange().GetMax()))
            .Build();
    }
    else if (name == HdTokens->displayColor) {//Is not part of a schema so using HdTokens->displayColor
        return _GetDisplayColorDataSource();
    } else if (name == HdPurposeSchemaTokens->purpose && ! (_adapter->GetRenderTag().IsEmpty()) ) { 
        return HdPurposeSchema::Builder()
                    .SetPurpose(HdRetainedTypedSampledDataSource<TfToken>::New(
                        _adapter->GetRenderTag()))
                    .Build();
    } else if (name == MayaHydraAdapterTokens->mayaNode
               && _type == MayaHydraAdapterTokens->mayaCustomDagNode) {
        auto* customAdapter = dynamic_cast<MayaHydraCustomDagAdapter*>(_adapter);
        if (customAdapter) {
            return MayaHydraCustomNodeDataSource::New(customAdapter);
        }
    }

    return nullptr;
}

HdDataSourceBaseHandle MayaHydraDataSource::_GetVisibilityDataSource()
{
    bool vis = _adapter->GetVisible();
    if (vis) {
        static const HdContainerDataSourceHandle visOn =
            HdVisibilitySchema::BuildRetained(
                HdRetainedTypedSampledDataSource<bool>::New(true));
        return visOn;
    }
    else {
        static const HdContainerDataSourceHandle visOff =
            HdVisibilitySchema::BuildRetained(
                HdRetainedTypedSampledDataSource<bool>::New(false));
        return visOff;
    }
}

HdDataSourceBaseHandle MayaHydraDataSource::_GetDisplayColorDataSource()
{
    return HdRetainedTypedSampledDataSource<GfVec4f>::New(_adapter->GetDisplayColor());
}

HdDataSourceBaseHandle MayaHydraDataSource::_GetPrimvarsDataSource()
{
    MayaHydraPrimvarsDataSourceHandle primvarsDs;

    for (size_t interpolation = HdInterpolationConstant;
        interpolation < HdInterpolationCount; ++interpolation) {

        HdPrimvarDescriptorVector v = _adapter->GetPrimvarDescriptors((HdInterpolation)interpolation);

        TfToken interpolationToken = _InterpolationAsToken(
            (HdInterpolation)interpolation);

        for (const auto& primvarDesc : v) {
            if (!primvarsDs) {
                primvarsDs = MayaHydraPrimvarsDataSource::New(_adapter);
            }
            primvarsDs->AddDesc(
                primvarDesc.name, interpolationToken, primvarDesc.role,
                primvarDesc.indexed);
        }
    }

    return primvarsDs;
}

TfToken MayaHydraDataSource::_InterpolationAsToken(HdInterpolation interpolation)
{
    switch (interpolation) {
    case HdInterpolationConstant:
        return HdPrimvarSchemaTokens->constant;
    case HdInterpolationUniform:
        return HdPrimvarSchemaTokens->uniform;
    case HdInterpolationVarying:
        return HdPrimvarSchemaTokens->varying;
    case HdInterpolationVertex:
        return HdPrimvarSchemaTokens->vertex;
    case HdInterpolationFaceVarying:
        return HdPrimvarSchemaTokens->faceVarying;
    case HdInterpolationInstance:
        return HdPrimvarSchemaTokens->instance;

    default:
        return HdPrimvarSchemaTokens->constant;
    }
}

HdDataSourceBaseHandle
MayaHydraDataSource::_GetMaterialBindingDataSource()
{
    const SdfPath path = _sceneIndex->GetMaterialId(_id);
    if (path.IsEmpty()) {
        return nullptr;
    }
    static const TfToken purposes[] = {
        HdMaterialBindingsSchemaTokens->allPurpose
    };
    HdDataSourceBaseHandle const materialBindingSources[] = {
        HdMaterialBindingSchema::Builder()
            .SetPath(
                HdRetainedTypedSampledDataSource<SdfPath>::New(path))
            .Build()
    };

    return
        HdMaterialBindingsSchema::BuildRetained(
            TfArraySize(purposes), purposes, materialBindingSources);
}

HdDataSourceBaseHandle
MayaHydraDataSource::_GetMaterialDataSource()
{    
    VtValue materialContainer = _sceneIndex->GetMaterialResource(_id);

    if (!materialContainer.IsHolding<HdMaterialNetworkMap>()) {
        return nullptr;
    }

    HdMaterialNetworkMap hdNetworkMap = 
        materialContainer.UncheckedGet<HdMaterialNetworkMap>();
    HdContainerDataSourceHandle materialDS = nullptr;    
    if (!_ConvertHdMaterialNetworkToHdDataSources(
        hdNetworkMap,
        &materialDS) ) {
        return nullptr;
    }
    return materialDS;
}
PXR_NAMESPACE_CLOSE_SCOPE
