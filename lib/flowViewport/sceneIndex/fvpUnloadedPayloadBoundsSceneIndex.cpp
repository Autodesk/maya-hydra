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

#include "fvpUnloadedPayloadBoundsSceneIndex.h"

#include "flowViewport/fvpUtils.h"
#include "flowViewport/sceneIndex/fvpBoundsCurvesDataSource.h"

#include <pxr/base/gf/range3d.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

TF_DEFINE_PRIVATE_TOKENS(_tokens, ((boundsPrimName, "__unloadedPayloadBounds")));

// Prims that do not come from a USD stage have no loading status, so are considered loaded.
bool isLoaded(const HdContainerDataSourceHandle& primSource)
{
    const HdBoolDataSourceHandle isLoadedDs
        = UsdImagingUsdPrimInfoSchema::GetFromParent(primSource).GetIsLoaded();
    return !isLoadedDs || isLoadedDs->GetTypedValue(0.0f);
}

bool hasValidExtent(const HdContainerDataSourceHandle& primSource)
{
    const HdExtentSchema          extentSchema = HdExtentSchema::GetFromParent(primSource);
    const HdVec3dDataSourceHandle minDs = extentSchema.GetMin();
    const HdVec3dDataSourceHandle maxDs = extentSchema.GetMax();
    if (!minDs || !maxDs) {
        return false;
    }
    return !GfRange3d(minDs->GetTypedValue(0.0f), maxDs->GetTypedValue(0.0f)).IsEmpty();
}

SdfPath boxPath(const SdfPath& unloadedPrimPath)
{
    return unloadedPrimPath.AppendChild(_tokens->boundsPrimName);
}

const HdDataSourceLocatorSet& displayColorLocators()
{
    static const HdDataSourceLocatorSet locators {
        HdPrimvarsSchema::GetDefaultLocator().Append(HdTokens->displayColor)
    };
    return locators;
}

} // namespace

namespace FVP_NS_DEF {

UnloadedPayloadBoundsSceneIndexRefPtr UnloadedPayloadBoundsSceneIndex::New(
    const HdSceneIndexBaseRefPtr&                   inputSceneIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
    const MayHaveUnloadedPrimsFn&                   mayHaveUnloadedPrims,
    bool                                            enabled)
{
    return TfCreateRefPtr(new UnloadedPayloadBoundsSceneIndex(
        inputSceneIndex, wireframeColorInterface, mayHaveUnloadedPrims, enabled));
}

UnloadedPayloadBoundsSceneIndex::UnloadedPayloadBoundsSceneIndex(
    const HdSceneIndexBaseRefPtr&                   inputSceneIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
    const MayHaveUnloadedPrimsFn&                   mayHaveUnloadedPrims,
    bool                                            enabled)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _wireframeColorInterface(wireframeColorInterface)
    , _mayHaveUnloadedPrims(mayHaveUnloadedPrims)
    , _enabled(enabled)
{
    TF_AXIOM(_wireframeColorInterface);
    TF_AXIOM(_mayHaveUnloadedPrims);

    // The input may already be populated, e.g. when the scene index chain is rebuilt,
    // so traverse the whole scene to initialize the sets to track unloaded prims.
    const HdSceneIndexPrimView view(inputSceneIndex);
    for (auto it = view.begin(); it != view.end(); ++it) {
        const SdfPath& primPath = *it;
        if (!_mayHaveUnloadedPrims(primPath)) {
            it.SkipDescendants();
            continue;
        }
        _UpdateTracking(primPath);
        // The rest of an unloaded subtree is drawn by the box of its root.
        if (_IsTracked(primPath)) {
            it.SkipDescendants();
        }
    }
}

HdSceneIndexPrim UnloadedPayloadBoundsSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (!_unloadedPrims.empty() && primPath.IsPrimPath() && primPath.GetNameToken() == _tokens->boundsPrimName) {
        const SdfPath unloadedPrimPath = primPath.GetParentPath();
        if (_HasBox(unloadedPrimPath)) {
            // The wireframe color of the box is only computed when read.
            return { HdPrimTypeTokens->basisCurves,
                     CreateBoundsCurvesPrimDataSource(
                         GetInputSceneIndex()->GetPrim(unloadedPrimPath).dataSource,
                         _wireframeColorInterface,
                         primPath) };
        }
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector UnloadedPayloadBoundsSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    if (_unloadedPrims.empty()) {
        return childPaths;
    }
    if (_HasBox(primPath)) {
        childPaths.push_back(boxPath(primPath));
    }
    return childPaths;
}

void UnloadedPayloadBoundsSceneIndex::SetEnabled(bool enabled)
{
    if (_enabled == enabled) {
        return;
    }

    _enabled = enabled;

    // Only the boxes of the unloaded prims that have an extent are added or removed.
    if (_unloadedPrimsWithExtents.empty() || !_IsObserved()) {
        return;
    }

    if (enabled) {
        HdSceneIndexObserver::AddedPrimEntries added;
        added.reserve(_unloadedPrimsWithExtents.size());
        for (const SdfPath& path : _unloadedPrimsWithExtents) {
            added.emplace_back(boxPath(path), HdPrimTypeTokens->basisCurves);
        }
        _SendPrimsAdded(added);
    } else {
        HdSceneIndexObserver::RemovedPrimEntries removed;
        removed.reserve(_unloadedPrimsWithExtents.size());
        for (const SdfPath& path : _unloadedPrimsWithExtents) {
            removed.emplace_back(boxPath(path));
        }
        _SendPrimsRemoved(removed);
    }
}

void UnloadedPayloadBoundsSceneIndex::DirtyWireframeColors(const SdfPathVector& primPaths)
{
    if (!_IsObserved() || !_enabled || _unloadedPrims.empty()) {
        return;
    }
    HdSceneIndexObserver::DirtiedPrimEntries dirtied;
    for (const SdfPath& primPath : primPaths) {
        _AddColorDirtiesUnder(primPath, dirtied);
    }
    if (!dirtied.empty()) {
        _SendPrimsDirtied(dirtied);
    }
}

void UnloadedPayloadBoundsSceneIndex::_StopTracking(const SdfPath& primPath)
{
    // Fast path: most scenes track nothing, so skip the erase.
    if (_unloadedPrims.empty()) {
        return;
    }
    if (_unloadedPrims.erase(primPath)) {
        // _unloadedPrimsWithExtents is a subset of _unloadedPrims.
        _unloadedPrimsWithExtents.erase(primPath);
    }
}

void UnloadedPayloadBoundsSceneIndex::_UpdateTracking(const SdfPath& primPath)
{
    // Default assumption : most prims are loaded, so should not be tracked.
    const HdSceneIndexPrim prim = primPath.IsPrimPath() ? GetInputSceneIndex()->GetPrim(primPath): HdSceneIndexPrim();
    if (!prim.dataSource || isLoaded(prim.dataSource)) {
        _StopTracking(primPath);
        return;
    }
    
    // Only the root of an unloaded subtree gets a box, so if the parent is also unloaded,
    // don't track the prim. This parent GetPrim is only reached for unloaded prims.
    if (!isLoaded(GetInputSceneIndex()->GetPrim(primPath.GetParentPath()).dataSource)) {
        _StopTracking(primPath);
        return;
    }

    // Re-add: the prim may already be tracked, possibly with a different extent state.
    _StopTracking(primPath);
    _unloadedPrims.insert(primPath);
    if (hasValidExtent(prim.dataSource)) {
        _unloadedPrimsWithExtents.insert(primPath);
    }
}

void UnloadedPayloadBoundsSceneIndex::_AddColorDirtiesUnder(
    const SdfPath&                            primPath,
    HdSceneIndexObserver::DirtiedPrimEntries& inoutEntries) const
{
    if (!_enabled) {
        return;
    }
    // Only the unloaded prims with an extent have a box.
    for (auto it = _unloadedPrimsWithExtents.lower_bound(primPath);
         it != _unloadedPrimsWithExtents.end() && it->HasPrefix(primPath);
         ++it) {
        inoutEntries.emplace_back(boxPath(*it), displayColorLocators());
    }
}

void UnloadedPayloadBoundsSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    // Box entries go in their own vectors, so the input entries are forwarded without a copy.
    HdSceneIndexObserver::AddedPrimEntries   added;
    HdSceneIndexObserver::RemovedPrimEntries removed;

    for (const auto& entry : entries) {
        if (!_mayHaveUnloadedPrims(entry.primPath)) {
            if (_IsTracked(entry.primPath)) {
                if (_HasBox(entry.primPath)) {
                    removed.emplace_back(boxPath(entry.primPath));
                }
                _StopTracking(entry.primPath);
            }
            continue;
        }

        const bool hadBox = _HasBox(entry.primPath);
        _UpdateTracking(entry.primPath);

        if (_HasBox(entry.primPath)) {
            // Also when the box already existed, as re-adding the unloaded prim resyncs it.
            added.emplace_back(boxPath(entry.primPath), HdPrimTypeTokens->basisCurves);
        } else if (hadBox) {
            // E.g. the prim was loaded.
            removed.emplace_back(boxPath(entry.primPath));
        }
    }

    if (!_IsObserved()) {
        return;
    }
    if (!removed.empty()) {
        _SendPrimsRemoved(removed);
    }
    if (!added.empty()) {
        _SendPrimsAdded(added);
    }
    _SendPrimsAdded(entries);
}

void UnloadedPayloadBoundsSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    // Removing an unloaded prim removes its box, which is its child, so only the bookkeeping
    // needs updating.
    if (!_unloadedPrims.empty()) {
        for (const auto& entry : entries) {
            EraseSelfAndDescendants(entry.primPath, _unloadedPrimsWithExtents);
            EraseSelfAndDescendants(entry.primPath, _unloadedPrims);
        }
    }

    if (_IsObserved()) {
        _SendPrimsRemoved(entries);
    }
}

void UnloadedPayloadBoundsSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (_unloadedPrims.empty()) {
        if (_IsObserved()) {
            _SendPrimsDirtied(entries);
        }
        return;
    }

    static const HdDataSourceLocator& extentLocator = HdExtentSchema::GetDefaultLocator();
    static const HdDataSourceLocator& selectionsLocator = HdSelectionsSchema::GetDefaultLocator();

    // Box entries go in their own vectors, so the input entries are forwarded without a copy.
    HdSceneIndexObserver::DirtiedPrimEntries dirtied;
    HdSceneIndexObserver::AddedPrimEntries   added;
    HdSceneIndexObserver::RemovedPrimEntries removed;

    for (const auto& entry : entries) {
        // The selection is set on the unloaded prim, or one of its ancestors, but its box
        // shows it through its color.
        if (entry.dirtyLocators.Intersects(selectionsLocator)) {
            _AddColorDirtiesUnder(entry.primPath, dirtied);
        }

        if (!_IsTracked(entry.primPath)) {
            continue;
        }

        const bool extentDirtied = entry.dirtyLocators.Intersects(extentLocator);
        const bool hadBox = _HasBox(entry.primPath);
        // A prim that becomes loaded gets a type, so will go through PrimsAdded.
        // We only need to handle extent updates here.
        if (extentDirtied) {
            _UpdateTracking(entry.primPath);
        }
        const bool hasBox = _HasBox(entry.primPath);

        if (hadBox && hasBox) {
            // The box reads the transform, visibility, prim origin and extent of the unloaded
            // prim, so gets the same dirty locators.
            HdDataSourceLocatorSet boxLocators = entry.dirtyLocators;
            if (extentDirtied) {
                boxLocators.insert(GetBoundsCurvesPointsLocator());
            }
            dirtied.emplace_back(boxPath(entry.primPath), boxLocators);
        } else if (hasBox) {
            added.emplace_back(boxPath(entry.primPath), HdPrimTypeTokens->basisCurves);
        } else if (hadBox) {
            removed.emplace_back(boxPath(entry.primPath));
        }
    }

    if (!_IsObserved()) {
        return;
    }
    if (!removed.empty()) {
        _SendPrimsRemoved(removed);
    }
    if (!added.empty()) {
        _SendPrimsAdded(added);
    }
    if (!dirtied.empty()) {
        _SendPrimsDirtied(dirtied);
    }
    _SendPrimsDirtied(entries);
}

} // namespace FVP_NS_DEF
