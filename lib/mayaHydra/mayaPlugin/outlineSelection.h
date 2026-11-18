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
#ifndef MAYAHYDRA_OUTLINE_SELECTION_H
#define MAYAHYDRA_OUTLINE_SELECTION_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/usd/sdf/path.h>

#include <flowViewport/selection/fvpSelection.h>
#include <flowViewport/selection/fvpSelectionTypes.h>
#include <hvt/tasks/outline/outlineTarget.h>

#include <atomic>
#include <unordered_map>
#include <utility>
#include <vector>

// Turns the Flow Viewport selection into the inputs of the Hydra Viewport Toolbox outline: the
// prims selected whole, and the targets that restrict the outline to the selected instances, point
// instances and native instances alike.

namespace MAYAHYDRA_NS_DEF {

//! \brief  Whether \p path is at or below any entry of \p sortedSelectedPaths.
//!
//! \p sortedSelectedPaths must be sorted with SdfPath's default (std::less) ordering, as returned
//! by Selection::GetFullySelectedPaths().
//!
//! Binary-searches each ancestor of \p path, O(depth * log n), since this runs on every hover
//! change and selections can be large.
bool isPathInSelection(
    const PXR_NS::SdfPathVector& sortedSelectedPaths,
    const PXR_NS::SdfPath&       path);

//! \brief  Finds the native instancers drawing the native instances under a prim, for the outline.
//!
//! UsdImaging draws the native instances found under a prim P through instancers that it adds at
//! P/UsdNiPropagatedPrototypes/<binding>/<prototype>/UsdNiInstancer, each drawing one prototype.
//! P is the stage root for the instances outside any prototype, and the prototype root for the
//! instances nested in a prototype, so the instancers drawing the instances at or under a path are
//! under the scope of one of its strict ancestors. Lookups are cached until Clear(): the render
//! override keeps one finder across outline pushes and clears it when the instancers change (see
//! NativeInstancerCache), so that a selection change reads no scope again.
class NativeInstancerFinder
{
public:
    struct Instancer
    {
        PXR_NS::SdfPath path;
        PXR_NS::SdfPath prototype;
        //! (instance location, instance index) pairs, sorted by location. SdfPath sorts
        //! dictionary-style, so the instances at or under a path are the range that starts at the
        //! path: a lookup costs O(log n + matches), not a scan of every instance.
        std::vector<std::pair<PXR_NS::SdfPath, int>> sortedLocations;

        //! Append to \p indices the indices of the instances at or under \p path, in location
        //! order.
        void AppendInstancesUnder(const PXR_NS::SdfPath& path, std::vector<int>& indices) const;
    };
    using Instancers = std::vector<Instancer>;

    explicit NativeInstancerFinder(PXR_NS::HdSceneIndexBaseRefPtr sceneIndex);

    //! The native instancers that UsdImaging added for the native instances under \p root.
    const Instancers& GetInstancersUnder(const PXR_NS::SdfPath& root);

    //! Forget every lookup, for when the instancers may have changed.
    void Clear() { _instancersByRoot.clear(); }

private:
    PXR_NS::HdSceneIndexBaseRefPtr                                         _sceneIndex;
    std::unordered_map<PXR_NS::SdfPath, Instancers, PXR_NS::SdfPath::Hash> _instancersByRoot;
};

//! \brief  Keeps the native instancer lookups of the outline across selection changes.
//!
//! Reading and sorting the instance locations of many native instances costs more than the rest of
//! a selection change. The lookups are cleared when UsdImaging may have changed its native
//! instancers: a prim added, or dirtied in its instancer topology, under a
//! UsdNiPropagatedPrototypes scope, or any prim removed or renamed (such a subtree can hold a scope
//! without naming it). The notices only set a flag: the lookups belong to the render thread.
class NativeInstancerCache : public PXR_NS::HdSceneIndexObserver
{
public:
    explicit NativeInstancerCache(PXR_NS::HdSceneIndexBaseRefPtr sceneIndex);
    ~NativeInstancerCache() override;

    //! The cached lookups, cleared first if the native instancers changed since the last call.
    NativeInstancerFinder& GetFinder();

    void PrimsAdded(const PXR_NS::HdSceneIndexBase&, const AddedPrimEntries& entries) override;
    void PrimsRemoved(const PXR_NS::HdSceneIndexBase&, const RemovedPrimEntries& entries) override;
    void PrimsDirtied(const PXR_NS::HdSceneIndexBase&, const DirtiedPrimEntries& entries) override;
    void PrimsRenamed(const PXR_NS::HdSceneIndexBase&, const RenamedPrimEntries& entries) override;

private:
    void _InstancersChanged() { _instancersChanged = true; }

    PXR_NS::HdSceneIndexBaseRefPtr _sceneIndex;
    NativeInstancerFinder          _finder;
    std::atomic<bool>              _instancersChanged = { false };
};

//! \brief  Split the Fvp selection into the outline's whole-prim paths and instance targets.
//!
//! A prim with a PrimSelection that carries no instance indices is selected whole and goes to
//! \p selectedPaths, sorted like Selection::GetFullySelectedPaths() (isPathInSelection() relies on
//! it). Otherwise its PrimSelections become targets of \p selectedTargets, merged per instancer:
//! the targets with one level on the same instancer become one target listing the sorted union of
//! their indices. The selected native instances add targets on their prototypes, found through
//! \p nativeInstancerFinder.
void getOutlineSelection(
    const Fvp::Selection&            selection,
    NativeInstancerFinder&           nativeInstancerFinder,
    PXR_NS::SdfPathVector&           selectedPaths,
    HVT_NS::Outline::OutlineTargets& selectedTargets);

//! \brief  Split the lead object's prim selections into the outline's lead path and lead targets.
//!
//! The first prim selection without instance indices goes to \p leadPath, the others to
//! \p leadTargets: level-less for the other whole prims, so every prim of the lead gets the lead
//! color, with instance levels for instance selections, so that only the lead instances do. The
//! native instances of the lead add targets on their prototypes; then all the whole prims are
//! level-less targets, as a native instance prim resolves to no prim ID and would make \p leadPath
//! warn.
void getOutlineLead(
    const Fvp::PrimSelections&       leadSelections,
    NativeInstancerFinder&           nativeInstancerFinder,
    PXR_NS::SdfPath&                 leadPath,
    HVT_NS::Outline::OutlineTargets& leadTargets);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_OUTLINE_SELECTION_H
