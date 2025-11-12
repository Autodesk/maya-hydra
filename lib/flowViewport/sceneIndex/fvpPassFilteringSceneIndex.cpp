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

    if (std::find(HdRprimTypeTokens->allTokens.begin(), HdRprimTypeTokens->allTokens.end(), prim.primType) == HdRprimTypeTokens->allTokens.end()) {
        if (prim.primType == HdPrimTypeTokens->material 
            && _framePassData->_removeMaterials 
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
        updatedPrims.emplace_back(primPath, TfToken());
    } else if (isFilteredOut && !shouldBeFilteredOut) {
        _filteredPrims.erase(primPath);
        updatedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
    } else if (!isFilteredOut && !shouldBeFilteredOut && resync) {
        updatedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
    }

    bool updateHighlightMaterial = (isFilteredOut != shouldBeFilteredOut) || (!isFilteredOut && !shouldBeFilteredOut && dirtied);
    if (updateHighlightMaterial) {
        auto materialUpdates = _UpdateHighlightMaterialStatus(primPath);
        updatedPrims.insert(updatedPrims.end(), materialUpdates.begin(), materialUpdates.end());
    }

    return updatedPrims;
}

HdSceneIndexObserver::AddedPrimEntries PassFilteringSceneIndex::_UpdateHighlightMaterialStatus(const PXR_NS::SdfPath& primPath)
{
    if (!_framePassData->_removeMaterials) {
        // If we don't remove the materials, we can completely skip all the highlight material logic
        return {};
    }
    bool isMayaFacesHighlightPrim = primPath.GetName().find("PolyActiveFaces") != std::string::npos;
    bool isFvpHighlightPrim = !_framePassData->_highlightHierarchyPrefix.IsEmpty() && primPath.HasPrefix(_framePassData->_highlightHierarchyPrefix);
    if (!(isMayaFacesHighlightPrim || isFvpHighlightPrim)) {
        // Not a relevant prim
        return {};
    }

    auto prevMaterialPath = _highlightsToMaterialsPaths.find(primPath);
    auto removeMaterialEntry = [&]() -> HdSceneIndexObserver::AddedPrimEntries {
        if (prevMaterialPath != _highlightsToMaterialsPaths.end()) {
            _highlightsToMaterialsPaths.erase(primPath);
            _highlightMaterialsUsage[prevMaterialPath->second]--;
            if (_highlightMaterialsUsage[prevMaterialPath->second] == 0) {
                _highlightMaterialsUsage.erase(prevMaterialPath->second);
                if (_ShouldBeFilteredOut(prevMaterialPath->second)) {
                    _filteredPrims.insert(prevMaterialPath->second);
                    return {{prevMaterialPath->second, TfToken()}};
                }
            }
        }
        return {};
    };

    if (_IsFilteredOut(primPath)) {
        return removeMaterialEntry();
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
    if (isFvpHighlightPrim && !MaterialHasDisplacement(materialPrim)) {
        return removeMaterialEntry();
    }

    if (materialPath != prevMaterialPath->second) {
        auto updatedPrims = removeMaterialEntry();
        // Add the new material entry
        _highlightsToMaterialsPaths[primPath] = materialPath;
        _highlightMaterialsUsage[materialPath]++;
        if (_highlightMaterialsUsage[materialPath] == 1) {
            if (_IsFilteredOut(materialPath)) {
                _filteredPrims.erase(materialPath);
                updatedPrims.emplace_back(materialPath, materialPrim.primType);
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
        for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
            if ((*it).HasPrefix(removedEntry.primPath)) {
                it = _filteredPrims.erase(it);
            } else {
                it++;
            }
        }
        auto materialUpdates = _UpdateHighlightMaterialStatus(removedEntry.primPath);
        updatedPrims.insert(updatedPrims.end(), materialUpdates.begin(), materialUpdates.end());
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
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        auto updatedPrims = _UpdateFilteringStatus(entry.primPath);
        updatedEntries.insert(updatedEntries.end(), updatedPrims.begin(), updatedPrims.end());
        if (!_IsFilteredOut(entry.primPath)) {
            dirtiedEntries.emplace_back(entry);
        }
    }
    if (!updatedEntries.empty()) {
        _SendPrimsAdded(updatedEntries);
    }
    if (!dirtiedEntries.empty()) {
        _SendPrimsDirtied(dirtiedEntries);
    }
}

} // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX
