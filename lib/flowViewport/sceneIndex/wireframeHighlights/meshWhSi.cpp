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
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(primPath);
                }
            }
        }
        if (prim.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(primPath);
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);

    for (const auto& meshPath : _meshPaths) {
        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(meshPath);
        if (itSelectedParentPath != _fullySelectedPaths.begin() && meshPath.HasPrefix(*std::prev(itSelectedParentPath))) {
            _CreateSelectionHighlight(meshPath);
        }
    }
}

void MeshWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        bool isMesh = false;
        bool isSelected = false;
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (entry.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(entry.primPath);
                isMesh = true;
            }
        }
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(entry.primPath);
                    isSelected = true;
                }
            }
        }
        if (isMesh) {
            auto itSelectedParent = _fullySelectedPaths.upper_bound(entry.primPath);
            if (itSelectedParent != _fullySelectedPaths.begin() && entry.primPath.HasPrefix(*std::prev(itSelectedParent))) {
                if (_highlightedMeshPaths.find(entry.primPath) == _highlightedMeshPaths.end()) {
                    _CreateSelectionHighlight(entry.primPath);
                }
            }
        }
        if (isSelected) {
            auto itMeshChild = _meshPaths.lower_bound(entry.primPath);
            while (itMeshChild != _meshPaths.end() && itMeshChild->HasPrefix(entry.primPath)) {
                if (_highlightedMeshPaths.find(*itMeshChild) == _highlightedMeshPaths.end()) {
                    _CreateSelectionHighlight(*itMeshChild);
                }
                itMeshChild++;
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
            if (_highlightedMeshPaths.find(*itMesh) != _highlightedMeshPaths.end()) {
                _DeleteSelectionHighlight(*itMesh);
            }
            itMesh = _meshPaths.erase(itMesh);
        }

        auto itSelectedPath = _fullySelectedPaths.lower_bound(entry.primPath);
        while (itSelectedPath != _fullySelectedPaths.end() && itSelectedPath->HasPrefix(entry.primPath)) {
            itSelectedPath = _fullySelectedPaths.erase(itSelectedPath);
            // Child mesh highlights will have already been deleted above, since deleting a selected parent
            // implies deleting child meshes.
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
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            bool isFullySelected = false;
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                        isFullySelected = true;
                    }
                }
            }
            // Newly selected path : create selection highlights for all meshes not yet highlighted under it.
            if (isFullySelected && _fullySelectedPaths.find(entry.primPath) == _fullySelectedPaths.end()) {
                _fullySelectedPaths.emplace(entry.primPath);
                auto itMesh = _meshPaths.lower_bound(entry.primPath);
                while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
                    if (_highlightedMeshPaths.find(*itMesh) == _highlightedMeshPaths.end()) {
                        _CreateSelectionHighlight(*itMesh);
                    }
                    itMesh++;
                }
            }
            // Newly unselected path : delete selection highlights for all meshes under it if no other prim
            // that is a parent of that mesh is selected.
            else if (!isFullySelected && _fullySelectedPaths.find(entry.primPath) != _fullySelectedPaths.end()) {
                _fullySelectedPaths.erase(entry.primPath);
                auto itMesh = _meshPaths.lower_bound(entry.primPath);
                while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
                    if (_highlightedMeshPaths.find(*itMesh) != _highlightedMeshPaths.end()) {
                        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(*itMesh);
                        if (itSelectedParentPath == _fullySelectedPaths.begin() || !itMesh->HasPrefix(*std::prev(itSelectedParentPath))) {
                            // No selected parent.
                            _DeleteSelectionHighlight(*itMesh);
                        }
                    }
                    itMesh++;
                }
            }
        }

        // Propagate changes to the mesh and its children (needed for wireframe color updates)
        auto itHighlightedMesh = _highlightedMeshPaths.upper_bound(entry.primPath);
        if (itHighlightedMesh != _highlightedMeshPaths.begin()) {
            --itHighlightedMesh;
            if (entry.primPath.HasPrefix(*itHighlightedMesh)) {
                auto selectionPath = SelectionPathFromKey(SelectionKey(*itHighlightedMesh, ""));
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void MeshWhSi::_CreateSelectionHighlight(const SdfPath& meshPath)
{
    if (_highlightedMeshPaths.find(meshPath) != _highlightedMeshPaths.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { meshPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _highlightedMeshPaths.emplace(meshPath);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(meshPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), GetInputSceneIndex()->GetPrim(meshPath).primType);
    _SendPrimsAdded(addedPrims);
}

void MeshWhSi::_DeleteSelectionHighlight(const SdfPath& meshPath)
{
    if (_highlightedMeshPaths.find(meshPath) == _highlightedMeshPaths.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { meshPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    _highlightedMeshPaths.erase(meshPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
