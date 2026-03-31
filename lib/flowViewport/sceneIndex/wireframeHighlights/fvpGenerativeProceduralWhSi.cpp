// Copyright 2026 Autodesk
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

#include "fvpGenerativeProceduralWhSi.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/repr.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>

#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _IsGenerativeProcedural(const HdSceneIndexPrim& prim) { 
    return prim.primType == TfToken("hydraGenerativeProcedural")
            || prim.primType == TfToken("resolvedHydraGenerativeProcedural");
}

}

namespace FVP_NS_DEF {

GenerativeProceduralWhSi::GenerativeProceduralWhSi(
    const HdSceneIndexBaseRefPtr&                   inputSceneIndex,
    const SdfPath&                                  highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
    : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (_IsGenerativeProcedural(prim)) {
            _generativeProceduralPaths.emplace(primPath);
            if (HasFullySelectedAncestorInclusive(primPath)) {
                _CreateSelectionHighlight(primPath);
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

GenerativeProceduralWhSiRefPtr
GenerativeProceduralWhSi::New(const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
    const PXR_NS::SdfPath&                          highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(
        new GenerativeProceduralWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim GenerativeProceduralWhSi::GetHighlightPrim(
    const SdfPath& selectionPath,
    const SdfPath& fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
   
    HdSceneIndexPrim prim = GetPrim(originalPath);
    // For now we are assuming that all GP are generating meshes. 
    // If a GP is generating prims from a different type, then 
    // we will need to add additional support for those prim types. 
    if (prim.primType == HdPrimTypeTokens->mesh) {
        // GP-cooked meshes may lack a displayStyle schema if the
        // procedural plugin doesn't add one. Without it, SetWireframeRepr
        // cannot apply a wire repr. Inject a default reprSelector so that the
        // wireframe highlight becomes visible.
        if (!HdLegacyDisplayStyleSchema::GetFromParent(prim.dataSource)) {
            static const HdRetainedContainerDataSourceHandle defaultDisplayStyle
                = HdRetainedContainerDataSource::New(
                    HdLegacyDisplayStyleSchemaTokens->displayStyle,
                    HdRetainedContainerDataSource::New(
                        HdLegacyDisplayStyleSchemaTokens->reprSelector,
                        HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                            { HdReprTokens->refined, TfToken(), TfToken() })));
            prim.dataSource
                = HdOverlayContainerDataSource::New({ defaultDisplayStyle, prim.dataSource });
        }
        GfVec4f wireframeColor = _wireframeColorInterface->getWireframeColor(selectionKey.first);
        prim.dataSource = SetWireframeRepr(prim.dataSource, wireframeColor);
    }

    prim.dataSource
        = RepathInstancingDataSources(prim.dataSource, SdfPath::AbsoluteRootPath(), selectionPath);
    return prim;
}

SdfPathVector GenerativeProceduralWhSi::GetHighlightChildPrimPaths(
    const SdfPath& selectionPath,
    const SdfPath& fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());    
    auto originalChildPaths = GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        bool isRelevantPath = originalChildPath.HasPrefix(selectionKey.first)
            || selectionKey.first.HasPrefix(originalChildPath);
        if (isRelevantPath) {
            childPaths.emplace_back(
                originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
        }
    }
    return childPaths;
}

void GenerativeProceduralWhSi::ProcessAddedPrims(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (_IsGenerativeProcedural(prim)) {
            _generativeProceduralPaths.emplace(entry.primPath);
            if (HasFullySelectedAncestorInclusive(entry.primPath)) {
                _CreateSelectionHighlight(entry.primPath);
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void GenerativeProceduralWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        auto itProc = FindSelfOrFirstChild(entry.primPath, _generativeProceduralPaths);
        while (itProc != _generativeProceduralPaths.end() && itProc->HasPrefix(entry.primPath)) {
            _DeleteSelectionHighlight(*itProc);
            itProc = _generativeProceduralPaths.erase(itProc);
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void GenerativeProceduralWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        SdfPath primPath = entry.primPath;
        // Propagate changes
        auto itProcHighlights = FindSelfOrFirstParent(primPath, _primPathsToSelections);
        if (itProcHighlights != _primPathsToSelections.end()) {
            for (const auto& selectionKey : itProcHighlights->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(
                    entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath),
                    entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void GenerativeProceduralWhSi::ProcessFullySelectedChange(
    const SdfPath& primPath,
    bool           isFullySelected)
{
    if (isFullySelected) {
        for (auto itProc = FindSelfOrFirstChild(primPath, _generativeProceduralPaths);
             itProc != _generativeProceduralPaths.end() && itProc->HasPrefix(primPath);
             itProc++) {
            _CreateSelectionHighlight(*itProc);
        }
    } else {
        for (auto itProc = FindSelfOrFirstChild(primPath, _generativeProceduralPaths);
             itProc != _generativeProceduralPaths.end() && itProc->HasPrefix(primPath);
             itProc++) {
             if (!HasFullySelectedAncestorInclusive(*itProc)) {
                _DeleteSelectionHighlight(*itProc);
             }
        }
    }
}

void GenerativeProceduralWhSi::_CreateSelectionHighlight(const SdfPath& generativeProceduralPath) {
    const SelectionKey key { generativeProceduralPath, "" };
    const SdfPath selectionPath = RegisterSelection(key);
    HdSceneIndexObserver::AddedPrimEntries entries;
    std::function<void(const SdfPath&)> walkChildren;
    walkChildren = [&](const SdfPath& path) {
        HdSceneIndexPrim prim = GetPrim(path);
        entries.emplace_back(
            path.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), prim.primType);
        for (const auto& childPath : GetChildPrimPaths(path)) {
            walkChildren(childPath);
        }
    };
    walkChildren(generativeProceduralPath);

    _SendPrimsAdded(entries);
}

void GenerativeProceduralWhSi::_DeleteSelectionHighlight(const SdfPath& generativeProceduralPath) {
    const SelectionKey key { generativeProceduralPath, "" };
    const SdfPath selectionPath = UnregisterSelection(key);
    HdSceneIndexObserver::RemovedPrimEntries entries;

    std::function<void(const SdfPath&)> walkChildren;
    walkChildren = [&](const SdfPath& path) {
        HdSceneIndexPrim prim = GetPrim(path);
        entries.emplace_back(
            path.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
        for (const auto& childPath : GetChildPrimPaths(path)) {
            walkChildren(childPath);
        }
    };
    walkChildren(generativeProceduralPath);

    _SendPrimsRemoved(entries);
}

}
