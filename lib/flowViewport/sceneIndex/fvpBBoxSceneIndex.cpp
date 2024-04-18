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
// See the License for the specific language governing permissions and
// limitations under the License.
//

//Local headers
#include "fvpBBoxSceneIndex.h"
#include "flowViewport/selection/fvpSelection.h"
#include "flowViewport/fvpUtils.h"

//USD/Hydra headers
#include <pxr/base/gf/bbox3d.h>
#include <pxr/usdImaging/usdImaging/modelSchema.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/primOriginSchema.h>


// This class is a filtering scene index that converts the geometries into a bounding box using the extent attribute. 
// If the extent attribute is not present, we draw nothing, so an extent attribute must exist on all primitives for this mode to be supported correctly.

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
    TfTokenVector
    _Concat(const TfTokenVector &a, const TfTokenVector &b)
    {
        TfTokenVector result;
        result.reserve(a.size() + b.size());
        result.insert(result.end(), a.begin(), a.end());
        result.insert(result.end(), b.begin(), b.end());
        return result;
    }

    /// Base class for container data sources providing primvars.
    ///
    /// Provides primvars common to bounding boxes display:
    /// - displayColor (computed by querying displayColor from the prim data source).
    ///
    class _PrimvarsDataSource : public HdContainerDataSource
    {
    public:

        TfTokenVector GetNames() override {
            return {HdTokens->displayColor};
        }

        HdDataSourceBaseHandle Get(const TfToken &name) override {
            if (name == HdTokens->displayColor) {
                HdDataSourceBaseHandle const src =
                    Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec3fArray>::New(
                                                VtVec3fArray{{_wireframeColor[0], _wireframeColor[1], _wireframeColor[2]}}),
                                                HdPrimvarSchemaTokens->constant,
                                                HdPrimvarSchemaTokens->color);
                return src;
            }
            return nullptr;
        }

    protected:
        _PrimvarsDataSource(
            const HdContainerDataSourceHandle &primSource, const GfVec4f& wireframeColor)
          : _primSource(primSource),
            _wireframeColor(wireframeColor)
        {
        }

        HdContainerDataSourceHandle _primSource;
        GfVec4f _wireframeColor;
    };

    /// Base class for prim data sources.
    ///
    /// Provides:
    /// - xform (from the given prim data source)
    /// - purpose (from the given prim data source)
    /// - visibility (from the given prim data source)
    /// - displayStyle (constant)
    /// - instancedBy
    /// - primOrigin (for selection picking to work on usd prims in bounding box display mode)
    ///
    class _PrimDataSource : public HdContainerDataSource
    {
    public:

        TfTokenVector GetNames() override {
            return {
                HdXformSchemaTokens->xform,
                HdPurposeSchemaTokens->purpose,
                HdVisibilitySchemaTokens->visibility,
                HdInstancedBySchemaTokens->instancedBy,
                HdLegacyDisplayStyleSchemaTokens->displayStyle,
                HdPrimOriginSchemaTokens->primOrigin};
        }

        HdDataSourceBaseHandle Get(const TfToken &name) override {
            if (name == HdXformSchemaTokens->xform ||
                name == HdPurposeSchemaTokens->purpose ||
                name == HdVisibilitySchemaTokens->visibility ||
                name == HdInstancedBySchemaTokens->instancedBy ||
                name == HdPrimOriginSchemaTokens->primOrigin) {
                if (_primSource) {
                    return _primSource->Get(name);
                }
                return nullptr;
            }
            if (name == HdLegacyDisplayStyleSchemaTokens->displayStyle) {
                static const HdDataSourceBaseHandle src =
                    HdLegacyDisplayStyleSchema::Builder()
                        .SetCullStyle(
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                HdCullStyleTokens->nothing))//No culling
                        .Build();
                return src;
            }
            return nullptr;
        }

    protected:
        _PrimDataSource(const HdContainerDataSourceHandle &primSource)
          : _primSource(primSource)
        {
        }

        HdContainerDataSourceHandle _primSource;
    };

    /// Data source for primvars:points:primvarValue
    ///
    /// Computes 8 vertices of a box determined by extent of a given prim
    /// data source.
    ///
    class _BoundsPointsPrimvarValueDataSource final : public HdVec3fArrayDataSource
    {
    public:
        HD_DECLARE_DATASOURCE(_BoundsPointsPrimvarValueDataSource);

        VtValue GetValue(Time shutterOffset) {
            return VtValue(GetTypedValue(shutterOffset));
        }

        VtVec3fArray GetTypedValue(Time shutterOffset) {
            // Get extent from given prim source.
            HdExtentSchema extentSchema =
                HdExtentSchema::GetFromParent(_primSource);

            GfVec3f exts[2] = { GfVec3f(0.0f), GfVec3f(0.0f) };
            bool extentMinFound = false;
            if (HdVec3dDataSourceHandle src = extentSchema.GetMin()) {
                exts[0] = GfVec3f(src->GetTypedValue(shutterOffset));
                extentMinFound = true;
            }
            bool extentMaxFound = false;
            if (HdVec3dDataSourceHandle src = extentSchema.GetMax()) {
                exts[1] = GfVec3f(src->GetTypedValue(shutterOffset));
                extentMaxFound = true;
            }

            if (!extentMinFound || !extentMaxFound) {
                // If extent is not given, no bounding box will be displayed
                return VtVec3fArray();
            }

            /// Compute 8 points on box.
            VtVec3fArray pts(8);
            size_t i = 0;
            for (size_t j0 = 0; j0 < 2; j0++) {
                for (size_t j1 = 0; j1 < 2; j1++) {
                    for (size_t j2 = 0; j2 < 2; j2++) {
                        pts[i] = { exts[j0][0], exts[j1][1], exts[j2][2] };
                        ++i;
                    }
                }
            }

            return pts;
        }

        bool GetContributingSampleTimesForInterval(
            Time startTime,
            Time endTime,
            std::vector<Time> * outSampleTimes)
        {
            HdExtentSchema extentSchema =
                HdExtentSchema::GetFromParent(_primSource);

            HdSampledDataSourceHandle srcs[] = {
                extentSchema.GetMin(), extentSchema.GetMax() };

            return HdGetMergedContributingSampleTimesForInterval(
                TfArraySize(srcs), srcs,
                startTime, endTime, outSampleTimes);
        }

    private:
        _BoundsPointsPrimvarValueDataSource(
            const HdContainerDataSourceHandle &primSource)
          : _primSource(primSource)
        {
        }

        HdContainerDataSourceHandle _primSource;
    };

    /// Data source for primvars.
    ///
    /// Provides (on top of the base class):
    /// - points (using the above data source)
    ///
    class _BoundsPrimvarsDataSource final : public _PrimvarsDataSource
    {
    public:
        HD_DECLARE_DATASOURCE(_BoundsPrimvarsDataSource)

        TfTokenVector GetNames() override {
            static const TfTokenVector result = _Concat(
                _PrimvarsDataSource::GetNames(),
                { HdPrimvarsSchemaTokens->points });
            return result;
        }

        HdDataSourceBaseHandle Get(const TfToken &name) override {
            if (name == HdPrimvarsSchemaTokens->points) {
                return Fvp::PrimvarDataSource::New(
                    _BoundsPointsPrimvarValueDataSource::New(_primSource),
                    HdPrimvarSchemaTokens->vertex,
                    HdPrimvarSchemaTokens->point);
            }
            return _PrimvarsDataSource::Get(name);
        }

    private:
        _BoundsPrimvarsDataSource(const HdContainerDataSourceHandle &primSource, const GfVec4f& wireframeColor)
          : _PrimvarsDataSource(primSource, wireframeColor)
        {
        }
    };

    HdContainerDataSourceHandle
    _ComputeBoundsTopology()
    {
        //Is for a bounding box
        // Segments: CCW bottom face starting at (-x, -y, -z)
        //           CCW top face starting at (-x, -y, z)
        //           CCW vertical edges, starting at (-x, -y)
        const VtIntArray curveIndices{
                /* bottom face */ 0, 4, 4, 6, 6, 2, 2, 0,
                /* top face */    1, 5, 5, 7, 7, 3, 3, 1,
                /* edge pairs */  0, 1, 4, 5, 6, 7, 2, 3 };
        const VtIntArray curveVertexCounts{
                static_cast<int>(curveIndices.size()) };
    
        return HdBasisCurvesTopologySchema::Builder()
            .SetCurveVertexCounts(
                    HdRetainedTypedSampledDataSource<VtIntArray>::New(
                        curveVertexCounts))
            .SetCurveIndices(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(
                    curveIndices))
            .SetBasis(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    HdTokens->bezier))
            .SetType(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    HdTokens->linear))
            .SetWrap(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    HdTokens->segmented))
            .Build();
    }

    /// Prim data source.
    ///
    /// Provides (on top of the base class):
    /// - basisCurves (constant using above topology)
    /// - primvars (using above data source)
    /// - extent (from the original prim source)
    ///
    class _BoundsPrimDataSource : public _PrimDataSource
    {
    public:
        HD_DECLARE_DATASOURCE(_BoundsPrimDataSource)

        TfTokenVector GetNames() override {
            static const TfTokenVector result = _Concat(
                _PrimDataSource::GetNames(),
                { HdBasisCurvesSchemaTokens->basisCurves,
                  HdPrimvarsSchemaTokens->primvars,
                  HdExtentSchemaTokens->extent });
            return result;
        }

        HdDataSourceBaseHandle Get(const TfToken &name) override {
            if (name == HdBasisCurvesSchemaTokens->basisCurves) {
                static const HdDataSourceBaseHandle basisCurvesSrc =
                    HdBasisCurvesSchema::Builder()
                    .SetTopology(_ComputeBoundsTopology())
                    .Build();
                return basisCurvesSrc;
            }
            if (name == HdPrimvarsSchemaTokens->primvars) {
                return _BoundsPrimvarsDataSource::New(_primSource, _wireframeColor);
            }
            if (name == HdExtentSchemaTokens->extent) {
                if (_primSource) {
                    return _primSource->Get(name);
                }
                return nullptr;
            }
            return _PrimDataSource::Get(name);
        }

    private:
        _BoundsPrimDataSource(
            const HdContainerDataSourceHandle &primSource, const GfVec4f& wireframeColor)
          : _PrimDataSource(primSource),
            _wireframeColor(wireframeColor)
        {
        }

        GfVec4f _wireframeColor;
    };
}

BboxSceneIndex::BboxSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<const Selection>& selection) : 
    ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex),
    _selection(selection)
{
}

HdSceneIndexPrim BboxSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (prim.dataSource && ! _isExcluded(primPath) && ((prim.primType == HdPrimTypeTokens->mesh) || (prim.primType == HdPrimTypeTokens->basisCurves)) ){
        prim.primType   = HdPrimTypeTokens->basisCurves;//Convert to basisCurve for displaying a bounding box
        const GfVec4f wireframeColor = _selection->GetWireframeColor(primPath);
        prim.dataSource = _BoundsPrimDataSource::New(prim.dataSource, wireframeColor);
    }

    return prim;
}

void BboxSceneIndex::_PrimsAdded(const HdSceneIndexBase& sender, const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved())return;

    HdSceneIndexObserver::AddedPrimEntries newEntries;
    for (const HdSceneIndexObserver::AddedPrimEntry &entry : entries) {
        const SdfPath &path = entry.primPath;
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(path);
        if (prim.primType == HdPrimTypeTokens->mesh){
            newEntries.push_back({path, HdPrimTypeTokens->basisCurves});//Convert meshes to basisCurve to display a bounding box
        }else{
            newEntries.push_back(entry);
        }
    }

    _SendPrimsAdded(newEntries);
}

}//end of namespace FVP_NS_DEF
