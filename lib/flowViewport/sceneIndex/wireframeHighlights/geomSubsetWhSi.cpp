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

#include "geomSubsetWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
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

#if PXR_VERSION >= 2403

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _IsSelected(const HdSceneIndexPrim& prim)
{
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    return selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;
}

}

namespace FVP_NS_DEF {

GeomSubsetWhSiRefPtr GeomSubsetWhSi::New(
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
            prim.dataSource = TrimMeshForGeomSubset(prim.dataSource, GetInputSceneIndex()->GetPrim(selectionKey.first).dataSource);
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
            if (!instancedBy.IsDefined() && _IsSelected(prim)) {
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
            if (!instancedBy.IsDefined() && _IsSelected(prim)) {
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
        auto itGeomSubsetSelections = _primPathsToSelections.lower_bound(entry.primPath);
        while (itGeomSubsetSelections != _primPathsToSelections.end() && itGeomSubsetSelections->first.HasPrefix(entry.primPath)) {
            if (_primPathsToSelections.find(itGeomSubsetSelections->first) != _primPathsToSelections.end()) {
                highlightedGeomSubsetPathsToDelete.emplace(itGeomSubsetSelections->first);
            }
            itGeomSubsetSelections++;
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
        if (_primPathsToSelections.find(entry.primPath) != _primPathsToSelections.end()) {
            // Dirty prototype prim (to update highlight color)
            auto selectionPath = SelectionPathFromKey(SelectionKey(entry.primPath, ""));
            auto originalMeshPath = entry.primPath.GetParentPath();
            highlightEntries.emplace_back(originalMeshPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), entry.dirtyLocators);
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void GeomSubsetWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (prim.primType == HdPrimTypeTokens->geomSubset) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
        if (!instancedBy.IsDefined()) {
            if (isFullySelected) {
                _CreateSelectionHighlight(primPath);
            } else {
                _DeleteSelectionHighlight(primPath);
            }
        }
    }
}

void GeomSubsetWhSi::_CreateSelectionHighlight(const SdfPath& geomSubsetPath)
{
    if (_primPathsToSelections.find(geomSubsetPath) != _primPathsToSelections.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { geomSubsetPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    // Send notifications
    auto originalMeshPath = geomSubsetPath.GetParentPath();
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(originalMeshPath.ReplacePrefix(originalMeshPath.GetParentPath(), selectionPath), GetInputSceneIndex()->GetPrim(originalMeshPath).primType);
    _SendPrimsAdded(addedPrims);
}

void GeomSubsetWhSi::_DeleteSelectionHighlight(const SdfPath& geomSubsetPath)
{
    if (_primPathsToSelections.find(geomSubsetPath) == _primPathsToSelections.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { geomSubsetPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}

#endif // #if PXR_VERSION >= 2403
