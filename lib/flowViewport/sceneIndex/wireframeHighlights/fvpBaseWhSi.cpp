// Copyright 2025 Autodesk
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

#include "fvpBaseWhSi.h"

#include <flowViewport/fvpUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSourceMaterialNetworkInterface.h>
#include <pxr/imaging/hd/dependenciesSchema.h>
#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/smoothNormals.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/vertexAdjacency.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/usdImaging/usdImaging/directMaterialBindingsSchema.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>

#include <unordered_set>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
//Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
TF_DEFINE_PRIVATE_TOKENS(
     _primVarsTokens,
 
     (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
 );

const HdRetainedContainerDataSourceHandle refinedWireDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWire, TfToken(), TfToken() })));

const HdDataSourceLocator reprSelectorLocator(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdLegacyDisplayStyleSchemaTokens->reprSelector);

const HdDataSourceLocator primvarsOverrideWireframeColorLocator(
        HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor));

const HdDataSourceLocator pointsValueLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->points,
    HdPrimvarSchemaTokens->primvarValue);

const HdDataSourceLocator normalsPrimvarLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->normals);

const HdDataSourceLocator normalsValueLocator = HdDataSourceLocator(
    HdPrimvarsSchemaTokens->primvars,
    HdPrimvarsSchemaTokens->normals,
    HdPrimvarSchemaTokens->primvarValue);

// Returns all paths related to instancing for this prim; this is analogous to getting the edges
// connected to the given vertex (in this case a prim) of an instancing graph.
SdfPathVector _GetInstancingRelatedPaths(const HdSceneIndexPrim& prim, Fvp::InstancingPathsCollectionDirection direction)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    
    SdfPathVector instancingRelatedPaths;

    if ((direction & Fvp::InstancingPathsCollectionDirection::Prototypes)
        && instancerTopology.IsDefined()) {
        auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
        for (const auto& protoPath : protoPaths) {
            instancingRelatedPaths.push_back(protoPath);
        }
    }

    if ((direction & Fvp::InstancingPathsCollectionDirection::InstancedBy)
        && instancedBy.IsDefined()) {
        auto instancerPaths = instancedBy.GetPaths()->GetTypedValue(0);
        for (const auto& instancerPath : instancerPaths) {
            instancingRelatedPaths.push_back(instancerPath);
        }

        // Having a prototype root is not a hard requirement (a single prim being instanced
        // does not need to specify itself as its own prototype root).
        if (instancedBy.GetPrototypeRoots()) {
            auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
            for (const auto& protoRootPath : protoRootPaths) {
                instancingRelatedPaths.push_back(protoRootPath);
            }
        }
    }

    return instancingRelatedPaths;
}

bool _IsPrototype(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined();
}

bool _IsPrototypeSubPrim(const HdSceneIndexPrim& prim, const SdfPath& primPath)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBy.IsDefined()) {
        return false;
    }
    if (!instancedBy.GetPrototypeRoots()) {
        return false;
    }
    auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
    for (const auto& protoRootPath : protoRootPaths) {
        if (protoRootPath == primPath) {
            return false;
        }
    }
    return true;
}

// We consider prototypes that have child prims to be different hierarchies,
// separate from each other and from the "root" hierarchy.
VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
}

// Copied over from USD's rerootingSceneIndex.cpp
class _RerootingSceneIndexPathDataSource : public HdPathDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathDataSource)

    _RerootingSceneIndexPathDataSource(
        const SdfPath &srcPrefix,
        const SdfPath &dstPrefix,
        HdPathDataSourceHandle const &inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
    {
    }

    VtValue GetValue(const Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    bool GetContributingSampleTimesForInterval(
        const Time startTime,
        const Time endTime,
        std::vector<Time> * const outSampleTimes) override
    {
        if (!_inputDataSource) {
            return false;
        }

        return _inputDataSource->GetContributingSampleTimesForInterval(
                startTime, endTime, outSampleTimes);
    }

    SdfPath GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return SdfPath();
        }

        const SdfPath srcPath = _inputDataSource->GetTypedValue(shutterOffset);
        return srcPath.ReplacePrefix(_srcPrefix, _dstPrefix);
    }

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdPathDataSourceHandle const _inputDataSource;
};

// Copied over from USD's rerootingSceneIndex.cpp
class _RerootingSceneIndexPathArrayDataSource : public HdPathArrayDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathArrayDataSource)

    _RerootingSceneIndexPathArrayDataSource(
        const SdfPath& srcPrefix,
        const SdfPath& dstPrefix,
        HdPathArrayDataSourceHandle const & inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
    {
    }

    VtValue GetValue(const Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    bool GetContributingSampleTimesForInterval(
        const Time startTime,
        const Time endTime,
        std::vector<Time>*  const outSampleTimes) override
    {
        if (!_inputDataSource) {
            return false;
        }

        return _inputDataSource->GetContributingSampleTimesForInterval(
            startTime, endTime, outSampleTimes);
    }

    VtArray<SdfPath> GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return {};
        }

        VtArray<SdfPath> result
            = _inputDataSource->GetTypedValue(shutterOffset);

        const size_t n = result.size();

        if (n == 0) {
            return result;
        }

        size_t i = 0;

        // If _srcPrefix is absolute root path, we know that we
        // need to translate every path.
        if (!_srcPrefix.IsAbsoluteRootPath()) {
            // Find the first element where we need to change the path.
            //
            // Use const & so that paths[i] does not trigger VtArray
            // to make a copy.
            const VtArray<SdfPath> &paths = result.AsConst();
            while (!paths[i].HasPrefix(_srcPrefix)) {
                ++i;
                if (i == n) {
                    // No need to modify result if no path needed
                    // to be changed.
                    return result;
                }
            }
        }

        // Starting with the first element where the path matched the
        // prefix, process it and all following elements.
        for (; i < n; i++) {
            SdfPath &path = result[i];
            path = path.ReplacePrefix(_srcPrefix, _dstPrefix);
        }

        return result;
    }

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdPathArrayDataSourceHandle const _inputDataSource;
};

//TODO :: Move to FVP namespace
#if PXR_VERSION >= 2403
// Edits the mesh topology to only only contain its selected GeomSubsets
HdContainerDataSourceHandle
_TrimMeshForGeomSubset(const HdContainerDataSourceHandle& meshRootDataSource, const HdContainerDataSourceHandle& geomSubsetRootDataSource)
{
    HdMeshSchema meshSchema = HdMeshSchema::GetFromParent(meshRootDataSource);
    if (!meshSchema.IsDefined()) {
        return meshRootDataSource;
    }
    HdMeshTopologySchema meshTopologySchema = meshSchema.GetTopology();
    if (!meshTopologySchema.IsDefined()) {
        return meshRootDataSource;
    }
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, pointsValueLocator));
    if (!pointsValueDataSource) {
        return meshRootDataSource;
    }

    // Collect faces to keep based on the GeomSubset
    std::unordered_set<int> faceIndicesToKeep;
    #if HD_API_VERSION >= 71 // USD 24.08+
        HdGeomSubsetSchema geomSubsetSchema = HdGeomSubsetSchema::GetFromParent(geomSubsetRootDataSource);
    #else
        HdGeomSubsetSchema geomSubsetSchema = HdGeomSubsetSchema(geomSubsetRootDataSource);
    #endif
    if (!geomSubsetSchema.IsDefined() || geomSubsetSchema.GetType()->GetTypedValue(0) != HdGeomSubsetSchemaTokens->typeFaceSet) {
        return meshRootDataSource;
    }
    VtArray<int> faceIndices = geomSubsetSchema.GetIndices()->GetTypedValue(0);
    for (const auto& faceIndex : faceIndices) {
        faceIndicesToKeep.insert(faceIndex);
    }
    if (faceIndicesToKeep.empty()) {
        return meshRootDataSource;
    }

    // Edit the mesh topology
    HdContainerDataSourceEditor dataSourceEditor = HdContainerDataSourceEditor(meshRootDataSource);
    VtArray<int> originalFaceVertexCounts = meshTopologySchema.GetFaceVertexCounts()->GetTypedValue(0);
    VtArray<int> originalFaceVertexIndices = meshTopologySchema.GetFaceVertexIndices()->GetTypedValue(0);
    VtArray<int> trimmedFaceVertexCounts;
    VtArray<int> trimmedFaceVertexIndices;
    int maxVertexIndex = 0;
    size_t iFaceCounts = 0;
    size_t iFaceIndices = 0;
    while (iFaceCounts < originalFaceVertexCounts.size() && iFaceIndices < originalFaceVertexIndices.size()) {
        int currFaceCount = originalFaceVertexCounts[iFaceCounts];

        if (faceIndicesToKeep.find(iFaceCounts) != faceIndicesToKeep.end()) {
            trimmedFaceVertexCounts.push_back(currFaceCount);
            for (int faceIndicesOffset = 0; faceIndicesOffset < currFaceCount; faceIndicesOffset++) {
                int vertexIndex = originalFaceVertexIndices[iFaceIndices + faceIndicesOffset];
                trimmedFaceVertexIndices.push_back(vertexIndex);
                if (vertexIndex > maxVertexIndex) {
                    maxVertexIndex = vertexIndex;
                }
            }
        }

        iFaceCounts++;
        iFaceIndices += currFaceCount;
    }
    auto faceVertexCountsLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexCounts);
    auto faceVertexIndicesLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexIndices);
    
    dataSourceEditor.Set(faceVertexCountsLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexCounts));
    dataSourceEditor.Set(faceVertexIndicesLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexIndices));

    // We reduce the points and normals primvars so that they have only the exact number of elements required by the trimmed topology;
    // this avoids a warning from USD.
    VtArray<GfVec3f> points = pointsValueDataSource->GetTypedValue(0);
    points.resize(maxVertexIndex + 1);
    dataSourceEditor.Set(pointsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(points));

    auto normalsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, normalsValueLocator));
    if (normalsValueDataSource) {
        auto normals = normalsValueDataSource->GetTypedValue(0);
        normals.resize(maxVertexIndex + 1);
        dataSourceEditor.Set(normalsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(normals));
    }

    return dataSourceEditor.Finish();
}
#endif

}

namespace FVP_NS_DEF {

TfTokenVector RepathingContainerDataSource::GetNames()
{
    if (!_inputDataSource) {
        return {};
    }

    return _inputDataSource->GetNames();
}

HdDataSourceBaseHandle RepathingContainerDataSource::Get(const TfToken& name)
{
    if (!_inputDataSource) {
        return nullptr;
    }

    // wrap child containers so that we can wrap their children
    HdDataSourceBaseHandle const childSource = _inputDataSource->Get(name);
    if (!childSource) {
        return nullptr;
    }

    if (auto childContainer =
            HdContainerDataSource::Cast(childSource)) {
        return New(_srcPrefix, _dstPrefix, std::move(childContainer));
    }

    if (auto childPathDataSource =
            HdTypedSampledDataSource<SdfPath>::Cast(childSource)) {
        return _RerootingSceneIndexPathDataSource::New(
            _srcPrefix, _dstPrefix, childPathDataSource);
    }

    if (auto childPathArrayDataSource =
            HdTypedSampledDataSource<VtArray<SdfPath>>::Cast(
                childSource)) {
        return _RerootingSceneIndexPathArrayDataSource::New(
            _srcPrefix, _dstPrefix, childPathArrayDataSource);
    }

    return childSource;
}

//We want to set the displayStyle of the selected prim to refinedWireOnSurf only if the displayStyle of the prim is refined (meaning shaded)
HdContainerDataSourceHandle SetWireframeRepr(const HdContainerDataSourceHandle& dataSource, const GfVec4f& color)
{
    //Always edit its override wireframe color
    auto edited = HdContainerDataSourceEditor(dataSource);
    edited.Set(primvarsOverrideWireframeColorLocator,
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(VtVec4fArray{color}),
                            HdPrimvarSchemaTokens->constant,
                            HdPrimvarSchemaTokens->color));
    
    //Is the prim in refined displayStyle (meaning shaded) ?
    if (HdLegacyDisplayStyleSchema styleSchema =
            HdLegacyDisplayStyleSchema::GetFromParent(dataSource)) {

        if (HdTokenArrayDataSourceHandle ds =
                styleSchema.GetReprSelector()) {
            VtArray<TfToken> ar = ds->GetTypedValue(0.0f);
            TfToken refinedToken = ar[0];
            if(HdReprTokens->refined == refinedToken){
                //Is in refined display style, apply the wire on top of shaded reprselector
                return HdOverlayContainerDataSource::New({ edited.Finish(), refinedWireDisplayStyleDataSource});
            }
        }else{
            //No reprSelector found, assume it's in the Collection that we have set HdReprTokens->refined
            return HdOverlayContainerDataSource::New({ edited.Finish(), refinedWireDisplayStyleDataSource});
        }
    }

    //For the other case, we are only updating the wireframe color assuming we are already drawing lines
    return edited.Finish();
}

#if PXR_VERSION >= 2403
HdContainerDataSourceHandle
TrimMeshForGeomSubset(const HdContainerDataSourceHandle& meshRootDataSource, const HdContainerDataSourceHandle& geomSubsetRootDataSource)
{
    return _TrimMeshForGeomSubset(meshRootDataSource, geomSubsetRootDataSource);
}
#endif

PXR_NS::HdContainerDataSourceHandle ComputeSmoothNormals(const PXR_NS::HdContainerDataSourceHandle& meshRootDataSource)
{
    // Check if normals are already present
    auto normalsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, normalsValueLocator));
    if (normalsValueDataSource) {
        return meshRootDataSource;
    }

    // Get required schemas/dataSources
    HdMeshSchema meshSchema = HdMeshSchema::GetFromParent(meshRootDataSource);
    if (!meshSchema.IsDefined()) {
        return meshRootDataSource;
    }
    HdMeshTopologySchema meshTopologySchema = meshSchema.GetTopology();
    if (!meshTopologySchema.IsDefined()) {
        return meshRootDataSource;
    }
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, pointsValueLocator));
    if (!pointsValueDataSource) {
        return meshRootDataSource;
    }

    // Setup topology
    TfToken subdivScheme = PxOsdOpenSubdivTokens->none;
    if (HdTokenDataSourceHandle subdivSchemeDataSource = meshSchema.GetSubdivisionScheme()) {
        subdivScheme = subdivSchemeDataSource->GetTypedValue(0.0f);
    }
    VtIntArray holeIndices;
    if (HdIntArrayDataSourceHandle holeIndicesDataSource = meshTopologySchema.GetHoleIndices()) {
        holeIndices = holeIndicesDataSource->GetTypedValue(0.0f);
    }
    TfToken orientation = PxOsdOpenSubdivTokens->rightHanded;
    if (HdTokenDataSourceHandle orientationDataSource = meshTopologySchema.GetOrientation()) {
        orientation = orientationDataSource->GetTypedValue(0.0f);
    }
    HdMeshTopology meshTopology(
        subdivScheme, 
        orientation, 
        meshTopologySchema.GetFaceVertexCounts()->GetTypedValue(0), 
        meshTopologySchema.GetFaceVertexIndices()->GetTypedValue(0), 
        holeIndices);
    Hd_VertexAdjacency adjacency;
    adjacency.BuildAdjacencyTable(&meshTopology);

    // Compute normals
    auto points = pointsValueDataSource->GetTypedValue(0);
    auto normals = Hd_SmoothNormals::ComputeSmoothNormals(
        &adjacency, static_cast<int>(points.size()), points.cdata());

    // Apply normals
    auto normalsDataSource = HdPrimvarSchema::Builder()
        .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
        .SetRole(HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->normal))
        .SetPrimvarValue(HdRetainedTypedSampledDataSource<decltype(normals)>::New(normals))
        .Build();
    HdDataSourceLocator normalsPrimvarLocator = HdDataSourceLocator(HdPrimvarsSchemaTokens->primvars, HdPrimvarsSchemaTokens->normals);
    HdContainerDataSourceEditor dataSourceEditor(meshRootDataSource);
    dataSourceEditor.Set(normalsPrimvarLocator, normalsDataSource);
    return dataSourceEditor.Finish();
}

PXR_NS::HdContainerDataSourceHandle ForceDisplacement(const PXR_NS::HdContainerDataSourceHandle& meshRootDataSource, float displacement)
{
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, pointsValueLocator));
    if (!pointsValueDataSource) {
        return meshRootDataSource;
    }
    auto normalsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, normalsValueLocator));
    if (!normalsValueDataSource) {
        return meshRootDataSource;
    }

    auto points = pointsValueDataSource->GetTypedValue(0);
    auto normals = normalsValueDataSource->GetTypedValue(0);

    if (points.size() > normals.size()) {
        return meshRootDataSource;
    }

    // Compute displacement
    for (size_t iPoint = 0; iPoint < points.size(); iPoint++) {
        points[iPoint] += normals[iPoint] * displacement;
    }

    // Apply displaced points
    HdContainerDataSourceEditor dataSourceEditor(meshRootDataSource);
    dataSourceEditor.Set(pointsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(points));

    // Block materials to prevent displacement from being re-applied
    dataSourceEditor.Set(HdMaterialBindingsSchema::GetDefaultLocator(), HdBlockDataSource::New());
    dataSourceEditor.Set(UsdImagingDirectMaterialBindingsSchema::GetDefaultLocator(), HdBlockDataSource::New());

    return dataSourceEditor.Finish();
}

PXR_NS::HdContainerDataSourceHandle ForceScale(const PXR_NS::HdContainerDataSourceHandle& meshRootDataSource)
{
    auto xformSchema = HdXformSchema::GetFromParent(meshRootDataSource);
    if (!xformSchema.IsDefined() || !xformSchema.GetMatrix()) {
        return meshRootDataSource;
    }
    GfMatrix4d xformMatrix = xformSchema.GetMatrix()->GetTypedValue(0);

    HdContainerDataSourceEditor dataSourceEditor(meshRootDataSource);

    // Scale points
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, pointsValueLocator));
    if (pointsValueDataSource) {
        auto points = pointsValueDataSource->GetTypedValue(0);
        for (size_t iPoint = 0; iPoint < points.size(); iPoint++) {
            points[iPoint] = GfVec3f(xformMatrix.Transform(points[iPoint]));
        }
        dataSourceEditor.Set(pointsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(points));
    }

    // Scale normals
    auto normalsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(meshRootDataSource, normalsValueLocator));
    if (normalsValueDataSource) {
        auto normals = normalsValueDataSource->GetTypedValue(0);
        for (size_t iNormal = 0; iNormal < normals.size(); iNormal++) {
            normals[iNormal] = GfVec3f(xformMatrix.GetInverse().GetTranspose().TransformDir(normals[iNormal]));
            normals[iNormal].Normalize();
        }
        dataSourceEditor.Set(normalsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(normals));
    }

    // Set xform to the identity to prevent scaling from being re-applied
    dataSourceEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(GfMatrix4d(1)));

    return dataSourceEditor.Finish();
}

SdfPath GetMaterialPath(const PXR_NS::HdContainerDataSourceHandle& primDataSource)
{
    if (!primDataSource) {
        return {};
    }

    HdMaterialBindingsSchema materialBindingsSchema = HdMaterialBindingsSchema::GetFromParent(primDataSource);
    if (!materialBindingsSchema.IsDefined()) {
        return {};
    }

    HdMaterialBindingSchema materialBindingSchema = materialBindingsSchema.GetMaterialBinding();
    if (!materialBindingSchema) {
        return {};
    }

    HdPathDataSourceHandle bindingPathDataSource = materialBindingSchema.GetPath();
    if (!bindingPathDataSource) {
        return {};
    }

    return bindingPathDataSource->GetTypedValue(0);
}

PXR_NS::HdContainerDataSourceHandle AddDependency(
    const PXR_NS::HdContainerDataSourceHandle& primDataSource,
    const PXR_NS::TfToken& dependencyToken,
    const PXR_NS::SdfPath& dependedOnPrimPath,
    const PXR_NS::HdDataSourceLocator& dependedOnDataSourceLocator,
    const PXR_NS::HdDataSourceLocator& affectedDataSourceLocator)
{
    HdDependencySchema::Builder builder;

    if (!dependedOnPrimPath.IsEmpty()) {
        builder.SetDependedOnPrimPath(HdRetainedTypedSampledDataSource<SdfPath>::New(dependedOnPrimPath));
    }

    builder.SetDependedOnDataSourceLocator(HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(dependedOnDataSourceLocator));

    builder.SetAffectedDataSourceLocator(HdRetainedTypedSampledDataSource<HdDataSourceLocator>::New(affectedDataSourceLocator));

    auto dependencyDataSource = HdRetainedContainerDataSource::New(dependencyToken, builder.Build());

    HdContainerDataSourceEditor dataSourceEditor(primDataSource);
    dataSourceEditor.Overlay(HdDependenciesSchema::GetDefaultLocator(), dependencyDataSource);
    return dataSourceEditor.Finish();
}

PXR_NS::HdContainerDataSourceHandle RepathInstancingDataSources(
    const PXR_NS::HdContainerDataSourceHandle& primDataSource, 
    const PXR_NS::SdfPath& srcPrefix, 
    const PXR_NS::SdfPath& dstPrefix)
{
    static const std::set<HdDataSourceLocator> kInstancingDataSourceLocators = {
        HdInstancerTopologySchema::GetDefaultLocator(),
        HdInstancedBySchema::GetDefaultLocator(),
        HdInstanceSchema::GetDefaultLocator(),
        UsdImagingUsdPrimInfoSchema::GetDefaultLocator().Append(UsdImagingUsdPrimInfoSchemaTokens->piPropagatedPrototypes),
        UsdImagingUsdPrimInfoSchema::GetDefaultLocator().Append(UsdImagingUsdPrimInfoSchemaTokens->niPrototypePath),
    };

    HdContainerDataSourceEditor dataSourceEditor(primDataSource);
    for (const auto& instancingDataSourceLocator : kInstancingDataSourceLocators) {
        auto instancingDataSource = HdContainerDataSource::Get(primDataSource, instancingDataSourceLocator);
        
        if (auto containerDataSource = HdContainerDataSource::Cast(instancingDataSource)) {
            dataSourceEditor.Set(
                instancingDataSourceLocator,
                RepathingContainerDataSource::New(srcPrefix, dstPrefix, containerDataSource)
            );
        }

        if (auto pathDataSource = HdTypedSampledDataSource<SdfPath>::Cast(instancingDataSource)) {
            dataSourceEditor.Set(
                instancingDataSourceLocator,
                _RerootingSceneIndexPathDataSource::New(srcPrefix, dstPrefix, pathDataSource)
            );
        }

        if (auto pathArrayDataSource = HdTypedSampledDataSource<VtArray<SdfPath>>::Cast(instancingDataSource)) {
            dataSourceEditor.Set(
                instancingDataSourceLocator,
                _RerootingSceneIndexPathArrayDataSource::New(srcPrefix, dstPrefix, pathArrayDataSource)
            );
        }
    }
    return dataSourceEditor.Finish();
}

Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
    Fvp::PrimSelection primSelection;
    primSelection.primPath = primPath;

    auto nestedInstanceIndicesSchema = 
#if HD_API_VERSION < 66
    const_cast<HdSelectionSchema&>(selectionSchema).GetNestedInstanceIndices();
#else
    selectionSchema.GetNestedInstanceIndices();
#endif
    for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
        HdInstanceIndicesSchema instanceIndicesSchema = nestedInstanceIndicesSchema.GetElement(iNestedInstanceIndices);
        auto instanceIndices = instanceIndicesSchema.GetInstanceIndices()->GetTypedValue(0);
        primSelection.nestedInstanceIndices.push_back(
            {
                instanceIndicesSchema.GetInstancer()->GetTypedValue(0),
                instanceIndicesSchema.GetPrototypeIndex()->GetTypedValue(0),
                std::vector<int>(instanceIndices.begin(), instanceIndices.end())
            }
        );
    }

    return primSelection;
}

BaseWhSi::BaseWhSi(
    const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
    const PXR_NS::SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _highlightHierarchyPrefix(highlightHierarchyPrefix),
    _wireframeColorInterface(wireframeColorInterface)
{
    _excludedPaths.emplace(_highlightHierarchyPrefix);

    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected() && !selectionsSchema.GetElement(selectionId).GetNestedInstanceIndices()) {
                    _fullySelectedPaths.emplace(primPath);
                    break;
                }
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

HdSceneIndexPrim BaseWhSi::GetPrim(const PXR_NS::SdfPath &primPath) const
{
    if (primPath.HasPrefix(_highlightHierarchyPrefix) && !_selectionPaths.empty()) {
        auto it = FindSelfOrFirstParent(primPath, _selectionPaths);
        bool isHighlightPrim = it != _selectionPaths.end() && *it != primPath;
        if (isHighlightPrim) {
            auto selectionPath = primPath;
            while (_selectionPaths.find(selectionPath) == _selectionPaths.end()) {
                selectionPath = selectionPath.GetParentPath();
            }
            return GetHighlightPrim(selectionPath, primPath);
        }
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector BaseWhSi::GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const
{
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    if (primPath == SdfPath::AbsoluteRootPath()) {
        childPaths.emplace_back(_highlightHierarchyPrefix);
        return childPaths;
    }
    if (primPath.HasPrefix(_highlightHierarchyPrefix) && !_selectionPaths.empty()) {
        // To return the paths leading up to and including selection paths
        auto it = _selectionPaths.upper_bound(primPath);
        while (it != _selectionPaths.end() && it->HasPrefix(primPath)) {
            auto childPath = it->GetPrefixes()[primPath.GetPathElementCount()];
            if (std::find(childPaths.begin(), childPaths.end(), childPath) == childPaths.end()) {
                childPaths.emplace_back(childPath);
            }
            it++;
        }
        if (!childPaths.empty()) {
            return childPaths;
        }

        // To return the highlight sub-hierarchy paths
        auto selectionPath = primPath;
        while (_selectionPaths.find(selectionPath) == _selectionPaths.end() && selectionPath.HasPrefix(_highlightHierarchyPrefix)) {
            selectionPath = selectionPath.GetParentPath();
        }
        if (_selectionPaths.find(selectionPath) != _selectionPaths.end()) {
            return GetHighlightChildPrimPaths(selectionPath, primPath);
        }
    }
    return childPaths;
}

void BaseWhSi::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    _SendPrimsAdded(entries);
    HdSceneIndexObserver::AddedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    if (selectionsSchema.GetElement(selectionId).GetFullySelected() && !selectionsSchema.GetElement(selectionId).GetNestedInstanceIndices()) {
                        _fullySelectedPaths.emplace(entry.primPath);
                        break;
                    }
                }
            }
        }
    }
    ProcessAddedPrims(sender, filteredEntries);
}

void BaseWhSi::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
    HdSceneIndexObserver::RemovedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
            auto itFullySelectedPath = FindSelfOrFirstChild(entry.primPath, _fullySelectedPaths);
            while (itFullySelectedPath != _fullySelectedPaths.end() && itFullySelectedPath->HasPrefix(entry.primPath)) {
                itFullySelectedPath = _fullySelectedPaths.erase(itFullySelectedPath);
            }
        }
    }
    ProcessRemovedPrims(sender, filteredEntries);
}

void BaseWhSi::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    _SendPrimsDirtied(entries);
    HdSceneIndexObserver::DirtiedPrimEntries filteredEntries;
    std::vector<SdfPath> selectionChangePaths;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
            if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
                HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
                bool wasFullySelected = _fullySelectedPaths.find(entry.primPath) != _fullySelectedPaths.end();
                bool isFullySelected = false;
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements() && !isFullySelected; selectionId++) {
                        if (selectionsSchema.GetElement(selectionId).GetFullySelected() && !selectionsSchema.GetElement(selectionId).GetNestedInstanceIndices()) {
                            isFullySelected = true;
                        }
                    }
                }
                if (isFullySelected) {
                    _fullySelectedPaths.emplace(entry.primPath);
                } else {
                    _fullySelectedPaths.erase(entry.primPath);
                }
                if (wasFullySelected != isFullySelected) {
                    selectionChangePaths.push_back(entry.primPath);
                }
            }
        }
    }
    for (const auto& selectionChangePath : selectionChangePaths) {
        ProcessFullySelectedChange(selectionChangePath, _fullySelectedPaths.find(selectionChangePath) != _fullySelectedPaths.end());
    }
    ProcessDirtiedPrims(sender, filteredEntries);
}

void BaseWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    // no-op, can be overriden by derived classes
}

bool BaseWhSi::HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath)
{
    return FindSelfOrFirstParent(primPath, _fullySelectedPaths) != _fullySelectedPaths.end();
}

void BaseWhSi::AddExcludedPath(const PXR_NS::SdfPath& path)
{
    _excludedPaths.emplace(path);
}

bool BaseWhSi::IsExcludedPath(const PXR_NS::SdfPath& path) const
{
    return FindSelfOrFirstParent(path, _excludedPaths) != _excludedPaths.end();
}

SdfPath BaseWhSi::SelectionPathFromKey(const SelectionKey& selectionKey) const
{
    return selectionKey.first.ReplacePrefix(SdfPath::AbsoluteRootPath(), _highlightHierarchyPrefix).AppendElementString("Highlight_" + selectionKey.second);
}

SelectionKey BaseWhSi::SelectionKeyFromPath(const SdfPath& selectionPath) const
{
    auto selectedPrimPath = selectionPath.GetParentPath().ReplacePrefix(_highlightHierarchyPrefix, SdfPath::AbsoluteRootPath());
    auto highlightId = selectionPath.GetElementString().substr(std::string("Highlight_").size());
    return SelectionKey(selectedPrimPath, highlightId);
}

SdfPath BaseWhSi::RegisterSelection(const SelectionKey& selectionKey)
{
    _primPathsToSelections[selectionKey.first].emplace(selectionKey);
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.emplace(selectionPath);
    return selectionPath;
}

SdfPath BaseWhSi::UnregisterSelection(const SelectionKey& selectionKey)
{
    _primPathsToSelections[selectionKey.first].erase(selectionKey);
    if (_primPathsToSelections[selectionKey.first].empty()) {
        _primPathsToSelections.erase(selectionKey.first);
    }
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.erase(selectionPath);
    return selectionPath;
}

void
BaseWhSi::ForEachPrimInHierarchy(
    const PXR_NS::SdfPath& hierarchyRoot, 
    const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation
) const
{
    HdSceneIndexPrimView hierarchyView(GetInputSceneIndex(), hierarchyRoot);
    for (auto itPrim = hierarchyView.begin(); itPrim != hierarchyView.end(); ++itPrim) {
        const SdfPath& currPath = *itPrim;

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);

        // If the current prim is not part of the same hierarchy we are traversing, skip it and its descendents.
        VtArray<SdfPath> primRoots = _GetHierarchyRoots(currPrim);
        bool sharesHierarchy = std::find_if(primRoots.begin(), primRoots.end(), [hierarchyRoot](const auto& primRoot) -> bool {
            return hierarchyRoot.HasPrefix(primRoot);
        }) != primRoots.end();
        if (!sharesHierarchy) {
            itPrim.SkipDescendants();
            continue;
        }

        if (!operation(currPath, currPrim)) {
            itPrim.SkipDescendants();
            continue;
        }
    }
}

void
BaseWhSi::CollectInstancingPaths(const PXR_NS::SdfPath& primPath, InstancingPathsCollectionDirection direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths will be processed as part
    // of the children traversal later down this method.
    if (_IsPrototypeSubPrim(prim, primPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            CollectInstancingPaths(protoRootPath, direction, outInstancerPaths, outPrototypePaths);
        }
        return;
    }
    
    if (_IsPrototype(prim)) {
        if (outPrototypePaths.find(primPath) != outPrototypePaths.end()) {
            return;
        }
        outPrototypePaths.insert(primPath);
    } else {
        if (outInstancerPaths.find(primPath) != outInstancerPaths.end()) {
            return;
        }
        outInstancerPaths.insert(primPath);
    }

    // Traverse the children of this prim to find the affected child prims, and process their instancing-related
    // paths so we can create selection highlight mirrors for these prims as well.
    SdfPathVector affectedPrototypePaths;
    SdfPathVector affectedInstancedByPaths;
    auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (prim.primType == HdPrimTypeTokens->instancer || prim.primType == HdPrimTypeTokens->mesh) {
            if (direction & InstancingPathsCollectionDirection::Prototypes) {
                auto prototypePaths = _GetInstancingRelatedPaths(prim, InstancingPathsCollectionDirection::Prototypes);
                affectedPrototypePaths.insert(affectedPrototypePaths.end(), prototypePaths.begin(), prototypePaths.end());
            }
            if (direction & InstancingPathsCollectionDirection::InstancedBy) {
                auto instancedByPaths = _GetInstancingRelatedPaths(prim, InstancingPathsCollectionDirection::InstancedBy);
                affectedInstancedByPaths.insert(affectedInstancedByPaths.end(), instancedByPaths.begin(), instancedByPaths.end());
            }
            // We hit an instancing-related prim, don't process its children (nested instancing will be processed through the instancing-related paths).
            return false;
        }
        return true;
    };
    ForEachPrimInHierarchy(primPath, operation);

    for (const auto& affectedPrototypePath : affectedPrototypePaths) {
        CollectInstancingPaths(affectedPrototypePath, InstancingPathsCollectionDirection::Prototypes, outInstancerPaths, outPrototypePaths);
    }
    for (const auto& affectedInstancedByPath : affectedInstancedByPaths) {
        CollectInstancingPaths(affectedInstancedByPath, InstancingPathsCollectionDirection::InstancedBy, outInstancerPaths, outPrototypePaths);
    }
}

VtValue 
BaseWhSi::GetMaterialDisplacementValue(const PXR_NS::HdContainerDataSourceHandle& primDataSource) const
{
    if (!primDataSource) {
        return {};
    }

    // Step 1: Get the material path
    const SdfPath materialPath = GetMaterialPath(primDataSource);
    if (materialPath.IsEmpty()) {
        return {};
    }

    // Step 2: Retrieve the material
    HdSceneIndexPrim materialPrim = GetInputSceneIndex()->GetPrim(materialPath);
    if (!materialPrim.dataSource || (materialPrim.primType != HdPrimTypeTokens->material) ){
        return {};
    }

    HdMaterialSchema materialSchema = HdMaterialSchema::GetFromParent(materialPrim.dataSource);
    if (!materialSchema.IsDefined()) {
        return {};
    }

    // Step 3: Look for the displacement value
    HdDataSourceMaterialNetworkInterface materialNetworkInterface(
        materialPath, materialSchema.GetMaterialNetwork().GetContainer(), primDataSource);

    for (auto nodeName : materialNetworkInterface.GetNodeNames()) {
        VtValue paramValue = materialNetworkInterface.GetNodeParameterValue(nodeName, HdMaterialTerminalTokens->displacement);
        if (paramValue.IsHolding<float>()) {
            return paramValue;
        }
    }

    return {};
}

PXR_NS::HdContainerDataSourceHandle
BaseWhSi::MakeGeomSubsetHighlight(
    const PXR_NS::HdContainerDataSourceHandle& meshRootDataSource,
    const PXR_NS::HdContainerDataSourceHandle& geomSubsetRootDataSource) const
{
    HdContainerDataSourceHandle editedMeshRootDataSource = meshRootDataSource;

    VtValue displacementValue = GetMaterialDisplacementValue(geomSubsetRootDataSource);
    if (displacementValue.IsHolding<float>()) {
        // Manually apply the displacement on the mesh. We need to do this before trimming the mesh;
        // otherwise Storm will compute normals and displacement based on the trimmed mesh, which gives
        // incorrect results. Providing the normals primvar is not sufficient to fix this, so we must
        // do everything manually, including scaling.
        editedMeshRootDataSource = ComputeSmoothNormals(editedMeshRootDataSource);
        editedMeshRootDataSource = ForceScale(editedMeshRootDataSource);
        editedMeshRootDataSource = ForceDisplacement(editedMeshRootDataSource, displacementValue.UncheckedGet<float>());

        // Setup a dependency so that material updates dirty the points & normals primvars
        editedMeshRootDataSource = AddDependency(
            editedMeshRootDataSource, 
            TfToken("Fvp_WhSi_MaterialToPrimvars"), 
            GetMaterialPath(geomSubsetRootDataSource), 
            HdMaterialSchema::GetDefaultLocator(), 
            HdPrimvarsSchema::GetDefaultLocator());
    }

    // Trim the mesh to fit the geomSubset
    editedMeshRootDataSource = TrimMeshForGeomSubset(editedMeshRootDataSource, geomSubsetRootDataSource);

    return editedMeshRootDataSource;
}

}
