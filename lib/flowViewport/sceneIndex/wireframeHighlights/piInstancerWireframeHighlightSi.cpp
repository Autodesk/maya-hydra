#include "piInstancerWireframeHighlightSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <algorithm>
#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Returns all paths related to instancing for this prim; this is analogous to getting the edges
// connected to the given vertex (in this case a prim) of an instancing graph.
SdfPathVector _GetInstancingRelatedPaths(const HdSceneIndexPrim& prim, Fvp::SelectionHighlightsCollectionDirection direction)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    
    SdfPathVector instancingRelatedPaths;

    if ((direction & Fvp::SelectionHighlightsCollectionDirection::Prototypes)
        && instancerTopology.IsDefined()) {
        auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
        for (const auto& protoPath : protoPaths) {
            instancingRelatedPaths.push_back(protoPath);
        }
    }

    if ((direction & Fvp::SelectionHighlightsCollectionDirection::InstancedBy)
        && instancedBy.IsDefined()) {
        auto instancerPaths = instancedBy.GetPaths()->GetTypedValue(0);
        for (const auto& instancerPath : instancerPaths) {
            instancingRelatedPaths.push_back(instancerPath);
        }

        // Having a prototype root is not a hard requirement (a single prim being instanced
        // does not need to specify itself as its own prototype root).
        if (instancedBy.GetPrototypeRoots()) {
            auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
            for (const auto& protoRootPath : protoRootPaths) {
                instancingRelatedPaths.push_back(protoRootPath);
            }
        }
    }

    return instancingRelatedPaths;
}

bool _IsPrototype(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined();
}

bool _IsPrototypeSubPrim(const HdSceneIndexPrim& prim, const SdfPath& primPath)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBy.IsDefined()) {
        return false;
    }
    if (!instancedBy.GetPrototypeRoots()) {
        return false;
    }
    auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
    for (const auto& protoRootPath : protoRootPaths) {
        if (protoRootPath == primPath) {
            return false;
        }
    }
    return true;
}

// We consider prototypes that have child prims to be different hierarchies,
// separate from each other and from the "root" hierarchy.
VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
}

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr PointInstancerWireframeHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& instancerPrimPath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PointInstancerWireframeHighlightSceneIndex(inputSceneIndex, instancerPrimPath, selectionIndex, wireframeColorInterface));
}

HdSceneIndexPrim PointInstancerWireframeHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    if (_IsRelevantPath(primPath)) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
        if (prim.primType == HdPrimTypeTokens->mesh) {
            prim.dataSource = MakeWireframe(prim.dataSource, _wireframeColorInterface->getWireframeColor(_instancerPrimPath));
        }
        return prim;
    }
    return {};
};

SdfPathVector PointInstancerWireframeHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    SdfPathVector originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector prunedChildPaths;
    for (const auto& originalChildPath : originalChildPaths) {
        for (const auto& instancerPath : _instancerPaths) {
            if (instancerPath.HasPrefix(originalChildPath)) {
                prunedChildPaths.push_back(originalChildPath);
                break;
            }
        }

        for (const auto& prototypePath : _prototypePaths) {
            if (prototypePath.HasPrefix(originalChildPath) || originalChildPath.HasPrefix(prototypePath)) {
                prunedChildPaths.push_back(originalChildPath);
                break;
            }
        }
    }
    return prunedChildPaths;
}

PointInstancerWireframeHighlightSceneIndex::PointInstancerWireframeHighlightSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& instancerPrimPath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _instancerPrimPath(instancerPrimPath),
    _selectionIndex(selectionIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    _CollectInstancingPaths(instancerPrimPath, SelectionHighlightsCollectionDirection::Bidirectional, _instancerPaths, _prototypePaths);

    // std::cout << "_primPathsToConserve" << std::endl;
    // for (const auto& path : _primPathsToConserve) {
    //     std::cout << "--- " + path.GetString() << std::endl;
    // }
    // std::cout << std::endl;
}

void PointInstancerWireframeHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
}

void PointInstancerWireframeHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries prunedEntries;
    for (const auto& entry : entries) {
        if (_IsRelevantPath(entry.primPath)) {
            prunedEntries.push_back(entry);
            _instancerPaths.erase(entry.primPath);
            _prototypePaths.erase(entry.primPath);
        }
    }
    _SendPrimsRemoved(prunedEntries);
}

void PointInstancerWireframeHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries prunedEntries;
    for (const auto& entry : entries) {
        if (_IsRelevantPath(entry.primPath)) {
            prunedEntries.push_back(entry);

            // TODO ; handle instancing changes
        }
    }
    _SendPrimsDirtied(prunedEntries);
}

bool PointInstancerWireframeHighlightSceneIndex::_IsInstancerPath(const PXR_NS::SdfPath& primPath) const
{
    return _instancerPaths.find(primPath) != _instancerPaths.end();
    // for (const auto& instancerPath : _instancerPaths) {
    //     // Use direct path rather than prefix?
    //     if (primPath.HasPrefix(instancerPath)) {
    //         return true;
    //     }
    // }
    // return false;
}

bool PointInstancerWireframeHighlightSceneIndex::_IsPrototypePath(const PXR_NS::SdfPath& primPath) const
{
    for (const auto& prototypePath : _prototypePaths) {
        if (primPath.HasPrefix(prototypePath)) {
            return true;
        }
    }
    return false;
}

bool PointInstancerWireframeHighlightSceneIndex::_IsRelevantPath(const PXR_NS::SdfPath& primPath) const
{
    return _IsInstancerPath(primPath) || _IsPrototypePath(primPath);
}

void
PointInstancerWireframeHighlightSceneIndex::_CollectInstancingPaths(const PXR_NS::SdfPath& primPath, SelectionHighlightsCollectionDirection direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths will be processed as part
    // of the children traversal later down this method.
    if (_IsPrototypeSubPrim(prim, primPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            _CollectInstancingPaths(protoRootPath, direction, outInstancerPaths, outPrototypePaths);
        }
        return;
    }
    
    if (_IsPrototype(prim)) {
        if (outPrototypePaths.find(primPath) != outPrototypePaths.end()) {
            return;
        }
        outPrototypePaths.insert(primPath);
    } else {
        if (outInstancerPaths.find(primPath) != outInstancerPaths.end()) {
            return;
        }
        outInstancerPaths.insert(primPath);
    }

    // Traverse the children of this prim to find the affected child prims, and process their instancing-related
    // paths so we can create selection highlight mirrors for these prims as well.
    SdfPathVector affectedPrototypePaths;
    SdfPathVector affectedInstancedByPaths;
    auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (prim.primType == HdPrimTypeTokens->instancer || prim.primType == HdPrimTypeTokens->mesh) {
            if (direction & SelectionHighlightsCollectionDirection::Prototypes) {
                auto prototypePaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection::Prototypes);
                affectedPrototypePaths.insert(affectedPrototypePaths.end(), prototypePaths.begin(), prototypePaths.end());
            }
            if (direction & SelectionHighlightsCollectionDirection::InstancedBy) {
                auto instancedByPaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection::InstancedBy);
                affectedInstancedByPaths.insert(affectedInstancedByPaths.end(), instancedByPaths.begin(), instancedByPaths.end());
            }
            // We hit an instancing-related prim, don't process its children (nested instancing will be processed through the instancing-related paths).
            return false;
        }
        return true;
    };
    _ForEachPrimInHierarchy(primPath, operation);

    for (const auto& affectedPrototypePath : affectedPrototypePaths) {
        _CollectInstancingPaths(affectedPrototypePath, SelectionHighlightsCollectionDirection::Prototypes, outInstancerPaths, outPrototypePaths);
    }
    for (const auto& affectedInstancedByPath : affectedInstancedByPaths) {
        _CollectInstancingPaths(affectedInstancedByPath, SelectionHighlightsCollectionDirection::InstancedBy, outInstancerPaths, outPrototypePaths);
    }
}

void
PointInstancerWireframeHighlightSceneIndex::_ForEachPrimInHierarchy(
    const PXR_NS::SdfPath& hierarchyRoot, 
    const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation
) const
{
    HdSceneIndexPrimView hierarchyView(GetInputSceneIndex(), hierarchyRoot);
    for (auto itPrim = hierarchyView.begin(); itPrim != hierarchyView.end(); ++itPrim) {
        const SdfPath& currPath = *itPrim;

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);

        // If the current prim is not part of the same hierarchy we are traversing, skip it and its descendents.
        VtArray<SdfPath> primRoots = _GetHierarchyRoots(currPrim);
        bool sharesHierarchy = std::find_if(primRoots.begin(), primRoots.end(), [hierarchyRoot](const auto& primRoot) -> bool {
            return hierarchyRoot.HasPrefix(primRoot);
        }) != primRoots.end();
        if (!sharesHierarchy) {
            itPrim.SkipDescendants();
            continue;
        }

        if (!operation(currPath, currPrim)) {
            itPrim.SkipDescendants();
            continue;
        }
    }
}

}
