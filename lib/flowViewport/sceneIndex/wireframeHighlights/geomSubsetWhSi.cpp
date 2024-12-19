#include "geomSubsetWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/pathTable.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// We consider prototypes that have child prims to be different hierarchies,
// separate from each other and from the "root" hierarchy.
VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
}

// Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
//     Fvp::PrimSelection primSelection;
//     primSelection.primPath = primPath;

//     HdInstanceIndicesVectorSchema nestedInstanceIndicesSchema = selectionSchema.GetNestedInstanceIndices();
//     //std::cout << "nestedInstanceIndicesSchema.GetNumElements() = " << nestedInstanceIndicesSchema.GetNumElements() << std::endl;
//     for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
//         //std::cout << "iNestedInstanceIndices : " << iNestedInstanceIndices << std::endl;
//         HdInstanceIndicesSchema instanceIndicesSchema = nestedInstanceIndicesSchema.GetElement(iNestedInstanceIndices);
//         auto instanceIndices = instanceIndicesSchema.GetInstanceIndices()->GetTypedValue(0);
//         primSelection.nestedInstanceIndices.push_back(
//             {
//                 instanceIndicesSchema.GetInstancer()->GetTypedValue(0),
//                 instanceIndicesSchema.GetPrototypeIndex()->GetTypedValue(0),
//                 std::vector<int>(instanceIndices.begin(), instanceIndices.end())
//             }
//         );
//     }

//     return primSelection;
// }

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

// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

class _RerootingSceneIndexContainerDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexContainerDataSource)

    _RerootingSceneIndexContainerDataSource(
        const SdfPath &srcPrefix,
        const SdfPath &dstPrefix,
        HdContainerDataSourceHandle const &inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
    {
    }

    TfTokenVector GetNames() override
    {
        if (!_inputDataSource) {
            return {};
        }

        return _inputDataSource->GetNames();
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
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

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdContainerDataSourceHandle const _inputDataSource;
};

bool _IsSelected(const HdSceneIndexPrim& prim)
{
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    return selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;
}

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
    HdDataSourceLocator pointsValueLocator = HdDataSourceLocator(HdPrimvarsSchemaTokens->primvars, HdPrimvarsSchemaTokens->points, HdPrimvarSchemaTokens->primvarValue);
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

    // We reduce the points primvar so that it has only the exact number of points required by the trimmed topology;
    // this avoids a warning from USD.
    VtArray<GfVec3f> points = pointsValueDataSource->GetTypedValue(0);
    points.resize(maxVertexIndex + 1);
    dataSourceEditor.Set(pointsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(points));

    return dataSourceEditor.Finish();
}

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr GeomSubsetWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new GeomSubsetWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim GeomSubsetWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    auto originalMeshPath = selectionKey.first.GetParentPath();

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, originalMeshPath.GetParentPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = _RerootingSceneIndexContainerDataSource::New(originalMeshPath.GetParentPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
        if (originalPath == selectionKey.first.GetParentPath()) {
            prim.dataSource = _TrimMeshForGeomSubset(prim.dataSource, GetInputSceneIndex()->GetPrim(selectionKey.first).dataSource);
        }
    }
    return prim;
};

SdfPathVector GeomSubsetWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    auto originalMeshPath = selectionKey.first.GetParentPath();
    if (fullPrimPath == selectionPath) {
        return {originalMeshPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath)};
    }
    return {};
    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, originalMeshPath.GetParentPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        auto itPath = _highlightedGeomSubsetPaths.upper_bound(originalChildPath);
        bool isRelevantPath = (itPath != _highlightedGeomSubsetPaths.end() && itPath->HasPrefix(originalChildPath)) || (itPath != _highlightedGeomSubsetPaths.begin() && originalChildPath.HasPrefix(*std::prev(itPath)));
        if (isRelevantPath) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath));
        }
    }
    return childPaths;
}

GeomSubsetWhSi::GeomSubsetWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (prim.primType == HdPrimTypeTokens->geomSubset) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined() && _IsSelected(prim) && !_IsSelected(GetInputSceneIndex()->GetPrim(primPath.GetParentPath()))) {
                _CreateSelectionHighlight(primPath);
            }
        }
        return true;
    };
    _ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void GeomSubsetWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (prim.primType == HdPrimTypeTokens->geomSubset) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined() && _IsSelected(prim) && !_IsSelected(GetInputSceneIndex()->GetPrim(entry.primPath.GetParentPath()))) {
                _CreateSelectionHighlight(entry.primPath);
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void GeomSubsetWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        std::set<SdfPath> highlightedGeomSubsetPathsToDelete;
        auto itGeomSubset = _highlightedGeomSubsetPaths.lower_bound(entry.primPath);
        while (itGeomSubset != _highlightedGeomSubsetPaths.end() && itGeomSubset->HasPrefix(entry.primPath)) {
            if (_highlightedGeomSubsetPaths.find(*itGeomSubset) != _highlightedGeomSubsetPaths.end()) {
                highlightedGeomSubsetPathsToDelete.emplace(*itGeomSubset);
            }
            itGeomSubset++;
        }
        for (const auto& highlightedGeomSubsetPathToDelete : highlightedGeomSubsetPathsToDelete) {
            _DeleteSelectionHighlight(highlightedGeomSubsetPathToDelete);
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void GeomSubsetWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                bool isSelected = _IsSelected(prim);
                bool isMeshSelected = _IsSelected(GetInputSceneIndex()->GetPrim(entry.primPath.GetParentPath()));
                if (isSelected && _highlightedGeomSubsetPaths.find(entry.primPath) == _highlightedGeomSubsetPaths.end() && !isMeshSelected) {
                    std::cout << "Create on GeomSubset newly selected and parent mesh not" << std::endl;
                    _CreateSelectionHighlight(entry.primPath);
                }
                else if (!isSelected && _highlightedGeomSubsetPaths.find(entry.primPath) != _highlightedGeomSubsetPaths.end()) {
                    std::cout << "Delete on GeomSubset newly unselected" << std::endl;
                    _DeleteSelectionHighlight(entry.primPath);
                }
            }

            if (prim.primType == HdPrimTypeTokens->mesh) {
                bool isMeshSelected = _IsSelected(prim);
                auto operation = [this, isMeshSelected](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                    if (prim.primType == HdPrimTypeTokens->geomSubset) {
                        if (_IsSelected(prim)) {
                            if (!isMeshSelected && _highlightedGeomSubsetPaths.find(primPath) == _highlightedGeomSubsetPaths.end()) {
                                std::cout << "Create on Mesh newly unselected but GeomSubset selected" << std::endl;
                                _CreateSelectionHighlight(primPath);
                            }
                            else if (isMeshSelected && _highlightedGeomSubsetPaths.find(primPath) != _highlightedGeomSubsetPaths.end()) {
                                std::cout << "Delete on Mesh newly selected but GeomSubset also selected" << std::endl;
                                _DeleteSelectionHighlight(primPath);
                            }
                        }
                    }
                    return true;
                };
                _ForEachPrimInHierarchy(entry.primPath, operation);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void GeomSubsetWhSi::_CreateSelectionHighlight(const SdfPath& primPath)
{
    if (_highlightedGeomSubsetPaths.find(primPath) != _highlightedGeomSubsetPaths.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { primPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _highlightedGeomSubsetPaths.emplace(primPath);

    // Send notifications
    auto originalMeshPath = primPath.GetParentPath();
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(originalMeshPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), GetInputSceneIndex()->GetPrim(originalMeshPath).primType);
    //addedPrims.emplace_back(primPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), GetInputSceneIndex()->GetPrim(primPath).primType);
    _SendPrimsAdded(addedPrims);
}

void GeomSubsetWhSi::_DeleteSelectionHighlight(const SdfPath& primPath)
{
    if (_highlightedGeomSubsetPaths.find(primPath) == _highlightedGeomSubsetPaths.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { primPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    _highlightedGeomSubsetPaths.erase(primPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

void
GeomSubsetWhSi::_ForEachPrimInHierarchy(
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

}
