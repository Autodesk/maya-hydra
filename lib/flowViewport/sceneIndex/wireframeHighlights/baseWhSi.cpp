#include "baseWhSi.h"

#include <flowViewport/fvpUtils.h>

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
        while (_selectionPaths.find(selectionPath) == _selectionPaths.end()) {
            selectionPath = selectionPath.GetParentPath();
        }
        return GetHighlightChildPrimPaths(selectionPath, primPath);
    }
    return childPaths;
}

void BaseWhSi::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    _SendPrimsAdded(entries);
    ProcessAddedPrims(sender, entries);
}

void BaseWhSi::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
    ProcessRemovedPrims(sender, entries);
}

void BaseWhSi::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    _SendPrimsDirtied(entries);
    ProcessDirtiedPrims(sender, entries);
}

SdfPath BaseWhSi::SelectionPathFromKey(const SelectionKey& selectionKey) {
    // TODO : Handle highlight hierarchy prefix
    return selectionKey.first.AppendElementString("Selection_" + std::to_string(selectionKey.second));
}

SelectionKey BaseWhSi::SelectionKeyFromPath(const SdfPath& selectionPath)
{
    // TODO : Handle highlight hierarchy prefix
    auto selectedPrimPath = selectionPath.GetParentPath();
    auto selectionId = std::stoul(selectionPath.GetElementString().substr(std::string("Selection_").size()));
    return SelectionKey(selectedPrimPath, selectionId);
}

void BaseWhSi::RegisterSelection(const SelectionKey& selectionKey)
{
    // TODO : Handle highlight hierarchy prefix
    _primPathsToSelections[selectionKey.first].emplace(selectionKey);
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.emplace(selectionPath);
}

void BaseWhSi::UnregisterSelection(const SelectionKey& selectionKey)
{
    // TODO : Handle highlight hierarchy prefix
    _primPathsToSelections[selectionKey.first].erase(selectionKey);
    if (_primPathsToSelections[selectionKey.first].empty()) {
        _primPathsToSelections.erase(selectionKey.first);
    }
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.erase(selectionPath);
}

}
