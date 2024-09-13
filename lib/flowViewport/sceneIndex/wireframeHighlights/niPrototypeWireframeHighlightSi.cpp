#include "niPrototypeWireframeHighlightSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
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
            
            HdContainerDataSourceEditor dsEditor(prim.dataSource);
            
            dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());
            dsEditor.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(GetInputSceneIndex()->GetPrim(_prototypeSubprimPath).dataSource);
            HdSelectionSchema activeSelection = selectionsSchema.GetElement(_selectionIndex);
            HdInstanceIndicesSchema instanceIndices = activeSelection.GetNestedInstanceIndices().GetElement(0);
            auto instanceIndex = instanceIndices.GetInstanceIndices()->GetTypedValue(0).front();

            HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
            auto instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0).front();
            // Do it on every prim or only proto root?
            HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
            HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(instancerPrim.dataSource);
            auto instanceTransformsSchema = primvarsSchema.GetPrimvar(HdInstancerTokens->instanceTransforms);
            auto instanceTransforms = HdTypedSampledDataSource<VtArray<GfMatrix4d>>::Cast(instanceTransformsSchema.GetPrimvarValue());
            auto instanceXform = instanceTransforms->GetTypedValue(0)[instanceIndex];

            auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);

            dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(instanceXform * prototypeXform));

            prim.dataSource = dsEditor.Finish();

            std::cout << "ConvertHydraToFvpSelection" << std::endl;
            Fvp::PrimSelection prototypeSelection = ConvertHydraToFvpSelection(_prototypeSubprimPath, activeSelection);
            std::cout << "MakeWireframe" << std::endl;
            prim.dataSource = MakeWireframe(prim.dataSource, _wireframeColorInterface->getWireframeColor(prototypeSelection));// primPath? made relative?
            std::cout << "Done" << std::endl;
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
