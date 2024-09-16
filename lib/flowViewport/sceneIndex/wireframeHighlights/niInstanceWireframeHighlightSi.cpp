#include "niInstanceWireframeHighlightSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <algorithm>
#include <iostream>
#include <stack>

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

Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
    Fvp::PrimSelection primSelection;
    primSelection.primPath = primPath;

    HdInstanceIndicesVectorSchema nestedInstanceIndicesSchema = selectionSchema.GetNestedInstanceIndices();
    std::cout << "nestedInstanceIndicesSchema.GetNumElements() = " << nestedInstanceIndicesSchema.GetNumElements() << std::endl;
    for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
        std::cout << "iNestedInstanceIndices : " << iNestedInstanceIndices << std::endl;
        HdInstanceIndicesSchema instanceIndicesSchema = nestedInstanceIndicesSchema.GetElement(iNestedInstanceIndices);
        auto instanceIndices = instanceIndicesSchema.GetInstanceIndices()->GetTypedValue(0);
        primSelection.nestedInstanceIndices.push_back(
            {
                instanceIndicesSchema.GetInstancer()->GetTypedValue(0),
                instanceIndicesSchema.GetPrototypeIndex()->GetTypedValue(0),
                std::vector<int>(instanceIndices.begin(), instanceIndices.end())
            }
        );
    }

    return primSelection;
}

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr NiInstanceWireframeHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& prototypeSubprimPath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new NiInstanceWireframeHighlightSceneIndex(inputSceneIndex, prototypeSubprimPath, selectionIndex, wireframeColorInterface));
}

SdfPath NiInstanceWireframeHighlightSceneIndex::_PrototypeNamePath() const
{
    return SdfPath::AbsoluteRootPath().AppendChild(_prototypePath.GetNameToken());
}

HdSceneIndexPrim NiInstanceWireframeHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    if (primPath.HasPrefix(_PrototypeNamePath())) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath.ReplacePrefix(_PrototypeNamePath(), _prototypePath));
        if (prim.primType == HdPrimTypeTokens->mesh) {
            
            HdContainerDataSourceEditor dsEditor(prim.dataSource);
            
            dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());
            dsEditor.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());

            HdSceneIndexPrim instancePrim = GetInputSceneIndex()->GetPrim(_instancePath);

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(instancePrim.dataSource);
            HdSelectionSchema activeSelection = selectionsSchema.GetElement(_selectionIndex);

            auto instanceXform = HdXformSchema::GetFromParent(instancePrim.dataSource).GetMatrix()->GetTypedValue(0);
            auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);

            dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(instanceXform * prototypeXform));

            prim.dataSource = dsEditor.Finish();

            std::cout << "ConvertHydraToFvpSelection" << std::endl;
            Fvp::PrimSelection instanceSelection = ConvertHydraToFvpSelection(_instancePath, activeSelection);
            std::cout << "MakeWireframe" << std::endl;
            prim.dataSource = MakeWireframe(prim.dataSource, _wireframeColorInterface->getWireframeColor(instanceSelection));// primPath? made relative?
            std::cout << "Done" << std::endl;
        }
        return prim;
    }
    return {};
};

SdfPathVector NiInstanceWireframeHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    if (primPath == SdfPath::AbsoluteRootPath()) {
        return {_PrototypeNamePath()};
    } else if (primPath.HasPrefix(_PrototypeNamePath())) {
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(_PrototypeNamePath(), _prototypePath));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(_prototypePath, _PrototypeNamePath());
        }
        return childPaths;
    }
    return {};
}

NiInstanceWireframeHighlightSceneIndex::NiInstanceWireframeHighlightSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& instancePath,
    const size_t selectionIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _instancePath(instancePath),
    _selectionIndex(selectionIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    HdSceneIndexPrim instancePrim = GetInputSceneIndex()->GetPrim(instancePath);
    HdInstanceSchema instanceSchema = HdInstanceSchema::GetFromParent(instancePrim.dataSource);

    auto instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);
    HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    _prototypePath = instancerTopologySchema.GetPrototypes()->GetTypedValue(0)[instanceSchema.GetPrototypeIndex()->GetTypedValue(0)];

    //_CollectInstancingPaths(prototypeSubprimPath, SelectionHighlightsCollectionDirection::Bidirectional, _instancerPaths, _prototypePaths);

    // std::cout << "_primPathsToConserve" << std::endl;
    // for (const auto& path : _primPathsToConserve) {
    //     std::cout << "--- " + path.GetString() << std::endl;
    // }
    // std::cout << std::endl;
}

void NiInstanceWireframeHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
}

void NiInstanceWireframeHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath.HasPrefix(_prototypePath)) {
            _SendPrimsRemoved({HdSceneIndexObserver::RemovedPrimEntry(entry.primPath.ReplacePrefix(_prototypePath, _PrototypeNamePath()))});
        }
    }
}

void NiInstanceWireframeHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        if (entry.primPath.HasPrefix(_prototypePath)) {
            _SendPrimsDirtied({HdSceneIndexObserver::DirtiedPrimEntry(entry.primPath.ReplacePrefix(_prototypePath, _PrototypeNamePath()), entry.dirtyLocators)});
        }
        // To propagate wireframe color change to prototype
        if (entry.primPath.HasPrefix(_instancePath)) {
            HdSceneIndexObserver::DirtiedPrimEntries propagatedEntries;
            std::stack<SdfPath> pathsToDirty({_PrototypeNamePath()});
            while (!pathsToDirty.empty()) {
                auto currPathToDirty = pathsToDirty.top();
                pathsToDirty.pop();

                propagatedEntries.push_back({currPathToDirty, entry.dirtyLocators});

                for (const auto& childPath : GetChildPrimPaths(currPathToDirty)) {
                    pathsToDirty.push(childPath);
                }
            }
            _SendPrimsDirtied(propagatedEntries);
        }
    }
}


}
