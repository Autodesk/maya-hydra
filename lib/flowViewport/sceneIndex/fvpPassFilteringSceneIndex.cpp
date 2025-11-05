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
    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        _UpdateFilteringStatus(primPath);
        _UpdateHighlightMaterialStatus(primPath);
    }
}

bool PassFilteringSceneIndex::_IsFilteredOut(const PXR_NS::SdfPath& primPath) const
{
    return _filteredPrims.find(primPath) != _filteredPrims.end();
}

bool PassFilteringSceneIndex::_ShouldBeFilteredOut(const SdfPath& primPath) const
{
    // Clean version without logging
    if (!(_framePassData && _framePassData->_framePass) ) {
        return true; // Safety check, exclude if no pass name
    }

    if (primPath.IsAbsoluteRootPath()) {
        return false; // Always include root prims
    }

    // Fast checks that don't require GetPrim() - do these first for performance
    // Include paths: if specified and prim matches, definitely include it (workaround for special cases)
    if (!_framePassData->_includePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_includePaths)) {
        return false; // Prim is in the include paths, it will be rendered
    }

    // Exclude paths: if specified and prim matches, definitely exclude it (workaround for special cases)
    if (!_framePassData->_excludePaths.empty()
        && isPrefixedBySdfPath(primPath, _framePassData->_excludePaths)) {
        return true; // Prim is in the exclude paths, it will be skipped
    }

    // Now do the expensive GetPrim() call only when necessary
    const HdSceneIndexBaseRefPtr& inputSceneIndex = GetInputSceneIndex();
    if (!inputSceneIndex) {
        return false; // No input scene index, include by default
    }

    HdSceneIndexPrim prim = inputSceneIndex->GetPrim(primPath);

    // Check for lights
    if (_framePassData->_keepLights && HdPrimTypeIsLight(prim.primType)) {
        return false;
    }

    // Check for materials
    if (prim.primType == HdPrimTypeTokens->material) {
        if (_highlightMaterialsUsage.find(primPath) != _highlightMaterialsUsage.end()) {
            return false;
        } else if (_framePassData->_removeMaterials) {
            return true;
        }
    }

    // Now apply the main filtering logic based on purpose render tags
    if (prim.dataSource) {
        const TfToken purposeRenderTag = GetPurposeRenderTag(prim.dataSource);//From fvpUtils
        if (!purposeRenderTag.IsEmpty()) {
            return (_framePassData->_includeRenderTags.find(purposeRenderTag)
                   == _framePassData->_includeRenderTags.end());
        } else if (!_framePassData->_supportPrimsWithNoPurposeRenderTag && !prim.primType.IsEmpty()) {
            return true;
        }
    } else {
        // No data source
    }

    return false; // will be rendered.
}

void PassFilteringSceneIndex::_UpdateFilteringStatus(const PXR_NS::SdfPath& primPath)
{
    if (_ShouldBeFilteredOut(primPath)) {
        _filteredPrims.insert(primPath);
    } else {
        _filteredPrims.erase(primPath);
    }
}

void PassFilteringSceneIndex::_UpdateHighlightMaterialStatus(const PXR_NS::SdfPath& primPath)
{
    if (!_framePassData->_removeMaterials) {
        // If we don't remove the materials, we can completely skip all the highlight material logic
        return;
    }
    if (_framePassData->_highlightHierarchyPrefix.IsEmpty() || !primPath.HasPrefix(_framePassData->_highlightHierarchyPrefix)) {
        // This is not a highlight prim
        return;
    }

    auto prevMaterialPath = _highlightsToMaterialsPaths.find(primPath);
    auto removeMaterialEntry = [&]() {
        if (prevMaterialPath != _highlightsToMaterialsPaths.end()) {
            _highlightsToMaterialsPaths.erase(primPath);
            _highlightMaterialsUsage[prevMaterialPath->second]--;
            if (_highlightMaterialsUsage[prevMaterialPath->second] == 0) {
                _highlightMaterialsUsage.erase(prevMaterialPath->second);
                _filteredPrims.insert(primPath);
                _SendPrimsRemoved({prevMaterialPath->second});
            }
        }
    };

    if (_IsFilteredOut(primPath)) {
        removeMaterialEntry();
    }

    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!(prim.primType == HdPrimTypeTokens->mesh || prim.primType == HdPrimTypeTokens->geomSubset)) {
        return removeMaterialEntry();
    }
    auto materialPath = GetMaterialPath(prim.dataSource);
    if (materialPath.IsEmpty()) {
        return removeMaterialEntry();
    }
    auto materialPrim = GetInputSceneIndex()->GetPrim(materialPath);
    if (!MaterialHasDisplacement(materialPrim)) {
        return removeMaterialEntry();
    }

    if (materialPath != prevMaterialPath->second) {
        removeMaterialEntry();
        // Add the new material entry
        _highlightsToMaterialsPaths[primPath] = materialPath;
        _highlightMaterialsUsage[materialPath]++;
        if (_highlightMaterialsUsage[materialPath] == 1) {
            _filteredPrims.erase(materialPath);
            _SendPrimsAdded({{materialPath, materialPrim.primType}});
        }
    }
}

HdSceneIndexPrim PassFilteringSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (_IsFilteredOut(primPath)) {
        return {}; // Return empty prim
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PassFilteringSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    // If this prim is filtered out and there are no highlight materials under it, return no children
    if (_IsFilteredOut(primPath) && FindSelfOrFirstChild(primPath, _highlightMaterialsUsage) == _highlightMaterialsUsage.cend()) {
        return {};
    }

    // Get children from input scene index
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);

    // Filter out children that should be filtered out
    SdfPathVector filteredChildPaths;
    for (const SdfPath& childPath : childPaths) {
        if (!_IsFilteredOut(childPath) || FindSelfOrFirstChild(primPath, _highlightMaterialsUsage) != _highlightMaterialsUsage.cend()) {
            filteredChildPaths.push_back(childPath);
        }
    }

    return filteredChildPaths;
}

void PassFilteringSceneIndex::DirtyPrimsFromPurposeRenderTag(const TfToken purposeRenderTag)
{
    auto& inputSceneIndex = GetInputSceneIndex();
    if (inputSceneIndex) {
        HdSceneIndexObserver::AddedPrimEntries   newlyUnfilteredEntries;
        HdSceneIndexObserver::RemovedPrimEntries newlyFilteredEntries;
        for (const SdfPath& primPath : HdSceneIndexPrimView(inputSceneIndex)) {
            bool wasPreviouslyFiltered = _IsFilteredOut(primPath);
            _UpdateFilteringStatus(primPath);
            _UpdateHighlightMaterialStatus(primPath);
            if (wasPreviouslyFiltered != _IsFilteredOut(primPath)) {
                // Filtering status changed
                if (wasPreviouslyFiltered) {
                        newlyUnfilteredEntries.emplace_back(
                            primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
                } else {
                        newlyFilteredEntries.emplace_back(primPath);
                }
            }
        }
        _SendPrimsAdded(newlyUnfilteredEntries);
        _SendPrimsRemoved(newlyFilteredEntries);
    }
}

void PassFilteringSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries addedEntries;

    for (const auto& addedEntry : entries) {
        _UpdateFilteringStatus(addedEntry.primPath);
        if (!_IsFilteredOut(addedEntry.primPath)) {
            addedEntries.emplace_back(addedEntry);
            _UpdateHighlightMaterialStatus(addedEntry.primPath);
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
}

void PassFilteringSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;

    for (const auto& removedEntry : entries) {
        if (!_IsFilteredOut(removedEntry.primPath)) {
            removedEntries.emplace_back(removedEntry);
            _UpdateHighlightMaterialStatus(removedEntry.primPath);
        } else {
            _filteredPrims.erase(removedEntry.primPath);
        }
    }

    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
}

void PassFilteringSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    // There are three potential scenarios here for a given prim :
    // 1. Its filtering status did NOT change -> forward the PrimsDirtied notification as-is
    // 2. Its filtering status DID change :
    //    2a. If the prim was previously filtered -> it is now unfiltered, so send a PrimsAdded notification
    //    2b. If the prim was previously unfiltered -> it is now filtered, so send a PrimsRemoved notification
    HdSceneIndexObserver::AddedPrimEntries   newlyUnfilteredEntries;
    HdSceneIndexObserver::RemovedPrimEntries newlyFilteredEntries;
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        bool wasPreviouslyFiltered = _IsFilteredOut(entry.primPath);
        _UpdateFilteringStatus(entry.primPath);
        _UpdateHighlightMaterialStatus(entry.primPath);
        if (wasPreviouslyFiltered == _IsFilteredOut(entry.primPath)) {
            // Filtering status did not change, forward notification as-is
            if (!_IsFilteredOut(entry.primPath)) {
                dirtiedEntries.push_back(entry);
            }
        }
        else {
            // Filtering status changed, send a different notification instead
            if (wasPreviouslyFiltered) {
                    newlyUnfilteredEntries.emplace_back(
                        entry.primPath, GetInputSceneIndex()->GetPrim(entry.primPath).primType);
            } else {
                    newlyFilteredEntries.emplace_back(entry.primPath);
            }
        }
    }
    _SendPrimsAdded(newlyUnfilteredEntries);
    _SendPrimsRemoved(newlyFilteredEntries);
    _SendPrimsDirtied(dirtiedEntries);
}

} // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX
