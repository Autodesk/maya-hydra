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
    prim.dataSource = RepathingContainerDataSource::New(originalMeshPath.GetParentPath(), selectionPath, prim.dataSource);
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
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void GeomSubsetWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
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

            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (instancedBy.IsDefined()) {
                continue;
            }

            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                bool isSelected = _IsSelected(prim);
                bool isMeshSelected = _IsSelected(GetInputSceneIndex()->GetPrim(entry.primPath.GetParentPath()));
                if (isSelected && _highlightedGeomSubsetPaths.find(entry.primPath) == _highlightedGeomSubsetPaths.end() && !isMeshSelected) {
                    _CreateSelectionHighlight(entry.primPath);
                }
                else if (!isSelected && _highlightedGeomSubsetPaths.find(entry.primPath) != _highlightedGeomSubsetPaths.end()) {
                    _DeleteSelectionHighlight(entry.primPath);
                }
            }

            if (prim.primType == HdPrimTypeTokens->mesh) {
                bool isMeshSelected = _IsSelected(prim);
                auto operation = [this, isMeshSelected](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                    if (prim.primType == HdPrimTypeTokens->geomSubset) {
                        if (_IsSelected(prim)) {
                            if (!isMeshSelected && _highlightedGeomSubsetPaths.find(primPath) == _highlightedGeomSubsetPaths.end()) {
                                _CreateSelectionHighlight(primPath);
                            }
                            else if (isMeshSelected && _highlightedGeomSubsetPaths.find(primPath) != _highlightedGeomSubsetPaths.end()) {
                                _DeleteSelectionHighlight(primPath);
                            }
                        }
                    }
                    return true;
                };
                ForEachPrimInHierarchy(entry.primPath, operation);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void GeomSubsetWhSi::_CreateSelectionHighlight(const SdfPath& geomSubsetPath)
{
    if (_highlightedGeomSubsetPaths.find(geomSubsetPath) != _highlightedGeomSubsetPaths.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { geomSubsetPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _highlightedGeomSubsetPaths.emplace(geomSubsetPath);

    // Send notifications
    auto originalMeshPath = geomSubsetPath.GetParentPath();
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(originalMeshPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), GetInputSceneIndex()->GetPrim(originalMeshPath).primType);
    //addedPrims.emplace_back(primPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), GetInputSceneIndex()->GetPrim(primPath).primType);
    _SendPrimsAdded(addedPrims);
}

void GeomSubsetWhSi::_DeleteSelectionHighlight(const SdfPath& geomSubsetPath)
{
    if (_highlightedGeomSubsetPaths.find(geomSubsetPath) == _highlightedGeomSubsetPaths.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { geomSubsetPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    _highlightedGeomSubsetPaths.erase(geomSubsetPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
