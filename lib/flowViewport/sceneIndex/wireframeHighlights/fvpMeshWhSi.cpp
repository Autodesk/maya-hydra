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

#include "fvpMeshWhSi.h"

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

// Copied from fvpBaseWhSi.cpp
namespace {
    TF_DEFINE_PRIVATE_TOKENS(
        _tokens,
        // Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
        (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
    );

    const HdDataSourceLocator primvarsOverrideWireframeColorLocator(
        HdPrimvarsSchema::GetDefaultLocator().Append(_tokens->overrideWireframeColor));
}

MeshWhSiRefPtr MeshWhSi::New(
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
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
    prim.dataSource = RepathInstancingDataSources(prim.dataSource, SdfPath::AbsoluteRootPath(), selectionPath);
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
        // Delete all selection highlights for meshes rooted under the removed prim
        auto itMesh = FindSelfOrFirstChild(entry.primPath, _meshPaths);
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
        // Propagate changes to the mesh and its children
        auto itMeshHighlights = FindSelfOrFirstParent(entry.primPath, _primPathsToSelections);
        if (itMeshHighlights != _primPathsToSelections.end()) {
            for (const auto& selectionKey : itMeshHighlights->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void MeshWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    if (isFullySelected) {
        for (auto itMesh = FindSelfOrFirstChild(primPath, _meshPaths); itMesh != _meshPaths.end() && itMesh->HasPrefix(primPath); itMesh++) {
            _CreateSelectionHighlight(*itMesh);
        }
    }
    else {
        for (auto itMesh = FindSelfOrFirstChild(primPath, _meshPaths); itMesh != _meshPaths.end() && itMesh->HasPrefix(primPath); itMesh++) {
            if (!HasFullySelectedAncestorInclusive(*itMesh)) {
                _DeleteSelectionHighlight(*itMesh);
            }
        }
    }

    // When the selection state of a mesh prim changes, we need to send a signal to update primvars/overrideWireframeColor.
    // overrideWireframeColor only exists when wireframe is enabled.
    // We assume that dirtying overrideWireframeColor even if it doesn't exist is less expensive than to check if it exists.
    // The handling of the lead selection vs active selection highlight is done elsewhere in MhDirtyLeadObjectSceneIndex.
    _SendPrimsDirtied({ { primPath, { primvarsOverrideWireframeColorLocator } } });
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
    auto operation = [&addedPrims, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), prim.primType);
        return true;
    };
    ForEachPrimInHierarchy(meshPath, operation);
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
