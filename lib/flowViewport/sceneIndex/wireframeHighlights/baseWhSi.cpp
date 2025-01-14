#include "baseWhSi.h"

#include <flowViewport/fvpUtils.h>
#include <iostream>

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
//Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
TF_DEFINE_PRIVATE_TOKENS(
     _primVarsTokens,
 
     (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
 );

const HdRetainedContainerDataSourceHandle refinedWireDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWire, TfToken(), TfToken() })));

const HdDataSourceLocator reprSelectorLocator(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdLegacyDisplayStyleSchemaTokens->reprSelector);

const HdDataSourceLocator primvarsOverrideWireframeColorLocator(
        HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor));

// Returns all paths related to instancing for this prim; this is analogous to getting the edges
// connected to the given vertex (in this case a prim) of an instancing graph.
SdfPathVector _GetInstancingRelatedPaths(const HdSceneIndexPrim& prim, Fvp::SelectionHighlightsCollectionDirection2 direction)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    
    SdfPathVector instancingRelatedPaths;

    if ((direction & Fvp::SelectionHighlightsCollectionDirection2::Prototypes2)
        && instancerTopology.IsDefined()) {
        auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
        for (const auto& protoPath : protoPaths) {
            instancingRelatedPaths.push_back(protoPath);
        }
    }

    if ((direction & Fvp::SelectionHighlightsCollectionDirection2::InstancedBy2)
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

//We want to set the displayStyle of the selected prim to refinedWireOnSurf only if the displayStyle of the prim is refined (meaning shaded)
HdContainerDataSourceHandle SetWireframeRepr(const HdContainerDataSourceHandle& dataSource, const GfVec4f& color)
{
    //Always edit its override wireframe color
    auto edited = HdContainerDataSourceEditor(dataSource);
    edited.Set(primvarsOverrideWireframeColorLocator,
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(VtVec4fArray{color}),
                            HdPrimvarSchemaTokens->constant,
                            HdPrimvarSchemaTokens->color));
    
    //Is the prim in refined displayStyle (meaning shaded) ?
    if (HdLegacyDisplayStyleSchema styleSchema =
            HdLegacyDisplayStyleSchema::GetFromParent(dataSource)) {

        if (HdTokenArrayDataSourceHandle ds =
                styleSchema.GetReprSelector()) {
            VtArray<TfToken> ar = ds->GetTypedValue(0.0f);
            TfToken refinedToken = ar[0];
            if(HdReprTokens->refined == refinedToken){
                //Is in refined display style, apply the wire on top of shaded reprselector
                return HdOverlayContainerDataSource::New({ edited.Finish(), refinedWireDisplayStyleDataSource});
            }
        }else{
            //No reprSelector found, assume it's in the Collection that we have set HdReprTokens->refined
            return HdOverlayContainerDataSource::New({ edited.Finish(), refinedWireDisplayStyleDataSource});
        }
    }

    //For the other case, we are only updating the wireframe color assuming we are already drawing lines
    return edited.Finish();
}

BaseWhSi::BaseWhSi(
    const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
    const PXR_NS::SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _highlightHierarchyPrefix(highlightHierarchyPrefix),
    _wireframeColorInterface(wireframeColorInterface)
{

}

HdSceneIndexPrim BaseWhSi::GetPrim(const PXR_NS::SdfPath &primPath) const
{
    if (primPath.HasPrefix(_highlightHierarchyPrefix) && !_selectionPaths.empty()) {
        auto it = _selectionPaths.upper_bound(primPath);
        bool isHighlightPrim = it != _selectionPaths.begin() && primPath.HasPrefix(*std::prev(it)) && primPath != *std::prev(it);
        if (isHighlightPrim) {
            auto selectionPath = primPath;
            while (_selectionPaths.find(selectionPath) == _selectionPaths.end()) {
                selectionPath = selectionPath.GetParentPath();
            }
            return GetHighlightPrim(selectionPath, primPath);
        }
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector BaseWhSi::GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const
{
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    if (primPath == SdfPath::AbsoluteRootPath()) {
        childPaths.emplace_back(_highlightHierarchyPrefix);
        return childPaths;
    }
    if (primPath.HasPrefix(_highlightHierarchyPrefix) && !_selectionPaths.empty()) {
        // To return the paths leading up to and including selection paths
        auto it = _selectionPaths.upper_bound(primPath);
        while (it != _selectionPaths.end() && it->HasPrefix(primPath)) {
            auto childPath = it->GetPrefixes()[primPath.GetPathElementCount()];
            if (std::find(childPaths.begin(), childPaths.end(), childPath) == childPaths.end()) {
                childPaths.emplace_back(childPath);
            }
            it++;
        }
        if (!childPaths.empty()) {
            return childPaths;
        }

        // To return the highlight sub-hierarchy paths
        auto selectionPath = primPath;
        while (_selectionPaths.find(selectionPath) == _selectionPaths.end() && selectionPath.HasPrefix(_highlightHierarchyPrefix)) {
            selectionPath = selectionPath.GetParentPath();
        }
        if (_selectionPaths.find(selectionPath) != _selectionPaths.end()) {
            return GetHighlightChildPrimPaths(selectionPath, primPath);
        }
    }
    return childPaths;
}

void BaseWhSi::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    _SendPrimsAdded(entries);
    HdSceneIndexObserver::AddedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
        }
    }
    ProcessAddedPrims(sender, filteredEntries);
}

void BaseWhSi::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
    HdSceneIndexObserver::RemovedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
        }
    }
    ProcessRemovedPrims(sender, filteredEntries);
}

void BaseWhSi::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    _SendPrimsDirtied(entries);
    HdSceneIndexObserver::DirtiedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (!IsExcludedPath(entry.primPath)) {
            filteredEntries.emplace_back(entry);
        }
    }
    ProcessDirtiedPrims(sender, filteredEntries);
}

void BaseWhSi::AddExcludedPath(const PXR_NS::SdfPath& path)
{
    _excludedPaths.emplace(path);
}

bool BaseWhSi::IsExcludedPath(const PXR_NS::SdfPath& path) const
{
    auto itExcludedPath = _excludedPaths.upper_bound(path);
    if (itExcludedPath != _excludedPaths.begin() && path.HasPrefix(*std::prev(itExcludedPath))) {
        return true;
    }
    return false;
}

SdfPath BaseWhSi::SelectionPathFromKey(const SelectionKey& selectionKey) const
{
    return selectionKey.first.ReplacePrefix(SdfPath::AbsoluteRootPath(), _highlightHierarchyPrefix).AppendElementString("Highlight_" + selectionKey.second);
}

SelectionKey BaseWhSi::SelectionKeyFromPath(const SdfPath& selectionPath) const
{
    auto selectedPrimPath = selectionPath.GetParentPath().ReplacePrefix(_highlightHierarchyPrefix, SdfPath::AbsoluteRootPath());
    auto highlightId = selectionPath.GetElementString().substr(std::string("Highlight_").size());
    return SelectionKey(selectedPrimPath, highlightId);
}

SdfPath BaseWhSi::RegisterSelection(const SelectionKey& selectionKey)
{
    _primPathsToSelections[selectionKey.first].emplace(selectionKey);
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.emplace(selectionPath);
    return selectionPath;
}

SdfPath BaseWhSi::UnregisterSelection(const SelectionKey& selectionKey)
{
    _primPathsToSelections[selectionKey.first].erase(selectionKey);
    if (_primPathsToSelections[selectionKey.first].empty()) {
        _primPathsToSelections.erase(selectionKey.first);
    }
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.erase(selectionPath);
    return selectionPath;
}

void
BaseWhSi::ForEachPrimInHierarchy(
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

void
BaseWhSi::CollectInstancingPaths(const PXR_NS::SdfPath& primPath, SelectionHighlightsCollectionDirection2 direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths will be processed as part
    // of the children traversal later down this method.
    if (_IsPrototypeSubPrim(prim, primPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            CollectInstancingPaths(protoRootPath, direction, outInstancerPaths, outPrototypePaths);
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
            if (direction & SelectionHighlightsCollectionDirection2::Prototypes2) {
                auto prototypePaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection2::Prototypes2);
                affectedPrototypePaths.insert(affectedPrototypePaths.end(), prototypePaths.begin(), prototypePaths.end());
            }
            if (direction & SelectionHighlightsCollectionDirection2::InstancedBy2) {
                auto instancedByPaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection2::InstancedBy2);
                affectedInstancedByPaths.insert(affectedInstancedByPaths.end(), instancedByPaths.begin(), instancedByPaths.end());
            }
            // We hit an instancing-related prim, don't process its children (nested instancing will be processed through the instancing-related paths).
            return false;
        }
        return true;
    };
    ForEachPrimInHierarchy(primPath, operation);

    for (const auto& affectedPrototypePath : affectedPrototypePaths) {
        CollectInstancingPaths(affectedPrototypePath, SelectionHighlightsCollectionDirection2::Prototypes2, outInstancerPaths, outPrototypePaths);
    }
    for (const auto& affectedInstancedByPath : affectedInstancedByPaths) {
        CollectInstancingPaths(affectedInstancedByPath, SelectionHighlightsCollectionDirection2::InstancedBy2, outInstancerPaths, outPrototypePaths);
    }
}

}
