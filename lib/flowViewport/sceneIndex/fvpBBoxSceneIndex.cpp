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
#include <pxr/imaging/hd/meshTopologySchema.h>
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
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>

#include <algorithm>

// This class is a filtering scene index that converts the geometries into a bounding box using the extent attribute. 
// If the extent attribute is not present, we draw nothing, so an extent attribute must exist on all primitives for this mode to be supported correctly.

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
    TF_DEFINE_PRIVATE_TOKENS(
        _primvarsTokens,
        (overrideWireframeColor)
    );

    const TfToken extentsHintBoundsToken("extentsHintBounds");

    HdContainerDataSourceHandle _OverlayExtent(
        const HdContainerDataSourceHandle& primSource,
        const GfRange3d& extent)
    {
        const HdContainerDataSourceHandle extentOverlay
            = HdRetainedContainerDataSource::New(
                HdExtentSchemaTokens->extent,
                HdExtentSchema::Builder()
                    .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(
                        extent.GetMin()))
                    .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(
                        extent.GetMax()))
                    .Build());
        return HdOverlayContainerDataSource::New(extentOverlay, primSource);
    }

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
            return {HdTokens->displayColor, _primvarsTokens->overrideWireframeColor};
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
            if (name == _primvarsTokens->overrideWireframeColor) {
                return Fvp::PrimvarDataSource::New(
                    HdRetainedTypedSampledDataSource<VtVec4fArray>::New(
                        VtVec4fArray{_wireframeColor}),
                    HdPrimvarSchemaTokens->constant,
                    HdPrimvarSchemaTokens->color);
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
                        .SetReprSelector(
                            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                                {HdReprTokens->wire, TfToken(), TfToken()}))
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

    HdContainerDataSourceHandle _ComputeBoundsTopology()
    {
        const VtIntArray faceVertexCounts{4, 4, 4, 4, 4, 4};
        const VtIntArray faceVertexIndices{
            0, 4, 6, 2,
            1, 3, 7, 5,
            0, 1, 5, 4,
            2, 6, 7, 3,
            0, 2, 3, 1,
            4, 5, 7, 6};

        return HdMeshTopologySchema::Builder()
            .SetFaceVertexCounts(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(faceVertexCounts))
            .SetFaceVertexIndices(
                HdRetainedTypedSampledDataSource<VtIntArray>::New(faceVertexIndices))
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
            TfTokenVector result = _Concat(
                _PrimDataSource::GetNames(),
                { HdMeshSchemaTokens->mesh,
                  HdPrimvarsSchemaTokens->primvars,
                  HdExtentSchemaTokens->extent
                });
            if (_isExtentsHintBounds) {
                result.push_back(extentsHintBoundsToken);
            }
            return result;
        }

        HdDataSourceBaseHandle Get(const TfToken &name) override {
            if (name == HdMeshSchemaTokens->mesh) {
                static const HdDataSourceBaseHandle meshSrc =
                    HdMeshSchema::Builder()
                    .SetTopology(_ComputeBoundsTopology())
                    .SetDoubleSided(
                        HdRetainedTypedSampledDataSource<bool>::New(true))
                    .Build();
                return meshSrc;
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
            if (name == extentsHintBoundsToken) {
                return _isExtentsHintBounds
                    ? HdRetainedTypedSampledDataSource<bool>::New(true)
                    : nullptr;
            }

            return _PrimDataSource::Get(name);
        }

    private:
        _BoundsPrimDataSource(
            const HdContainerDataSourceHandle &primSource,
            const GfVec4f& wireframeColor,
            bool isExtentsHintBounds)
          : _PrimDataSource(primSource),
            _wireframeColor(wireframeColor),
            _isExtentsHintBounds(isExtentsHintBounds)
        {
        }

        GfVec4f _wireframeColor;
        bool _isExtentsHintBounds;
    };
}

BboxSceneIndex::BboxSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
    Mode mode,
    PrimPredicate primPredicate,
    WireframeColorProvider wireframeColorProvider,
    ExtentProvider extentProvider,
    ChildPrimPathsProvider childPrimPathsProvider,
    PrimDataSourceProvider primDataSourceProvider)
    : ParentClass(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _wireframeColorInterface(wireframeColorInterface)
    , _mode(mode)
    , _primPredicate(std::move(primPredicate))
    , _wireframeColorProvider(std::move(wireframeColorProvider))
    , _extentProvider(std::move(extentProvider))
    , _childPrimPathsProvider(std::move(childPrimPathsProvider))
    , _primDataSourceProvider(std::move(primDataSourceProvider))
{
    TF_AXIOM(_wireframeColorInterface || _wireframeColorProvider);
}

void BboxSceneIndex::Enable(bool enable)
{
    if (_mode == Mode::Geometries && enable != _enabled) {
        _enabled = enable;
        _DirtyAllPrims();
    }
}

void BboxSceneIndex::_DirtyAllPrims()
{
    if (!_IsObserved()) {
        return;
    }

    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    for (const SdfPath& path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(path);
        if (prim.dataSource && !_isExcluded(path) && _IsSupportedPrimType(prim.primType)) {
            addedEntries.emplace_back(
                path, _enabled ? HdPrimTypeTokens->mesh : prim.primType);
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
}

bool BboxSceneIndex::_ShouldConvert(
    const SdfPath& primPath, const HdSceneIndexPrim& prim) const
{
    if (!prim.dataSource || _isExcluded(primPath)) {
        return false;
    }

    if (_mode == Mode::Geometries) {
        return _enabled && _IsSupportedPrimType(prim.primType);
    }

    if (prim.dataSource->Get(extentsHintBoundsToken)) {
        return true;
    }

    if (!prim.primType.IsEmpty() || !_primPredicate || !_primPredicate(primPath)) {
        return false;
    }

    HdExtentSchema extent = HdExtentSchema::GetFromParent(prim.dataSource);
    if (extent.GetMin() && extent.GetMax()) {
        return true;
    }
    return _extentProvider && _extentProvider(primPath).has_value();
}

HdContainerDataSourceHandle BboxSceneIndex::_CreateBoundingBoxDataSource(
    const SdfPath& primPath, const HdSceneIndexPrim& prim) const
{
    if (!prim.dataSource) {
        return nullptr;
    }

    const GfVec4f wireframeColor = _wireframeColorProvider
        ? _wireframeColorProvider(primPath)
        : _wireframeColorInterface->getWireframeColor(primPath);
    return _BoundsPrimDataSource::New(
        prim.dataSource,
        wireframeColor,
        _mode == Mode::EmptyModelsWithExtents);
}

HdSceneIndexPrim BboxSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!prim.dataSource && _primDataSourceProvider) {
        prim.dataSource = _primDataSourceProvider(primPath);
    }
    if (_ShouldConvert(primPath, prim)) {
        if (_extentProvider) {
            HdExtentSchema extent = HdExtentSchema::GetFromParent(prim.dataSource);
            if (!extent.GetMin() || !extent.GetMax()) {
                if (const auto suppliedExtent = _extentProvider(primPath)) {
                    prim.dataSource = _OverlayExtent(prim.dataSource, *suppliedExtent);
                }
            }
        }
        prim.primType = HdPrimTypeTokens->mesh;
        prim.dataSource = _CreateBoundingBoxDataSource(primPath, prim);
    }
    return prim;
}

SdfPathVector BboxSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    SdfPathVector result = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    if (_childPrimPathsProvider) {
        const SdfPathVector supplied = _childPrimPathsProvider(primPath);
        for (const SdfPath& path : supplied) {
            if (std::find(result.begin(), result.end(), path) == result.end()) {
                result.push_back(path);
            }
        }
    }
    return result;
}

namespace {
std::set<SdfPath> _ComputeSyntheticPrimPaths(
    const BboxSceneIndex::ChildPrimPathsProvider& childPrimPathsProvider,
    const BboxSceneIndex::PrimDataSourceProvider& primDataSourceProvider,
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const std::function<bool(const SdfPath&, const HdSceneIndexPrim&)>& shouldConvert)
{
    std::set<SdfPath> newPaths;
    if (childPrimPathsProvider) {
        for (const SdfPath& path
             : childPrimPathsProvider(SdfPath::AbsoluteRootPath())) {
            HdSceneIndexPrim prim = inputSceneIndex->GetPrim(path);
            if (!prim.dataSource && primDataSourceProvider) {
                prim.dataSource = primDataSourceProvider(path);
            }
            if (shouldConvert(path, prim)) {
                newPaths.insert(path);
            }
        }
    }
    return newPaths;
}
} // namespace

void BboxSceneIndex::RemoveStaleSyntheticPrims(bool sendNotifications)
{
    const std::set<SdfPath> newPaths = _ComputeSyntheticPrimPaths(
        _childPrimPathsProvider,
        _primDataSourceProvider,
        GetInputSceneIndex(),
        [this](const SdfPath& path, const HdSceneIndexPrim& prim) {
            return _ShouldConvert(path, prim);
        });

    if (sendNotifications && _IsObserved()) {
        HdSceneIndexObserver::RemovedPrimEntries removed;
        for (const SdfPath& path : _syntheticPrimPaths) {
            if (newPaths.count(path) == 0) {
                removed.emplace_back(path);
            }
        }
        if (!removed.empty()) {
            _SendPrimsRemoved(removed);
        }
    }

    for (auto it = _syntheticPrimPaths.begin(); it != _syntheticPrimPaths.end();) {
        if (newPaths.count(*it) == 0) {
            it = _syntheticPrimPaths.erase(it);
        } else {
            ++it;
        }
    }
}

void BboxSceneIndex::AddNewSyntheticPrims(bool sendNotifications)
{
    const std::set<SdfPath> newPaths = _ComputeSyntheticPrimPaths(
        _childPrimPathsProvider,
        _primDataSourceProvider,
        GetInputSceneIndex(),
        [this](const SdfPath& path, const HdSceneIndexPrim& prim) {
            return _ShouldConvert(path, prim);
        });

    if (sendNotifications && _IsObserved()) {
        HdSceneIndexObserver::AddedPrimEntries added;
        for (const SdfPath& path : newPaths) {
            if (_syntheticPrimPaths.count(path) == 0) {
                added.emplace_back(path, GetPrim(path).primType);
            }
        }
        if (!added.empty()) {
            _SendPrimsAdded(added);
        }
    }

    _syntheticPrimPaths.insert(newPaths.begin(), newPaths.end());
}

void BboxSceneIndex::RefreshSyntheticPrims(bool sendNotifications)
{
    RemoveStaleSyntheticPrims(sendNotifications);
    AddNewSyntheticPrims(sendNotifications);
}

void BboxSceneIndex::_PrimsAdded(
    const HdSceneIndexBase& sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved()) {
        return;
    }

    HdSceneIndexObserver::AddedPrimEntries transformedEntries;
    transformedEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        transformedEntries.emplace_back(
            entry.primPath,
            _ShouldConvert(entry.primPath, prim) ? HdPrimTypeTokens->mesh : entry.primType);
    }
    _SendPrimsAdded(transformedEntries);
}

void BboxSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase& sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) {
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries transformedEntries;
    transformedEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        HdDataSourceLocatorSet dirtyLocators = entry.dirtyLocators;
        if (dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_ShouldConvert(entry.primPath, prim)) {
                dirtyLocators.insert(
                    HdPrimvarsSchema::GetDefaultLocator().Append(HdTokens->displayColor));
                dirtyLocators.insert(
                    HdPrimvarsSchema::GetDefaultLocator().Append(
                        _primvarsTokens->overrideWireframeColor));
            }
        }
        transformedEntries.emplace_back(entry.primPath, std::move(dirtyLocators));
    }
    _SendPrimsDirtied(transformedEntries);
}

}//end of namespace FVP_NS_DEF
