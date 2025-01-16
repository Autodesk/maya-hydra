#include "meshWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
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

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr MeshWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new MeshWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim MeshWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = RepathingContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
    return prim;
};

SdfPathVector MeshWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        bool isRelevantPath = originalChildPath.HasPrefix(selectionKey.first) || selectionKey.first.HasPrefix(originalChildPath);
        if (isRelevantPath) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
        }
    }
    return childPaths;
}

MeshWhSi::MeshWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (prim.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(primPath);
                if (HasFullySelectedAncestorInclusive(primPath)) {
                    _CreateSelectionHighlight(primPath);
                }
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void MeshWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (entry.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(entry.primPath);
                if (HasFullySelectedAncestorInclusive(entry.primPath)) {
                    _CreateSelectionHighlight(entry.primPath);
                }
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void MeshWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        auto itMesh = _meshPaths.lower_bound(entry.primPath);
        while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
            _DeleteSelectionHighlight(*itMesh);
            itMesh = _meshPaths.erase(itMesh);
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void MeshWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // Propagate changes to the mesh and its children (needed for wireframe color updates)
        auto itMeshHighlights = _primPathsToSelections.upper_bound(entry.primPath);
        if (itMeshHighlights != _primPathsToSelections.begin()) {
            --itMeshHighlights;
            if (entry.primPath.HasPrefix(itMeshHighlights->first)) {
                for (const auto& selectionKey : itMeshHighlights->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void MeshWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    if (isFullySelected) {
        for (auto itMesh = _meshPaths.lower_bound(primPath); itMesh != _meshPaths.end() && itMesh->HasPrefix(primPath); itMesh++) {
            _CreateSelectionHighlight(*itMesh);
        }
    }
    else {
        for (auto itMesh = _meshPaths.lower_bound(primPath); itMesh != _meshPaths.end() && itMesh->HasPrefix(primPath); itMesh++) {
            if (!HasFullySelectedAncestorInclusive(*itMesh)) {
                _DeleteSelectionHighlight(*itMesh);
            }
        }
    }
}

void MeshWhSi::_CreateSelectionHighlight(const SdfPath& meshPath)
{
    if (_primPathsToSelections.find(meshPath) != _primPathsToSelections.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { meshPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(meshPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), GetInputSceneIndex()->GetPrim(meshPath).primType);
    _SendPrimsAdded(addedPrims);
}

void MeshWhSi::_DeleteSelectionHighlight(const SdfPath& meshPath)
{
    if (_primPathsToSelections.find(meshPath) == _primPathsToSelections.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { meshPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
