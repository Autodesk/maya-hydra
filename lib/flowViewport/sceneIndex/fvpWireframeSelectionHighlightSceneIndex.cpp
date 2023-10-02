//
// Copyright 2023 Autodesk
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

#include "flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.h"

#include "flowViewport/debugCodes.h"

#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/selectionsSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
const HdRetainedContainerDataSourceHandle sSelectedDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWireOnSurf, HdReprTokens->wireOnSurf, TfToken() })));

const HdRetainedContainerDataSourceHandle sUnselectedDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refined, HdReprTokens->refined, TfToken() })));

const HdDataSourceLocator reprSelectorLocator(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdLegacyDisplayStyleSchemaTokens->reprSelector);
}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr
WireframeSelectionHighlightSceneIndex::New(HdSceneIndexBaseRefPtr const &inputSceneIndex, const SdfPath& sceneRoot)
{
    return TfCreateRefPtr(new WireframeSelectionHighlightSceneIndex(inputSceneIndex, sceneRoot));
}

WireframeSelectionHighlightSceneIndex::
WireframeSelectionHighlightSceneIndex(HdSceneIndexBaseRefPtr const &inputSceneIndex, const SdfPath& sceneRoot)
  : PassThroughSelectionInterfaceSceneIndexBase(inputSceneIndex)
  , _sceneRoot(sceneRoot)
{}

HdSceneIndexPrim
WireframeSelectionHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::GetPrim(%s) called.\n", primPath.GetText());

    auto prim = _GetInputSceneIndex()->GetPrim(primPath);

    // If this isn't one of our prims, we're not responsible for providing a
    // selection highlight for it.  Selection highlighting is done only on
    // meshes.  HYDRA-569: this means that HdsiImplicitSurfaceSceneIndex must
    // be used before this scene index to convert implicit surfaces (e.g. USD
    // cube / cone / sphere / capsule primitive types) to meshes.  
    if (primPath.HasPrefix(_sceneRoot) && 
        prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = HdOverlayContainerDataSource::New(
            { prim.dataSource, HasFullySelectedAncestorInclusive(primPath) ? 
                sSelectedDisplayStyleDataSource : 
                sUnselectedDisplayStyleDataSource });
    }
    return prim;

}

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
WireframeSelectionHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsAdded() called.\n");

    _SendPrimsAdded(entries);
}

void
WireframeSelectionHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsDirtied() called.\n");

    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // If the dirtied prim isn't one of ours, we're not responsible for
        // providing a selection highlight for it.
        if (entry.primPath.HasPrefix(_sceneRoot) && 
            entry.dirtyLocators.Contains(
                HdSelectionsSchema::GetDefaultLocator())) {
            TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
                .Msg("    %s selections locator dirty.\n", entry.primPath.GetText());
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            dirtySelectionHighlightRecursive(entry.primPath, &highlightEntries);
        }
    }

    if (!highlightEntries.empty()) {
        // Append all incoming dirty entries.
        highlightEntries.reserve(highlightEntries.size()+entries.size());
        highlightEntries.insert(
            highlightEntries.end(), entries.begin(), entries.end());
        _SendPrimsDirtied(highlightEntries);
    }
    else {
        _SendPrimsDirtied(entries);
    }
}

void
WireframeSelectionHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsRemoved() called.\n");

    _SendPrimsRemoved(entries);
}

void WireframeSelectionHighlightSceneIndex::dirtySelectionHighlightRecursive(
    const SdfPath&                            primPath, 
    HdSceneIndexObserver::DirtiedPrimEntries* highlightEntries
)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("    marking %s wireframe highlight locator dirty.\n", primPath.GetText());
    highlightEntries->emplace_back(primPath, HdDataSourceLocatorSet(reprSelectorLocator));
    for (const auto& childPath : GetChildPrimPaths(primPath)) {
        dirtySelectionHighlightRecursive(childPath, highlightEntries);
    }
}

}
