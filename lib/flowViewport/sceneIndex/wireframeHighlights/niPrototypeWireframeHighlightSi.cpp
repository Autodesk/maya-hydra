#include "niPrototypeWireframeHighlightSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <algorithm>
#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// // Returns all paths related to instancing for this prim; this is analogous to getting the edges
// // connected to the given vertex (in this case a prim) of an instancing graph.
// SdfPathVector _GetInstancingRelatedPaths(const HdSceneIndexPrim& prim, Fvp::SelectionHighlightsCollectionDirection direction)
// {
//     HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
//     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    
//     SdfPathVector instancingRelatedPaths;

//     if ((direction & Fvp::SelectionHighlightsCollectionDirection::Prototypes)
//         && instancerTopology.IsDefined()) {
//         auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
//         for (const auto& protoPath : protoPaths) {
//             instancingRelatedPaths.push_back(protoPath);
//         }
//     }

//     if ((direction & Fvp::SelectionHighlightsCollectionDirection::InstancedBy)
//         && instancedBy.IsDefined()) {
//         auto instancerPaths = instancedBy.GetPaths()->GetTypedValue(0);
//         for (const auto& instancerPath : instancerPaths) {
//             instancingRelatedPaths.push_back(instancerPath);
//         }

//         // Having a prototype root is not a hard requirement (a single prim being instanced
//         // does not need to specify itself as its own prototype root).
//         if (instancedBy.GetPrototypeRoots()) {
//             auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
//             for (const auto& protoRootPath : protoRootPaths) {
//                 instancingRelatedPaths.push_back(protoRootPath);
//             }
//         }
//     }

//     return instancingRelatedPaths;
// }

// bool _IsPrototype(const HdSceneIndexPrim& prim)
// {
//     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
//     return instancedBy.IsDefined();
// }

// bool _IsPrototypeSubPrim(const HdSceneIndexPrim& prim, const SdfPath& primPath)
// {
//     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
//     if (!instancedBy.IsDefined()) {
//         return false;
//     }
//     if (!instancedBy.GetPrototypeRoots()) {
//         return false;
//     }
//     auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
//     for (const auto& protoRootPath : protoRootPaths) {
//         if (protoRootPath == primPath) {
//             return false;
//         }
//     }
//     return true;
// }

// // We consider prototypes that have child prims to be different hierarchies,
// // separate from each other and from the "root" hierarchy.
// VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
// {
//     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
//     return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
//         ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
//         : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
// }

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr NiPrototypeWireframeHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& prototypeSubprimPath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new NiPrototypeWireframeHighlightSceneIndex(inputSceneIndex, prototypeSubprimPath, selectionIndex, wireframeColorInterface));
}

SdfPath NiPrototypeWireframeHighlightSceneIndex::_PrototypeSubprimNamePath() const
{
    return SdfPath::AbsoluteRootPath().AppendChild(_prototypeSubprimPath.GetNameToken());
}

HdSceneIndexPrim NiPrototypeWireframeHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    if (primPath.HasPrefix(_PrototypeSubprimNamePath())) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath.ReplacePrefix(_PrototypeSubprimNamePath(), _prototypeSubprimPath));
        if (prim.primType == HdPrimTypeTokens->mesh) {
            prim.dataSource = MakeWireframe(prim.dataSource, _wireframeColorInterface->getWireframeColor(_prototypeSubprimPath));// primPath? made relative?
            HdContainerDataSourceEditor dsEditor(prim.dataSource);
            dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());
            prim.dataSource = dsEditor.Finish();
        }
        return prim;
    }
    return {};
};

SdfPathVector NiPrototypeWireframeHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    if (primPath == SdfPath::AbsoluteRootPath()) {
        return {_PrototypeSubprimNamePath()};
    } else if (primPath.HasPrefix(_PrototypeSubprimNamePath())) {
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(_PrototypeSubprimNamePath(), _prototypeSubprimPath));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(_prototypeSubprimPath, _PrototypeSubprimNamePath());
        }
        return childPaths;
    }
    return {};
}

NiPrototypeWireframeHighlightSceneIndex::NiPrototypeWireframeHighlightSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& prototypeSubprimPath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _prototypeSubprimPath(prototypeSubprimPath),
    _selectionIndex(selectionIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    //_CollectInstancingPaths(prototypeSubprimPath, SelectionHighlightsCollectionDirection::Bidirectional, _instancerPaths, _prototypePaths);

    // std::cout << "_primPathsToConserve" << std::endl;
    // for (const auto& path : _primPathsToConserve) {
    //     std::cout << "--- " + path.GetString() << std::endl;
    // }
    // std::cout << std::endl;
}

void NiPrototypeWireframeHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
}

void NiPrototypeWireframeHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath.HasPrefix(_prototypeSubprimPath)) {
            _SendPrimsRemoved({HdSceneIndexObserver::RemovedPrimEntry(entry.primPath.ReplacePrefix(_prototypeSubprimPath, _PrototypeSubprimNamePath()))});
        }
    }
}

void NiPrototypeWireframeHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath.HasPrefix(_prototypeSubprimPath)) {
            _SendPrimsDirtied({HdSceneIndexObserver::DirtiedPrimEntry(entry.primPath.ReplacePrefix(_prototypeSubprimPath, _PrototypeSubprimNamePath()), entry.dirtyLocators)});
        }
    }
}


}
