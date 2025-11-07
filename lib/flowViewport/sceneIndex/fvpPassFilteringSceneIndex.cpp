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

#include <iostream>
#ifdef VIEWPORT_TOOLBOX

#include "fvpPassFilteringSceneIndex.h"
#include "flowViewport/fvpUtils.h"

#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Check if the given SdfPath is prefixed by any of the paths in the vector.
bool isPrefixedBySdfPath(const SdfPath& pathToCheck, const SdfPathVector& paths)
{
    if (paths.empty()) {
        return false;
    }

    for (const auto& path : paths) {
        if (pathToCheck.HasPrefix(path)) {
            return true;
        }
    }

    return false;
}

} // namespace

namespace FVP_NS_DEF {
    
PassFilteringSceneIndexRefPtr PassFilteringSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const Fvp::FramePassConstDataPtr& framePassData)
{
    // If we return TfCreateRefPtr directly, Clang will destroy the rvalue before
    // returning, which means we will return a null pointer. To avoid this, store 
    // the pointer in an lvalue first and return that.
    auto refPtr = TfCreateRefPtr(new PassFilteringSceneIndex(inputSceneIndex, framePassData));
    return refPtr;
}

PassFilteringSceneIndex::PassFilteringSceneIndex(
    HdSceneIndexBaseRefPtr const& inputSceneIndex,
    const Fvp::FramePassConstDataPtr& framePassData)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _framePassData(framePassData)
{
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::PassFilteringSceneIndex", "potato");
    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        _UpdateFilteringStatus(primPath);
        _UpdateHighlightMaterialStatus(primPath);
    }
    Fvp::ProfileEnd(eventId);
}

bool PassFilteringSceneIndex::_IsAncestorFilteredOutInclusive(const SdfPath& primPath) const
{
    return FindSelfOrFirstParent(primPath, _filteredPrims) != _filteredPrims.end();
}

bool PassFilteringSceneIndex::_ShouldBeFilteredOut(const SdfPath& primPath) const
{
    int eventId = Fvp::ProfileBegin("_ShouldBeFilteredOut", primPath.GetText());
    // Clean version without logging
    if (!(_framePassData && _framePassData->_framePass) ) {
        Fvp::ProfileEnd(eventId);
        //reason = "_framePassData && _framePassData->_framePass";
        return true; // Safety check, exclude if no pass name
    }

    if (primPath.IsAbsoluteRootPath()) {
        Fvp::ProfileEnd(eventId);
        //reason = "IsAbsoluteRootPath";
        return false; // Always include root prims
    }

    // Fast checks that don't require GetPrim() - do these first for performance
    // Include paths: if specified and prim matches, definitely include it (workaround for special cases)
    if (!_framePassData->_includePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_includePaths)) {
            //reason = "_includePaths";
            Fvp::ProfileEnd(eventId);
        return false; // Prim is in the include paths, it will be rendered
    }

    // Exclude paths: if specified and prim matches, definitely exclude it (workaround for special cases)
    if (!_framePassData->_excludePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_excludePaths)) {
            //reason = "_excludePaths";
            Fvp::ProfileEnd(eventId);
        return true; // Prim is in the exclude paths, it will be skipped
    }

    // Now do the expensive GetPrim() call only when necessary
    const HdSceneIndexBaseRefPtr& inputSceneIndex = GetInputSceneIndex();
    if (!inputSceneIndex) {
        //reason = "no input scene index";
        Fvp::ProfileEnd(eventId);
        return false; // No input scene index, include by default
    }

    HdSceneIndexPrim prim = inputSceneIndex->GetPrim(primPath);

    if (_framePassData->_removeMaterials && prim.primType == HdPrimTypeTokens->material) {
        //reason = "materials removed";
        Fvp::ProfileEnd(eventId);
        return true;
    }

    //if (std::find(HdRprimTypeTokens->allTokens.begin(), HdRprimTypeTokens->allTokens.end(), prim.primType) == HdRprimTypeTokens->allTokens.end()) {
    //    if (prim.primType == HdPrimTypeTokens->material 
    //        && _framePassData->_removeMaterials) {
    //        return true;
    //    }
    //    return false; // Include all non-Rprims by default
    //}

    // Now apply the main filtering logic based on purpose render tags
    if (prim.dataSource) {
        const TfToken purposeRenderTag = GetPurposeRenderTag(prim.dataSource);
        if (!purposeRenderTag.IsEmpty() && _framePassData->_renderTags.find(purposeRenderTag) == _framePassData->_renderTags.end()) {
            //reason = "render tag was not enabled for this pass";
            Fvp::ProfileEnd(eventId);
            return true;
        }
        if (purposeRenderTag.IsEmpty() && !_framePassData->_supportPrimsWithNoPurposeRenderTag && std::find(HdRprimTypeTokens->allTokens.begin(), HdRprimTypeTokens->allTokens.end(), prim.primType) != HdRprimTypeTokens->allTokens.end()) {
            //reason = "!_framePassData->_supportPrimsWithNoPurposeRenderTag";
            Fvp::ProfileEnd(eventId);
            return true;
        }
    }
    //reason = "default";
    Fvp::ProfileEnd(eventId);
    return false; // will be rendered.
}

void PassFilteringSceneIndex::_UpdateFilteringStatus(const PXR_NS::SdfPath& primPath)
{
    int eventId = Fvp::ProfileBegin("_UpdateFilteringStatus", primPath.GetText());
    if (_IsAncestorFilteredOutInclusive(primPath) && _filteredPrims.find(primPath) == _filteredPrims.end()) {
        // This is a child of a filtered prim. Nothing to do.
        Fvp::ProfileEnd(eventId);
        return;
    }
    //bool wasFiltered = _IsAncestorFilteredOutInclusive(primPath);
    //std::string reason;
    if (_ShouldBeFilteredOut(primPath)) {
        //if (!wasFiltered) {
        //    std::cout << "Filtering " << primPath << " for : " << reason << std::endl;
        //}
        for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
            if ((*it).HasPrefix(primPath)) {
                it = _filteredPrims.erase(it);
            } else {
                ++it;
            }
        }
        _filteredPrims.insert(primPath);
        // Remove child paths so only root filtered prims are kept. When one is unfiltered, we recheck its children anywayss
    } else {
        //if (wasFiltered) {
        //    std::cout << "UN-filtering " << primPath << " for : " << reason << std::endl;
        //}
        _filteredPrims.erase(primPath);
    }
    Fvp::ProfileEnd(eventId);
}

void PassFilteringSceneIndex::_UpdateHighlightMaterialStatus(const PXR_NS::SdfPath& primPath)
{
    int eventId = Fvp::ProfileBegin("_UpdateHighlightMaterialStatus", primPath.GetText());
    if (!_framePassData->_removeMaterials) {
        // If we don't remove the materials, we can completely skip all the highlight material logic
        Fvp::ProfileEnd(eventId);
        return;
    }
    bool isMayaFacesHighlightPrim = primPath.GetName().find("PolyActiveFaces") != std::string::npos;
    bool isFvpHighlightPrim = !_framePassData->_highlightHierarchyPrefix.IsEmpty() && primPath.HasPrefix(_framePassData->_highlightHierarchyPrefix);
    if (!(isMayaFacesHighlightPrim || isFvpHighlightPrim)) {
        // Not a relevant prim
        Fvp::ProfileEnd(eventId);
        return;
    }

    auto prevMaterialPath = _highlightsToMaterialsPaths.find(primPath);
    auto removeMaterialEntry = [&]() {
        if (prevMaterialPath != _highlightsToMaterialsPaths.end()) {
            _highlightsToMaterialsPaths.erase(primPath);
            _highlightMaterialsUsage[prevMaterialPath->second]--;
            if (_highlightMaterialsUsage[prevMaterialPath->second] == 0) {
                _highlightMaterialsUsage.erase(prevMaterialPath->second);
                _SendPrimsRemoved({prevMaterialPath->second});
            }
        }
    };

    if (_IsAncestorFilteredOutInclusive(primPath)) {
        removeMaterialEntry();
        Fvp::ProfileEnd(eventId);
        return;
    }

    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!(prim.primType == HdPrimTypeTokens->mesh || prim.primType == HdPrimTypeTokens->geomSubset)) {
        removeMaterialEntry();
        Fvp::ProfileEnd(eventId);
        return;
    }
    auto materialPath = GetMaterialPath(prim.dataSource);
    if (materialPath.IsEmpty()) {
        removeMaterialEntry();
        Fvp::ProfileEnd(eventId);
        return;
    }
    auto materialPrim = GetInputSceneIndex()->GetPrim(materialPath);
    if (isFvpHighlightPrim && !MaterialHasDisplacement(materialPrim)) {
        removeMaterialEntry();
        Fvp::ProfileEnd(eventId);
        return;
    }

    if (materialPath != prevMaterialPath->second) {
        removeMaterialEntry();
        // Add the new material entry
        _highlightsToMaterialsPaths[primPath] = materialPath;
        _highlightMaterialsUsage[materialPath]++;
        if (_highlightMaterialsUsage[materialPath] == 1) {
            _SendPrimsAdded({{materialPath, materialPrim.primType}});
        }
    }
    Fvp::ProfileEnd(eventId);
}

void PassFilteringSceneIndex::_UpdateFilteringForTree(const PXR_NS::SdfPath& primPath)
{
    int eventId = Fvp::ProfileBegin("_UpdateFilteringForTree", primPath.GetText());
    if (_IsAncestorFilteredOutInclusive(primPath) && _filteredPrims.find(primPath) == _filteredPrims.end()) {
        // This is a child of a filtered prim. Nothing to do.
        Fvp::ProfileEnd(eventId);
        return;
    }
    bool wasFiltered = _IsAncestorFilteredOutInclusive(primPath);
    _UpdateFilteringStatus(primPath);
    if (wasFiltered == _IsAncestorFilteredOutInclusive(primPath)) {
        // Was filtered and still is : nothing to do
        // Was not filtered and still not : nothing to do
        Fvp::ProfileEnd(eventId);
        return;
    }

    // The filtering status changed.
    if (_IsAncestorFilteredOutInclusive(primPath)) {
        for (const auto& subTreePath : HdSceneIndexPrimView(GetInputSceneIndex(), primPath)) {
            _UpdateHighlightMaterialStatus(subTreePath);
        }
        _SendPrimsRemoved({primPath});
    } else {
        // Prim was just unfiltered. Need to check its children.
        // Update the whole sub-hierarchy
        // HdSceneIndexPrimView includes the prim itself
        HdSceneIndexObserver::AddedPrimEntries addedEntries;
        HdSceneIndexPrimView subTree(GetInputSceneIndex(), primPath);
        for (auto itTree = subTree.begin(); itTree != subTree.end(); ++itTree) {
            const SdfPath& currPath = *itTree;
            if (currPath != primPath) {
                _UpdateFilteringStatus(currPath);
            }
            if (_filteredPrims.find(currPath) == _filteredPrims.end()) {
                _UpdateHighlightMaterialStatus(currPath);
                addedEntries.emplace_back(currPath, GetInputSceneIndex()->GetPrim(currPath).primType);
            } else {
                itTree.SkipDescendants();
            }
        }
        _SendPrimsAdded(addedEntries);
    }
    Fvp::ProfileEnd(eventId);
}

HdSceneIndexPrim PassFilteringSceneIndex::GetPrim(const SdfPath& primPath) const
{
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::GetPrim", primPath.GetText());
    // If an ancestor if filtered out and this is not a highlight material, return nothing
    if (_IsAncestorFilteredOutInclusive(primPath) && _highlightMaterialsUsage.find(primPath) == _highlightMaterialsUsage.end()) {
        Fvp::ProfileEnd(eventId);
        return {};
    }
    Fvp::ProfileEnd(eventId);
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PassFilteringSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::GetChildPrimPaths", primPath.GetText());
    // If this prim is filtered out and there are no highlight materials under it, return no children
    if (_IsAncestorFilteredOutInclusive(primPath) && FindSelfOrFirstChild(primPath, _highlightMaterialsUsage) == _highlightMaterialsUsage.cend()) {
        Fvp::ProfileEnd(eventId);
        return {};
    }

    // Get children from input scene index
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);

    // Filter out children that should be filtered out
    SdfPathVector filteredChildPaths;
    for (const SdfPath& childPath : childPaths) {
        if (!_IsAncestorFilteredOutInclusive(childPath) || FindSelfOrFirstChild(primPath, _highlightMaterialsUsage) != _highlightMaterialsUsage.cend()) {
            filteredChildPaths.push_back(childPath);
        }
    }
    Fvp::ProfileEnd(eventId);
    return filteredChildPaths;
}

void PassFilteringSceneIndex::DirtyPrimsFromPurposeRenderTag(const TfToken purposeRenderTag)
{
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::DirtyPrimsFromPurposeRenderTag", purposeRenderTag.GetText());
    auto& inputSceneIndex = GetInputSceneIndex();
    if (inputSceneIndex) {
        for (const SdfPath& primPath : HdSceneIndexPrimView(inputSceneIndex)) {
            _UpdateFilteringForTree(primPath);
        }
    }
    Fvp::ProfileEnd(eventId);
}

void PassFilteringSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::_PrimsAdded", "PassFilteringSceneIndex::_PrimsAdded");

    for (const auto& addedEntry : entries) {
        _UpdateFilteringForTree(addedEntry.primPath);
        if (!_IsAncestorFilteredOutInclusive(addedEntry.primPath) || _highlightMaterialsUsage.find(addedEntry.primPath) != _highlightMaterialsUsage.end()) {
            addedEntries.emplace_back(addedEntry);
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
    Fvp::ProfileEnd(eventId);
}

void PassFilteringSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::_PrimsRemoved", "PassFilteringSceneIndex::_PrimsRemoved");

    for (const auto& removedEntry : entries) {
        if (!_IsAncestorFilteredOutInclusive(removedEntry.primPath)) {
            for (const auto& subTreePath : HdSceneIndexPrimView(GetInputSceneIndex(), removedEntry.primPath)) {
                _UpdateHighlightMaterialStatus(subTreePath);
            }
            removedEntries.emplace_back(removedEntry);
        } else {
            _filteredPrims.erase(removedEntry.primPath);
            for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
                if ((*it).HasPrefix(removedEntry.primPath)) {
                    it = _filteredPrims.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
    Fvp::ProfileEnd(eventId);
}

void PassFilteringSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    int eventId = Fvp::ProfileBegin("PassFilteringSceneIndex::_PrimsDirtied", "PassFilteringSceneIndex::_PrimsDirtied");
    for (const auto& entry : entries) {
        _UpdateFilteringForTree(entry.primPath);
        if (!_IsAncestorFilteredOutInclusive(entry.primPath) || _highlightMaterialsUsage.find(entry.primPath) != _highlightMaterialsUsage.end()) {
            dirtiedEntries.emplace_back(entry);
        }
    }
    _SendPrimsDirtied(dirtiedEntries);
    Fvp::ProfileEnd(eventId);
}

} // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX
