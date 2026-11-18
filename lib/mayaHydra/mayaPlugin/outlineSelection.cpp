//
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
#include "outlineSelection.h"

#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <set>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

namespace {

//! \brief  The outline target of a prim selection.
//!
//! One level per instancer of its nested instance indices, none when it carries no instance
//! indices (the whole prim). The indices are instancer-wide on both sides, so they pass through
//! unchanged, and the prototype index is dropped: an instancer-wide index belongs to one prototype.
//! Paths are in the render index namespace, as the selection is.
HVT_NS::Outline::OutlineTarget toOutlineTarget(const Fvp::PrimSelection& primSelection)
{
    HVT_NS::Outline::OutlineTarget target { primSelection.primPath, {} };
    for (const Fvp::InstancesSelection& instances : primSelection.nestedInstanceIndices) {
        // An OutlineTarget has one level per instancer: merge the entries of one instancer that
        // differ only by prototype.
        auto level = std::find_if(
            target.instanceLevels.begin(),
            target.instanceLevels.end(),
            [&](const HVT_NS::Outline::OutlineInstanceLevel& l) {
                return l.instancer == instances.instancerPath;
            });
        if (level == target.instanceLevels.end()) {
            target.instanceLevels.push_back({ instances.instancerPath, {} });
            level = std::prev(target.instanceLevels.end());
        }
        for (int instanceIndex : instances.instanceIndices) {
            level->instanceIndices.push_back(instanceIndex);
        }
    }
    return target;
}

//! \brief  Append the outline targets of one prim's selections to \p targets, merged.
//!
//! The Instances pick mode adds one PrimSelection per selected instance, so a marquee of N
//! instances would give N targets. Hydra Viewport Toolbox merges them as well, but the targets
//! cached by the caller are copied and compared on every outline push, hover changes included, so
//! they are merged once, on the selection change: the targets with one level on the same instancer
//! become one target listing the sorted union of their indices. Targets with several levels are
//! kept apart: a union of intersections is not the intersection of the unions.
void appendMergedOutlineTargets(
    const Fvp::PrimSelections&       primSelections,
    HVT_NS::Outline::OutlineTargets& targets)
{
    const auto first = static_cast<std::ptrdiff_t>(targets.size());
    bool       merged = false;
    for (const Fvp::PrimSelection& primSelection : primSelections) {
        HVT_NS::Outline::OutlineTarget target = toOutlineTarget(primSelection);
        if (target.instanceLevels.size() == 1) {
            // All the selections of one prim share its path: match on the instancer only.
            const SdfPath& instancer = target.instanceLevels[0].instancer;
            auto           found = std::find_if(
                targets.begin() + first,
                targets.end(),
                [&](const HVT_NS::Outline::OutlineTarget& t) {
                    return t.instanceLevels.size() == 1
                        && t.instanceLevels[0].instancer == instancer;
                });
            if (found != targets.end()) {
                VtIntArray& indices = found->instanceLevels[0].instanceIndices;
                for (int instanceIndex : target.instanceLevels[0].instanceIndices) {
                    indices.push_back(instanceIndex);
                }
                merged = true;
                continue;
            }
        }
        targets.push_back(std::move(target));
    }

    if (!merged) {
        return;
    }
    // Sorted and deduplicated, so that the targets do not depend on the selection order.
    for (auto it = targets.begin() + first; it != targets.end(); ++it) {
        if (it->instanceLevels.size() != 1) {
            continue;
        }
        VtIntArray&      indices = it->instanceLevels[0].instanceIndices;
        std::vector<int> sorted(indices.cbegin(), indices.cend());
        std::sort(sorted.begin(), sorted.end());
        sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
        indices = VtIntArray(sorted.begin(), sorted.end());
    }
}

//! The scope under which UsdImaging adds the native instancers. A UsdImaging name
//! (niInstanceAggregationSceneIndex.cpp), not exported.
const TfToken& propagatedPrototypesScope()
{
    static const TfToken scope("UsdNiPropagatedPrototypes");
    return scope;
}

//! Whether \p path is at or under a UsdNiPropagatedPrototypes scope, where UsdImaging keeps its
//! native instancers.
bool isUnderPropagatedPrototypes(const SdfPath& path)
{
    for (SdfPath p = path; !p.IsEmpty() && !p.IsAbsoluteRootPath(); p = p.GetParentPath()) {
        if (p.GetNameToken() == propagatedPrototypesScope()) {
            return true;
        }
    }
    return false;
}

//! \brief  Append the outline targets of the native instances that \p sortedWholePaths and
//!         \p targets select.
//!
//! A native instance prim has no rprim under it: its instancer draws its prototype elsewhere in the
//! namespace. So neither a path selected whole nor a target outlines the native instances at or
//! under its path. For each native instancer drawing some of them, a target on its prototype keeps
//! them: its levels are those of the selecting target (none for a whole path), plus one on the
//! instancer.
//! The instance index is the position in instanceLocations, as in HdInstanceSchema::instanceIndex.
//! Instancers drawn under the selecting path are skipped: their rprims are already under it.
//!
//! \p sortedWholePaths must be sorted, as for isPathInSelection().
void appendNativeInstanceTargets(
    NativeInstancerFinder&           finder,
    const SdfPathVector&             sortedWholePaths,
    HVT_NS::Outline::OutlineTargets& targets)
{
    // The roots whose scope can hold the instancers of the instances at or under path.
    const auto forEachRoot = [](const SdfPath& path, auto&& fn) {
        for (SdfPath root = path.GetParentPath(); !root.IsEmpty();
             root = root.IsAbsoluteRootPath() ? SdfPath() : root.GetParentPath()) {
            fn(root);
        }
    };

    // Ascending and unique, as the instance indices of a target are expected to be.
    const auto toTargetIndices = [](std::vector<int>& indices) {
        std::sort(indices.begin(), indices.end());
        indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
        return VtIntArray(indices.begin(), indices.end());
    };

    // Targets with instance levels: the native instances under a point instance, or nested in the
    // prototype of another native instance. Appended targets are not visited again.
    const size_t targetCount = targets.size();
    for (size_t iTarget = 0; iTarget < targetCount; ++iTarget) {
        if (targets[iTarget].instanceLevels.empty()) {
            continue;
        }
        // Copies: push_back() below can reallocate targets.
        const SdfPath path = targets[iTarget].path;
        const auto    levels = targets[iTarget].instanceLevels;
        forEachRoot(path, [&](const SdfPath& root) {
            for (const auto& instancer : finder.GetInstancersUnder(root)) {
                if (instancer.path.HasPrefix(path)) {
                    continue;
                }
                std::vector<int> indices;
                instancer.AppendInstancesUnder(path, indices);
                if (!indices.empty()) {
                    HVT_NS::Outline::OutlineTarget target { instancer.prototype, levels };
                    target.instanceLevels.push_back({ instancer.path, toTargetIndices(indices) });
                    targets.push_back(std::move(target));
                }
            }
        });
    }

    // Paths selected whole: one target per instancer, with the instances of all of them. An
    // instance is in the selection when one of the paths is a prefix of its location, so each path
    // looks up its own range, instead of testing every instance against the selection.
    std::set<SdfPath> roots;
    for (const SdfPath& path : sortedWholePaths) {
        forEachRoot(path, [&](const SdfPath& root) { roots.insert(root); });
    }
    for (const SdfPath& root : roots) {
        for (const auto& instancer : finder.GetInstancersUnder(root)) {
            if (isPathInSelection(sortedWholePaths, instancer.path)) {
                continue;
            }
            std::vector<int> indices;
            for (const SdfPath& path : sortedWholePaths) {
                instancer.AppendInstancesUnder(path, indices);
            }
            if (!indices.empty()) {
                targets.push_back(
                    { instancer.prototype, { { instancer.path, toTargetIndices(indices) } } });
            }
        }
    }
}

} // namespace

bool isPathInSelection(const SdfPathVector& sortedSelectedPaths, const SdfPath& path)
{
    if (sortedSelectedPaths.empty() || path.IsEmpty()) {
        return false;
    }
    for (SdfPath p = path; !p.IsEmpty();) {
        if (std::binary_search(sortedSelectedPaths.begin(), sortedSelectedPaths.end(), p)) {
            return true;
        }
        // GetParentPath() of the absolute root is itself.
        const SdfPath parent = p.GetParentPath();
        if (parent == p) {
            break;
        }
        p = parent;
    }
    return false;
}

void NativeInstancerFinder::Instancer::AppendInstancesUnder(
    const SdfPath&    path,
    std::vector<int>& indices) const
{
    auto it = std::lower_bound(
        sortedLocations.begin(),
        sortedLocations.end(),
        path,
        [](const std::pair<SdfPath, int>& location, const SdfPath& p) {
            return location.first < p;
        });
    for (; it != sortedLocations.end() && it->first.HasPrefix(path); ++it) {
        indices.push_back(it->second);
    }
}

NativeInstancerFinder::NativeInstancerFinder(HdSceneIndexBaseRefPtr sceneIndex)
    : _sceneIndex(std::move(sceneIndex))
{
}

const NativeInstancerFinder::Instancers&
NativeInstancerFinder::GetInstancersUnder(const SdfPath& root)
{
    auto [it, inserted] = _instancersByRoot.try_emplace(root);
    if (!inserted || !_sceneIndex) {
        return it->second;
    }
    const SdfPath scope = root.AppendChild(propagatedPrototypesScope());
    for (const SdfPath& binding : _sceneIndex->GetChildPrimPaths(scope)) {
        for (const SdfPath& prototypeBase : _sceneIndex->GetChildPrimPaths(binding)) {
            for (const SdfPath& instancerPath : _sceneIndex->GetChildPrimPaths(prototypeBase)) {
                const HdSceneIndexPrim prim = _sceneIndex->GetPrim(instancerPath);
                if (prim.primType != HdPrimTypeTokens->instancer) {
                    continue;
                }
                const HdInstancerTopologySchema topology
                    = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
                const HdPathArrayDataSourceHandle prototypesDs = topology.GetPrototypes();
                const HdPathArrayDataSourceHandle locationsDs = topology.GetInstanceLocations();
                if (!prototypesDs || !locationsDs) {
                    continue;
                }
                const VtArray<SdfPath> prototypes = prototypesDs->GetTypedValue(0.0f);
                if (prototypes.size() != 1) {
                    continue;
                }
                // The instance index is the position in instanceLocations, as in
                // HdInstanceSchema::instanceIndex.
                const VtArray<SdfPath> locations = locationsDs->GetTypedValue(0.0f);
                Instancer              instancer { instancerPath, prototypes[0], {} };
                instancer.sortedLocations.reserve(locations.size());
                for (size_t i = 0; i < locations.size(); ++i) {
                    instancer.sortedLocations.emplace_back(locations[i], static_cast<int>(i));
                }
                // UsdImaging lists the locations sorted in practice: check before sorting,
                // which is the costly part for many instances.
                if (!std::is_sorted(
                        instancer.sortedLocations.begin(), instancer.sortedLocations.end())) {
                    std::sort(instancer.sortedLocations.begin(), instancer.sortedLocations.end());
                }
                it->second.push_back(std::move(instancer));
            }
        }
    }
    return it->second;
}

NativeInstancerCache::NativeInstancerCache(HdSceneIndexBaseRefPtr sceneIndex)
    : _sceneIndex(std::move(sceneIndex))
    , _finder(_sceneIndex)
{
    _sceneIndex->AddObserver(HdSceneIndexObserverPtr(this));
}

NativeInstancerCache::~NativeInstancerCache()
{
    _sceneIndex->RemoveObserver(HdSceneIndexObserverPtr(this));
}

NativeInstancerFinder& NativeInstancerCache::GetFinder()
{
    if (_instancersChanged.exchange(false)) {
        _finder.Clear();
    }
    return _finder;
}

void NativeInstancerCache::PrimsAdded(const HdSceneIndexBase&, const AddedPrimEntries& entries)
{
    for (const AddedPrimEntry& entry : entries) {
        if (isUnderPropagatedPrototypes(entry.primPath)) {
            _InstancersChanged();
            return;
        }
    }
}

void NativeInstancerCache::PrimsRemoved(const HdSceneIndexBase&, const RemovedPrimEntries& entries)
{
    if (!entries.empty()) {
        _InstancersChanged();
    }
}

void NativeInstancerCache::PrimsDirtied(const HdSceneIndexBase&, const DirtiedPrimEntries& entries)
{
    for (const DirtiedPrimEntry& entry : entries) {
        // The locator test first: it is the cheaper one, and rarely passes.
        if (entry.dirtyLocators.Intersects(HdInstancerTopologySchema::GetDefaultLocator())
            && isUnderPropagatedPrototypes(entry.primPath)) {
            _InstancersChanged();
            return;
        }
    }
}

void NativeInstancerCache::PrimsRenamed(const HdSceneIndexBase&, const RenamedPrimEntries& entries)
{
    if (!entries.empty()) {
        _InstancersChanged();
    }
}

void getOutlineSelection(
    const Fvp::Selection&            selection,
    NativeInstancerFinder&           nativeInstancerFinder,
    SdfPathVector&                   selectedPaths,
    HVT_NS::Outline::OutlineTargets& selectedTargets)
{
    selectedPaths.clear();
    selectedTargets.clear();
    for (const auto& [primPath, primSelections] : selection) {
        const bool selectedWhole = std::any_of(
            primSelections.begin(), primSelections.end(), [](const Fvp::PrimSelection& s) {
                return s.nestedInstanceIndices.empty();
            });
        if (selectedWhole) {
            selectedPaths.push_back(primPath);
            continue;
        }

        appendMergedOutlineTargets(primSelections, selectedTargets);
    }
    appendNativeInstanceTargets(nativeInstancerFinder, selectedPaths, selectedTargets);
}

void getOutlineLead(
    const Fvp::PrimSelections&       leadSelections,
    NativeInstancerFinder&           nativeInstancerFinder,
    SdfPath&                         leadPath,
    HVT_NS::Outline::OutlineTargets& leadTargets)
{
    leadPath = SdfPath();
    leadTargets.clear();
    SdfPathVector wholePaths;
    for (const Fvp::PrimSelection& primSelection : leadSelections) {
        if (primSelection.nestedInstanceIndices.empty()) {
            wholePaths.push_back(primSelection.primPath);
        } else {
            leadTargets.push_back(toOutlineTarget(primSelection));
        }
    }

    SdfPathVector sortedWholePaths = wholePaths;
    std::sort(sortedWholePaths.begin(), sortedWholePaths.end());
    const size_t targetCount = leadTargets.size();
    appendNativeInstanceTargets(nativeInstancerFinder, sortedWholePaths, leadTargets);
    const bool hasNativeInstances = leadTargets.size() > targetCount;

    for (const SdfPath& path : wholePaths) {
        if (leadPath.IsEmpty() && !hasNativeInstances) {
            leadPath = path;
        } else {
            leadTargets.push_back({ path, {} });
        }
    }
}

} // namespace MAYAHYDRA_NS_DEF
