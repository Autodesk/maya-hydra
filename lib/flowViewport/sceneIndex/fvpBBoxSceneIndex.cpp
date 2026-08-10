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
#include "flowViewport/fvpUtils.h"
#include "flowViewport/fvpPurposeRenderTagsForPasses.h"

//USD/Hydra headers
#include <pxr/base/gf/bbox3d.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
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
#include <pxr/imaging/hd/sceneIndexPrimView.h>

// This class is a filtering scene index that converts the geometries into a bounding box using the extent attribute. 
// If the extent attribute is not present, we draw nothing, so an extent attribute must exist on all primitives for this mode to be supported correctly.

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{

    //Prims on which we need to change the geometry into a bounding box
    const std::set<TfToken> kSupportedPrimTypes = {
        HdPrimTypeTokens->mesh,
        HdPrimTypeTokens->basisCurves,
    };

    bool _IsSupportedPrimType(const TfToken& primType)
    {
        return kSupportedPrimTypes.find(primType) != kSupportedPrimTypes.cend();
    }


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
                name == HdVisibilitySchemaTokens->visibility ||
                name == HdInstancedBySchemaTokens->instancedBy ||
                name == HdPrimOriginSchemaTokens->primOrigin) {
                if (_primSource) {
                    return _primSource->Get(name);
                }
                return nullptr;
            }
            
            // Force the purpose render tag to be in the secondary graphics for bounding box display mode
            if (name == HdPurposeSchemaTokens->purpose) {
                return HdPurposeSchema::Builder()
                            .SetPurpose(HdRetainedTypedSampledDataSource<TfToken>::New(
                                Fvp::secondaryGraphicsRenderTagToken))
                            .Build();
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
        inline static const GfVec3f _floatMaxVec3f {FLT_MAX};
    public:
        HD_DECLARE_DATASOURCE(_BoundsPointsPrimvarValueDataSource);

        VtValue GetValue(Time shutterOffset) {
            return VtValue(GetTypedValue(shutterOffset));
        }

        VtVec3fArray GetTypedValue(Time shutterOffset) {
            // Get extent from given prim source.
            HdExtentSchema extentSchema =
                HdExtentSchema::GetFromParent(_primSource);

            // Note: If the scene description doesn't provide the extents, Storm uses
            // the default constructed GfRange3d which is [FLT_MAX, -FLT_MAX],
            GfVec3f exts[2] = { GfVec3f(0.5f), GfVec3f(-0.5f) };
            if (HdVec3dDataSourceHandle src = extentSchema.GetMin()) {
                auto minExt = GfVec3f(src->GetTypedValue(shutterOffset));
                if (minExt != _floatMaxVec3f) {
                    exts[0] = minExt;
                }
            }
            if (HdVec3dDataSourceHandle src = extentSchema.GetMax()) {
                auto maxExt = GfVec3f(src->GetTypedValue(shutterOffset));
                if (maxExt != -_floatMaxVec3f) {
                    exts[1] = maxExt;
                }
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
                  HdExtentSchemaTokens->extent
                });
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

BboxSceneIndex::BboxSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface) : 
    ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    TF_AXIOM(_wireframeColorInterface);
}

void BboxSceneIndex::Enable(bool enable) 
{ 
    if (enable != _enabled) {
        _enabled = enable;
        _DirtyAllPrims();
    }
}

void BboxSceneIndex::_DirtyAllPrims()
{
    if (!_IsObserved()) {
        return;
    }

    // Instead of just dirtying, let's re-add all prims
    // This forces a complete resync of the prims with the new topology
    HdSceneIndexObserver::AddedPrimEntries   addedEntries;

    for (const SdfPath& path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(path);

        // Check if this prim is supported for conversion (regardless of enabled state)
        if (prim.dataSource && !_isExcluded(path) && _IsSupportedPrimType(prim.primType)) {
            // Re-add it with the appropriate type based on current enabled state
            if (_enabled) {
                // Convert to bounding box
                addedEntries.emplace_back(path, HdPrimTypeTokens->basisCurves);
            } else {
                // Convert back to original type
                addedEntries.emplace_back(path, prim.primType);
            }
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
}

void BboxSceneIndex::_PrimsDirtied(
    const PXR_NS::HdSceneIndexBase&                         sender,
   const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) {
        return;
    }

    // When bbox mode is off (the common case), nothing needs translating.
    if (!_enabled) {
        _SendPrimsDirtied(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries transformedEntries;
    transformedEntries.reserve(entries.size());

    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);

        // Only transform entries for prims that we're actually converting
        if (prim.dataSource && _IsSupportedPrimType(prim.primType)
            && !_isExcluded(entry.primPath)) {

            // Transform the dirty locators to basisCurves equivalents
            HdDataSourceLocatorSet transformedLocators;

            for (const auto& locator : entry.dirtyLocators) {
                // Transform mesh-specific locators to basisCurves equivalents
                if (locator.HasPrefix(HdMeshSchema::GetDefaultLocator())) {
                    // Convert mesh topology locators to basisCurves topology locators
                    transformedLocators.insert(HdBasisCurvesSchema::GetDefaultLocator());
                }else {
                    // For any other locators, just keep them as-is
                    transformedLocators.insert(locator);
                }
            }

            transformedEntries.emplace_back(entry.primPath, transformedLocators);
        } else {
            // For prims we're not converting, just pass through the original entry
            transformedEntries.emplace_back(entry);
        }
    }

    _SendPrimsDirtied(transformedEntries);
}

// Modify the helper functions to take the prim as a parameter
bool BboxSceneIndex::_ShouldConvertToBoundingBox(
    const SdfPath&          primPath,
    const HdSceneIndexPrim& prim) const
{
    // Check if bounding box mode is enabled
    if (!_enabled) {
        return false;
    }

    // Check if the prim path is excluded
    if (_isExcluded(primPath)) {
        return false;
    }

    // Check if the prim has a data source
    if (!prim.dataSource) {
        return false;
    }

    // Check if the prim type is supported for conversion
    if (!_IsSupportedPrimType(prim.primType)) {
        return false;
    }

    return true;
}

// Helper function to create the bounding box data source
HdContainerDataSourceHandle BboxSceneIndex::_CreateBoundingBoxDataSource(
    const SdfPath&          primPath,
    const HdSceneIndexPrim& prim) const
{
    if (!prim.dataSource) {
        return nullptr;
    }

    // Get the wireframe color for this prim
    const GfVec4f wireframeColor = _wireframeColorInterface->getWireframeColor(primPath);

    // Create the bounding box data source using the existing _BoundsPrimDataSource
    return _BoundsPrimDataSource::New(prim.dataSource, wireframeColor);
}

// Update the calling functions
HdSceneIndexPrim BboxSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // Use the helper function to check if we should convert this prim
    if (_ShouldConvertToBoundingBox(primPath, prim)) {
        prim.primType = HdPrimTypeTokens->basisCurves;
        prim.dataSource = _CreateBoundingBoxDataSource(primPath, prim);
    }

    return prim;
}

void BboxSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved())
        return;

    if (!_enabled) {
        _SendPrimsAdded(entries);
        return;
    }

    HdSceneIndexObserver::AddedPrimEntries transformedEntries;

    for (const auto& entry : entries) {
        const SdfPath& primPath = entry.primPath;
        const TfToken& originalPrimType = entry.primType;

        // Get the prim once
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

        // Check if this prim should be converted to a bounding box
        if (_ShouldConvertToBoundingBox(primPath, prim)) {
            transformedEntries.emplace_back(primPath, HdPrimTypeTokens->basisCurves);
        } else {
            transformedEntries.emplace_back(primPath, originalPrimType);
        }
    }

    _SendPrimsAdded(transformedEntries);
}

}//end of namespace FVP_NS_DEF
