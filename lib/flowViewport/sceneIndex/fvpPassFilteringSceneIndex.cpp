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

#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/repr.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>

#include <iostream>
#include <algorithm>

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

bool isRprimType(const TfToken& primType)
{
    return std::find(HdRprimTypeTokens->allTokens.begin(), HdRprimTypeTokens->allTokens.end(), primType) != HdRprimTypeTokens->allTokens.end();
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
        if (_framePassData->_passIndex == 0) {
            std::cout << "Constructor processing " << primPath << std::endl;
        }
        _UpdateFilteringStatus(primPath);
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

    if (!isRprimType(prim.primType)) {
        if (prim.primType == HdPrimTypeTokens->material 
            && _highlightMaterialsUsage.find(primPath) == _highlightMaterialsUsage.end()) {
            // Filter out non-highlight materials
            return true;
        }
        return false; // Include all non-Rprims by default
    }

    // Now apply the main filtering logic based on purpose render tags
    if (prim.dataSource) {
        const TfToken purposeRenderTag = GetPurposeRenderTag(prim.dataSource);//From fvpUtils
        if (!purposeRenderTag.IsEmpty()) {
            return (_framePassData->_includeRenderTags.find(purposeRenderTag)
                   == _framePassData->_includeRenderTags.end());
        } else {
            return !_framePassData->_supportPrimsWithNoPurposeRenderTag;
        }
    } else {
        // No data source
    }

    return false; // will be rendered.
}

HdSceneIndexObserver::AddedPrimEntries PassFilteringSceneIndex::_UpdateFilteringStatus(const PXR_NS::SdfPath& primPath, bool dirtied, bool resync)
{
    HdSceneIndexObserver::AddedPrimEntries updatedPrims;

    bool isFilteredOut = _IsFilteredOut(primPath);
    bool shouldBeFilteredOut = _ShouldBeFilteredOut(primPath);

    if (!isFilteredOut && shouldBeFilteredOut) {
        _filteredPrims.insert(primPath);
        if (_framePassData->_passIndex == 0) {
            std::cout << "Filtering out (_UpdateFilteringStatus) " << primPath << std::endl;
        }
        updatedPrims.emplace_back(primPath, TfToken());
    } else if (isFilteredOut && !shouldBeFilteredOut) {
        _filteredPrims.erase(primPath);
        if (_framePassData->_passIndex == 0) {
            std::cout << "Unfiltering (_UpdateFilteringStatus) " << primPath << std::endl;
        }
        updatedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
    } else if (!isFilteredOut && !shouldBeFilteredOut && resync) {
        if (_framePassData->_passIndex == 0) {
            std::cout << "Adding or resyncing (_UpdateFilteringStatus) " << primPath << std::endl;
        }
        updatedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
    }

    bool updateHighlightMaterial = (isFilteredOut != shouldBeFilteredOut) || (!isFilteredOut && !shouldBeFilteredOut && dirtied);
    if (updateHighlightMaterial) {
        auto materialUpdates = _UpdateHighlightMaterialStatus(primPath);
        updatedPrims.insert(updatedPrims.end(), materialUpdates.begin(), materialUpdates.end());
    }

    return updatedPrims;
}

HdSceneIndexObserver::AddedPrimEntries PassFilteringSceneIndex::_RemoveHighlightMaterialEntry(const PXR_NS::SdfPath& primPath)
{
    auto itMaterialPath = _highlightsToMaterialsPaths.find(primPath);
    if (itMaterialPath != _highlightsToMaterialsPaths.end()) {
        auto materialPath = itMaterialPath->second;
        _highlightsToMaterialsPaths.erase(primPath);
        _highlightMaterialsUsage[materialPath]--;
        if (_highlightMaterialsUsage[materialPath] == 0) {
            _highlightMaterialsUsage.erase(materialPath);
            if (_ShouldBeFilteredOut(materialPath)) {
                if (_framePassData->_passIndex == 0) {
                    std::cout << "Material filtering out " << materialPath << " from " << primPath << std::endl;
                }
                _filteredPrims.insert(materialPath);
                return {{materialPath, TfToken()}};
            }
        }
    }

    return {};
}

HdSceneIndexObserver::AddedPrimEntries PassFilteringSceneIndex::_UpdateHighlightMaterialStatus(const PXR_NS::SdfPath& primPath)
{
    if (_IsFilteredOut(primPath)) {
        return _RemoveHighlightMaterialEntry(primPath);
    }
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!isRprimType(prim.primType)) {
        return _RemoveHighlightMaterialEntry(primPath);
    }
    auto materialPath = GetMaterialPath(prim.dataSource);
    if (materialPath.IsEmpty()) {
        return _RemoveHighlightMaterialEntry(primPath);
    }

    // Do some checks to see if the material needs to have displacement to be relevant
    bool requireDisplacement = prim.primType == HdPrimTypeTokens->basisCurves;
    if (!requireDisplacement) {
        auto displayStyle = HdLegacyDisplayStyleSchema::GetFromParent(prim.dataSource);
        if (displayStyle.IsDefined() && displayStyle.GetReprSelector()) {
            auto reprSelector = displayStyle.GetReprSelector()->GetTypedValue(0);
            if (reprSelector[0] == HdReprTokens->wire || reprSelector[0] == HdReprTokens->refinedWire) {
                requireDisplacement = true;
            }
        }
    }
    if (requireDisplacement && !MaterialHasDisplacement(GetInputSceneIndex()->GetPrim(materialPath))) {
        return _RemoveHighlightMaterialEntry(primPath);
    }

    auto prevMaterialPath = _highlightsToMaterialsPaths.find(primPath);
    if (prevMaterialPath == _highlightsToMaterialsPaths.end() || materialPath != prevMaterialPath->second) {
        auto updatedPrims = _RemoveHighlightMaterialEntry(primPath);
        // Add the new material entry
        _highlightsToMaterialsPaths[primPath] = materialPath;
        _highlightMaterialsUsage[materialPath]++;
        if (_highlightMaterialsUsage[materialPath] == 1) {
            if (_IsFilteredOut(materialPath)) {
                _filteredPrims.erase(materialPath);
                if (_framePassData->_passIndex == 0) {
                    std::cout << "Material unfiltering in " << materialPath << " from " << primPath << std::endl;
                }
                updatedPrims.emplace_back(materialPath, HdPrimTypeTokens->material);
            }
        }
        return updatedPrims;
    }

    return {};
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
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void PassFilteringSceneIndex::DirtyPrimsFromPurposeRenderTag(const TfToken purposeRenderTag)
{
    auto& inputSceneIndex = GetInputSceneIndex();
    if (inputSceneIndex) {
        HdSceneIndexObserver::AddedPrimEntries updatedEntries;
        for (const SdfPath& primPath : HdSceneIndexPrimView(inputSceneIndex)) {
            if (_framePassData->_passIndex == 0) {
                std::cout << "DirtyPrimsFromPurposeRenderTag processing " << primPath << std::endl;
            }
            auto updatedPrims = _UpdateFilteringStatus(primPath, false);
            updatedEntries.insert(updatedEntries.end(), updatedPrims.begin(), updatedPrims.end());
        }
        if (!updatedEntries.empty()) {
            _SendPrimsAdded(updatedEntries);
        }
    }
}

void PassFilteringSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries addedEntries;

    for (const auto& addedEntry : entries) {
        if (_framePassData->_passIndex == 0) {
            std::cout << "Processing _PrimsAdded " << addedEntry.primPath << std::endl;
        }
        auto updatedEntries = _UpdateFilteringStatus(addedEntry.primPath, true, true);
        addedEntries.insert(addedEntries.end(), updatedEntries.begin(), updatedEntries.end());
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
    HdSceneIndexObserver::AddedPrimEntries updatedPrims;

    for (const auto& removedEntry : entries) {
        removedEntries.emplace_back(removedEntry);
        if (_framePassData->_passIndex == 0) {
            std::cout << "Processing _PrimsRemoved " << removedEntry.primPath << std::endl;
        }
        for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
            if ((*it).HasPrefix(removedEntry.primPath)) {
                if (_framePassData->_passIndex == 0) {
                    std::cout << "Removing  " << (*it) << " from filtered prims due to " << removedEntry.primPath  << " removed " << std::endl;
                }
                it = _filteredPrims.erase(it);
            } else {
                it++;
            }
        }
        for (const auto& primPath : HdSceneIndexPrimView(GetInputSceneIndex(), removedEntry.primPath)) {
            auto materialUpdates = _RemoveHighlightMaterialEntry(primPath);
            updatedPrims.insert(updatedPrims.end(), materialUpdates.begin(), materialUpdates.end());
        }
    }

    if (!updatedPrims.empty()) {
        _SendPrimsAdded(updatedPrims);
    }
    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
}

void PassFilteringSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries   updatedEntries;
    for (const auto& entry : entries) {
        if (_framePassData->_passIndex == 0) {
            std::cout << "Processing _PrimsDirtied (update) " << entry.primPath << std::endl;
        }
        auto updatedPrims = _UpdateFilteringStatus(entry.primPath);
        updatedEntries.insert(updatedEntries.end(), updatedPrims.begin(), updatedPrims.end());
    }
    if (!updatedEntries.empty()) {
        _SendPrimsAdded(updatedEntries);
    }

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        if (_framePassData->_passIndex == 0) {
            std::cout << "Processing _PrimsDirtied (dirty) " << entry.primPath << std::endl;
        }
        if (!_IsFilteredOut(entry.primPath)) {
            if (_framePassData->_passIndex == 0) {
                std::cout << "Adding dirty notification for " << entry.primPath << std::endl;
            }
            dirtiedEntries.emplace_back(entry);
        }
    }
    if (!dirtiedEntries.empty()) {
        _SendPrimsDirtied(dirtiedEntries);
    }
}

} // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX
