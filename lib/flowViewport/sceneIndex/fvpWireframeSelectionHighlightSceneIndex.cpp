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
#include "flowViewport/selection/fvpSelection.h"
#include "flowViewport/fvpUtils.h"

#include "flowViewport/debugCodes.h"

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <stack>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
//Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
TF_DEFINE_PRIVATE_TOKENS(
     _primVarsTokens,
 
     (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
 );

const HdRetainedContainerDataSourceHandle sRefinedWireDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWire, TfToken(), TfToken() })));

const HdRetainedContainerDataSourceHandle sRefinedWireOnSurfaceDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWireOnSurf, TfToken(), TfToken() })));

const HdDataSourceLocator reprSelectorLocator(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdLegacyDisplayStyleSchemaTokens->reprSelector);

const HdDataSourceLocator primvarsOverrideWireframeColorLocator(
        HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor));

const std::string selectionHighlightMirrorTag = "_SelectionHighlight";

SdfPath _GetSelectionHighlightMirrorPathFromOriginal(const SdfPath& originalPath)
{
    return originalPath.ReplaceName(TfToken(originalPath.GetName() + selectionHighlightMirrorTag));
}

SdfPath _GetOriginalPathFromSelectionHighlightMirror(const SdfPath& mirrorPath)
{
    const std::string primName = mirrorPath.GetName();
    return mirrorPath.ReplaceName(TfToken(primName.substr(0, primName.size() - selectionHighlightMirrorTag.size())));
}

// Computes the mask to use for an instancer's selection highlight mirror
// based on the instancer's topology and its selections. This allows
// highlighting only specific instances in the case of instance selections.
VtBoolArray _GetSelectionHighlightMask(const HdInstancerTopologySchema& originalInstancerTopology, const HdSelectionsSchema& selections) 
{
    // Schema getters were made const in USD 24.05 (specifically Hydra API version 66).
    // We work around this for previous versions by const casting.
    VtBoolArray originalMask = 
#if HD_API_VERSION < 66
    const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetMask()->GetTypedValue(0);
#else
    originalInstancerTopology.GetMask()->GetTypedValue(0);
#endif

    if (!selections.IsDefined()) {
        return originalMask;
    }

    size_t nbInstances = 0;
    auto instanceIndices = 
#if HD_API_VERSION < 66
    const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetInstanceIndices();
#else
    originalInstancerTopology.GetInstanceIndices();
#endif
    for (size_t iInstanceIndex = 0; iInstanceIndex < instanceIndices.GetNumElements(); iInstanceIndex++) {
        auto protoInstances = instanceIndices.GetElement(iInstanceIndex)->GetTypedValue(0);
        nbInstances += protoInstances.size();
    }
    TF_AXIOM(originalMask.empty() || originalMask.size() == nbInstances);
    VtBoolArray selectionHighlightMask(nbInstances, false);

    for (size_t iSelection = 0; iSelection < selections.GetNumElements(); iSelection++) {
        HdSelectionSchema selection = selections.GetElement(iSelection);
        // Instancer is expected to be marked "fully selected" even if only certain instances are selected,
        // based on USD's _AddToSelection function in selectionSceneIndexObserver.cpp :
        // https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/imaging/hdx/selectionSceneIndexObserver.cpp#L212-L251
        if (!selection.GetFullySelected() || !selection.GetFullySelected()->GetTypedValue(0)) {
            continue;
        }
        if (!selection.GetNestedInstanceIndices()) {
            // We have a selection that has no instances, which means the whole instancer is selected :
            // this overrides any instances selection.
            return originalMask;
        }
        HdInstanceIndicesVectorSchema nestedInstanceIndices = selection.GetNestedInstanceIndices();
        for (size_t iInstanceIndices = 0; iInstanceIndices < nestedInstanceIndices.GetNumElements(); iInstanceIndices++) {
            HdInstanceIndicesSchema instanceIndices = nestedInstanceIndices.GetElement(0);
            for (const auto& instanceIndex : instanceIndices.GetInstanceIndices()->GetTypedValue(0)) {
                selectionHighlightMask[instanceIndex] = originalMask.empty() ? true : originalMask[instanceIndex];
            }
        }
    }

    return selectionHighlightMask;
}

// Returns the overall data source for an instancer's selection highlight mirror.
// This replaces the mask data source and blocks the selections data source.
HdContainerDataSourceHandle _GetSelectionHighlightInstancerDataSource(const HdContainerDataSourceHandle& originalDataSource)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(originalDataSource);
    HdSelectionsSchema selections = HdSelectionsSchema::GetFromParent(originalDataSource);

    HdContainerDataSourceEditor editedDataSource = HdContainerDataSourceEditor(originalDataSource);

    if (selections.IsDefined()) {
        HdDataSourceLocator maskLocator = HdInstancerTopologySchema::GetDefaultLocator().Append(HdInstancerTopologySchemaTokens->mask);
        VtBoolArray selectionHighlightMask = _GetSelectionHighlightMask(instancerTopology, selections);
        auto selectionHighlightMaskDataSource = HdRetainedTypedSampledDataSource<VtBoolArray>::New(selectionHighlightMask);
        editedDataSource.Set(maskLocator, selectionHighlightMaskDataSource);
    }

    editedDataSource.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());

    return editedDataSource.Finish();
}

// Returns all paths related to instancing for this prim; this is analogous to getting the edges
// connected to the given vertex (in this case a prim) of an instancing graph.
SdfPathVector _GetInstancingRelatedPaths(const HdSceneIndexPrim& prim)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    
    SdfPathVector instancingRelatedPaths;

    if (instancerTopology.IsDefined()) {
        auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
        for (const auto& protoPath : protoPaths) {
            instancingRelatedPaths.push_back(protoPath);
        }
    }

    if (instancedBy.IsDefined()) {
        auto instancerPaths = instancedBy.GetPaths()->GetTypedValue(0);
        for (const auto& instancerPath : instancerPaths) {
            instancingRelatedPaths.push_back(instancerPath);
        }

        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            instancingRelatedPaths.push_back(protoRootPath);
        }
    }

    return instancingRelatedPaths;
}

VtArray<SdfPath> _GetPrototypeRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBy.IsDefined() || !instancedBy.GetPrototypeRoots()) {
        return {};
    }

    return instancedBy.GetPrototypeRoots()->GetTypedValue(0);
}

// This method is essentially "is not a prototype sub-prim".
bool _IsInstancingRoot(const HdSceneIndexPrim& prim, const SdfPath& primPath)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBy.IsDefined()) {
        return true;
    }
    if (!instancedBy.GetPrototypeRoots()) {
        return true;
    }
    auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
    for (const auto& protoRootPath : protoRootPaths) {
        if (protoRootPath == primPath) {
            return true;
        }
    }
    return false;
}

bool _IsPrototype(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined();
}

// Similar to USD's _RerootingSceneIndexPathDataSource :
// https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/usdImaging/usdImaging/rerootingSceneIndex.cpp#L35
class _SelectionHighlightRepathingPathDataSource : public HdPathDataSource
{
public:
    HD_DECLARE_DATASOURCE(_SelectionHighlightRepathingPathDataSource)

    _SelectionHighlightRepathingPathDataSource(
        HdPathDataSourceHandle const &inputDataSource,
        Fvp::WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
      : _inputDataSource(inputDataSource),
        _inputSceneIndex(inputSceneIndex)
    {
    }

    VtValue GetValue(const Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    bool GetContributingSampleTimesForInterval(
        const Time startTime,
        const Time endTime,
        std::vector<Time> * const outSampleTimes) override
    {
        if (!_inputDataSource) {
            return false;
        }

        return _inputDataSource->GetContributingSampleTimesForInterval(startTime, endTime, outSampleTimes);
    }

    SdfPath GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return SdfPath();
        }

        const SdfPath originalPath = _inputDataSource->GetTypedValue(shutterOffset);
        return _inputSceneIndex->GetSelectionHighlightPath(originalPath);
    }

private:
    HdPathDataSourceHandle const _inputDataSource;
    Fvp::WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

// Similar to USD's _RerootingSceneIndexPathArrayDataSource :
// https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/usdImaging/usdImaging/rerootingSceneIndex.cpp#L86
class _SelectionHighlightRepathingPathArrayDataSource : public HdPathArrayDataSource
{
public:
    HD_DECLARE_DATASOURCE(_SelectionHighlightRepathingPathArrayDataSource)

    _SelectionHighlightRepathingPathArrayDataSource(
        HdPathArrayDataSourceHandle const & inputDataSource,
        Fvp::WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
      : _inputDataSource(inputDataSource),
        _inputSceneIndex(inputSceneIndex)
    {
    }

    VtValue GetValue(const Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    bool GetContributingSampleTimesForInterval(
        const Time startTime,
        const Time endTime,
        std::vector<Time>*  const outSampleTimes) override
    {
        if (!_inputDataSource) {
            return false;
        }

        return _inputDataSource->GetContributingSampleTimesForInterval(startTime, endTime, outSampleTimes);
    }

    VtArray<SdfPath> GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return {};
        }

        VtArray<SdfPath> result = _inputDataSource->GetTypedValue(shutterOffset);
        for (auto& path : result) {
            path = _inputSceneIndex->GetSelectionHighlightPath(path);
        }

        return result;
    }

private:
    HdPathArrayDataSourceHandle const _inputDataSource;
    Fvp::WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

// Similar to USD's _RerootingSceneIndexContainerDataSource :
// https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/usdImaging/usdImaging/rerootingSceneIndex.cpp#L172
class _SelectionHighlightRepathingContainerDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_SelectionHighlightRepathingContainerDataSource)

    _SelectionHighlightRepathingContainerDataSource(
        HdContainerDataSourceHandle const &inputDataSource,
        Fvp::WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
      : _inputDataSource(inputDataSource),
        _inputSceneIndex(inputSceneIndex)
    {
    }

    TfTokenVector GetNames() override
    {
        return _inputDataSource ? _inputDataSource->GetNames() : TfTokenVector();
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (!_inputDataSource) {
            return nullptr;
        }

        HdDataSourceBaseHandle const childDataSource = _inputDataSource->Get(name);
        if (!childDataSource) {
            return nullptr;
        }

        if (auto childContainerDataSource = HdContainerDataSource::Cast(childDataSource)) {
            return New(childContainerDataSource, _inputSceneIndex);
        }

        if (auto childPathDataSource = HdTypedSampledDataSource<SdfPath>::Cast(childDataSource)) {
            return _SelectionHighlightRepathingPathDataSource::New(childPathDataSource, _inputSceneIndex);
        }

        if (auto childPathArrayDataSource = HdTypedSampledDataSource<VtArray<SdfPath>>::Cast(childDataSource)) {
            return _SelectionHighlightRepathingPathArrayDataSource::New(childPathArrayDataSource, _inputSceneIndex);
        }

        return childDataSource;
    }

private:
    HdContainerDataSourceHandle const _inputDataSource;
    Fvp::WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr
WireframeSelectionHighlightSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SelectionConstPtr&      selection,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
)
{
    return TfCreateRefPtr(new WireframeSelectionHighlightSceneIndex(inputSceneIndex, selection, wireframeColorInterface));
}

const HdDataSourceLocator& WireframeSelectionHighlightSceneIndex::ReprSelectorLocator()
{
    return reprSelectorLocator;
}

WireframeSelectionHighlightSceneIndex::
WireframeSelectionHighlightSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SelectionConstPtr&      selection,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _selection(selection)
    , _wireframeColorInterface(wireframeColorInterface)
{
    TF_AXIOM(_wireframeColorInterface);

    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (prim.primType == HdPrimTypeTokens->instancer) {
            _CreateInstancerHighlightsForInstancer(prim, primPath);
        }
        return true;
    };
    _ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

HdSceneIndexPrim
WireframeSelectionHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::GetPrim(%s) called.\n", primPath.GetText());

    if (_IsExcluded(primPath)) {
        return GetInputSceneIndex()->GetPrim(primPath);
    }
    
    // Is this prim part of a selection highlight mirror hierarchy?
    // If so, then we are dealing with a point instancer or instance selection.
    SdfPath selectionHighlightMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!selectionHighlightMirrorAncestor.IsEmpty()) {
        SdfPath originalPrimPath = primPath.ReplacePrefix(selectionHighlightMirrorAncestor, _GetOriginalPathFromSelectionHighlightMirror(selectionHighlightMirrorAncestor));
        HdSceneIndexPrim selectionHighlightPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);
        if (selectionHighlightPrim.dataSource) {
            // Redirects paths within data sources to their corresponding selection highlight mirror paths (when there is one)
            selectionHighlightPrim.dataSource = _SelectionHighlightRepathingContainerDataSource::New(selectionHighlightPrim.dataSource, this);

            // Use prim type-specific data source overrides
            if (selectionHighlightPrim.primType == HdPrimTypeTokens->instancer) {
                // Handles setting the mask for instance-specific highlighting
                selectionHighlightPrim.dataSource = _GetSelectionHighlightInstancerDataSource(selectionHighlightPrim.dataSource);
            }
            else if (selectionHighlightPrim.primType == HdPrimTypeTokens->mesh) {
                selectionHighlightPrim.dataSource = _HighlightSelectedPrim(selectionHighlightPrim.dataSource, originalPrimPath, sRefinedWireDisplayStyleDataSource);
            }
        }
        return selectionHighlightPrim;
    }
    
    // We are dealing with a mesh prototype selection, a regular selection, or no selection at all.
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        // Note : in the USD data model, the original prims that get propagated as prototypes have their original prim types erased.
        // Only the resulting propagated prototypes keep the original prim type.

        // We want to constrain the selected ancestor lookup to the propagated prototype only, if it is one.
        auto roots = _GetPrototypeRoots(prim);
        // If it is not a propagated prototype, consider the whole hierarchy.
        if (roots.empty()) {
            roots.push_back(SdfPath::AbsoluteRootPath());
        }
        for (const auto& root : roots) {
            if (_selection->HasFullySelectedAncestorInclusive(primPath, root)) {
                prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
                break;
            }
        }
    }
    return prim;
}

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    // When within a selection highlight mirror hierarchy, query the corresponding original prim's children
    SdfPath selectionHighlightMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!selectionHighlightMirrorAncestor.IsEmpty()) {
        SdfPath originalAncestor = _GetOriginalPathFromSelectionHighlightMirror(selectionHighlightMirrorAncestor);
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(selectionHighlightMirrorAncestor, originalAncestor));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(originalAncestor, selectionHighlightMirrorAncestor);
        }
        return childPaths;
    }

    // When outside a selection highlight mirror hierarchy, add each child's corresponding
    // selection highlight mirror, if there is one.
    auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector additionalChildPaths;
    for (const auto& childPath : childPaths) {
        SdfPath selectionHighlightPath = _GetSelectionHighlightMirrorPathFromOriginal(childPath);
        if (_selectionHighlightMirrorUseCounters.find(selectionHighlightPath) != _selectionHighlightMirrorUseCounters.end() 
            && _selectionHighlightMirrorUseCounters.at(selectionHighlightPath) > 0) {
            additionalChildPaths.push_back(selectionHighlightPath);
        }
    }
    childPaths.insert(childPaths.end(), additionalChildPaths.begin(), additionalChildPaths.end());
    return childPaths;
}

//We want to set the displayStyle of the selected prim to refinedWireOnSurf only if the displayStyle of the prim is refined (meaning shaded)
HdContainerDataSourceHandle WireframeSelectionHighlightSceneIndex::_HighlightSelectedPrim(const HdContainerDataSourceHandle& dataSource, const SdfPath& primPath, const HdContainerDataSourceHandle& highlightDataSource) const
{
    //Always edit its override wireframe color
    auto edited = HdContainerDataSourceEditor(dataSource);
    edited.Set(HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor),
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(VtVec4fArray{_wireframeColorInterface->getWireframeColor(primPath)}),
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
                return HdOverlayContainerDataSource::New({ edited.Finish(), highlightDataSource});
            }
        }else{
            //No reprSelector found, assume it's in the Collection that we have set HdReprTokens->refined
            return HdOverlayContainerDataSource::New({ edited.Finish(), highlightDataSource});
        }
    }

    //For the other case, we are only updating the wireframe color assuming we are already drawing lines
    return edited.Finish();
}


void
WireframeSelectionHighlightSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsAdded() called.\n");

    _SendPrimsAdded(entries);
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (prim.primType == HdPrimTypeTokens->instancer) {
            _CreateInstancerHighlightsForInstancer(prim, entry.primPath);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsDirtied() called.\n");

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedPrims;
    std::vector<SdfPath> instancerHighlightsToRebuild;
    std::vector<std::pair<SdfPath, SdfPath>> instancerHighlightUsersToAdd;
    std::vector<std::pair<SdfPath, SdfPath>> instancerHighlightUsersToRemove;

    for (const auto& entry : entries) {
        if (_IsExcluded(entry.primPath)) {
            // If the dirtied prim is excluded, don't provide selection
            // highlighting for it.
            continue;
        }

        // Propagate dirtiness to selection highlight prim
        auto selectionHighlightPath = GetSelectionHighlightPath(entry.primPath);
        if (selectionHighlightPath != entry.primPath) {
            dirtiedPrims.emplace_back(selectionHighlightPath, entry.dirtyLocators);
        }

        if (entry.dirtyLocators.Intersects(HdInstancerTopologySchema::GetDefaultLocator())
            && _selectionHighlightMirrorsByInstancer.find(entry.primPath) != _selectionHighlightMirrorsByInstancer.end()) {
            // An instancer with a selection highlight was changed; rebuild its selection highlight.
            // We do not need to check for instancedBy dirtying : if an instancedBy data source is dirtied,
            // then either a new instancer was added, which will be handled in _PrimsAdded, either an
            // existing instancer's instancerTopology data source was dirtied., which is handled here.
            instancerHighlightsToRebuild.push_back(entry.primPath);
        }
        
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
                .Msg("    %s selections locator dirty.\n", entry.primPath.GetText());
            
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);

            // Selection was changed on an instancer, so dirty its selection highlight mirror's instancerTopology mask
            // to update which instances are highlighted in the case of instance selection.
            if (prim.primType == HdPrimTypeTokens->instancer && selectionHighlightPath != entry.primPath) {
                dirtiedPrims.emplace_back(selectionHighlightPath, HdInstancerTopologySchema::GetDefaultLocator().Append(HdInstancerTopologySchemaTokens->mask));
            }
            
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            _DirtySelectionHighlightRecursive(entry.primPath, &dirtiedPrims);
            if (selectionHighlightPath != entry.primPath) {
                _DirtySelectionHighlightRecursive(selectionHighlightPath, &dirtiedPrims);
            }

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            bool isSelected = selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;

            // Update child instancer highlights for ancestor-based selection highlighting
            // (i.e. selecting one or more of an instancer's parents should highlight the instancer)
            auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                if (prim.primType == HdPrimTypeTokens->instancer) {
                    if (isSelected) {
                        instancerHighlightUsersToAdd.push_back({primPath,entry.primPath});
                    } else {
                        instancerHighlightUsersToRemove.push_back({primPath,entry.primPath});
                    }
                }
                return true;
            };
            if (!_IsPrototype(prim)) {
                _ForEachPrimInHierarchy(entry.primPath, operation);
            }
        }
    }

    if (!dirtiedPrims.empty()) {
        // Append all incoming dirty entries.
        dirtiedPrims.reserve(dirtiedPrims.size() + entries.size());
        dirtiedPrims.insert(dirtiedPrims.end(), entries.begin(), entries.end());
        _SendPrimsDirtied(dirtiedPrims);
    }
    else {
        _SendPrimsDirtied(entries);
    }

    for (const auto& instancerHighlightToRebuild : instancerHighlightsToRebuild) {
        _RebuildInstancerHighlight(instancerHighlightToRebuild);
    }
    for (const auto& instancerHighlightUserToAdd : instancerHighlightUsersToAdd) {
        _AddInstancerHighlightUser(instancerHighlightUserToAdd.first, instancerHighlightUserToAdd.second);
    }
    for (const auto& instancerHighlightUserToRemove : instancerHighlightUsersToRemove) {
        _RemoveInstancerHighlightUser(instancerHighlightUserToRemove.first, instancerHighlightUserToRemove.second);
    }
}

void
WireframeSelectionHighlightSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::_PrimsRemoved() called.\n");

    for (const auto& entry : entries) {
        // Collect and delete instancer highlights for all instancers rooted under the removed prim
        // (or if the removed prim is an instancer itself and has a highlight)
        SdfPathVector instancerHighlightsToDelete;
        for (const auto& selectionHighlightMirrorsForInstancer : _selectionHighlightMirrorsByInstancer) {
            SdfPath instancerPath = selectionHighlightMirrorsForInstancer.first;
            if (instancerPath.HasPrefix(entry.primPath)) {
                instancerHighlightsToDelete.push_back(instancerPath);
            }
        }
        for (const auto& instancerHighlightToDelete : instancerHighlightsToDelete) {
            _DeleteInstancerHighlight(instancerHighlightToDelete);
        }
    }
    _SendPrimsRemoved(entries);
}

void WireframeSelectionHighlightSceneIndex::_DirtySelectionHighlightRecursive(
    const SdfPath&                            primPath, 
    HdSceneIndexObserver::DirtiedPrimEntries* highlightEntries
)
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("    marking %s wireframe highlight locator dirty.\n", primPath.GetText());

    highlightEntries->emplace_back(primPath, HdDataSourceLocatorSet {reprSelectorLocator, primvarsOverrideWireframeColorLocator});
    for (const auto& childPath : GetChildPrimPaths(primPath)) {
        _DirtySelectionHighlightRecursive(childPath, highlightEntries);
    }
}

void WireframeSelectionHighlightSceneIndex::addExcludedSceneRoot(
    const PXR_NS::SdfPath& sceneRoot
)
{
    _excludedSceneRoots.emplace(sceneRoot);
}

bool WireframeSelectionHighlightSceneIndex::_IsExcluded(
    const PXR_NS::SdfPath& sceneRoot
) const
{
    for (const auto& excluded : _excludedSceneRoots) {
        if (sceneRoot.HasPrefix(excluded)) {
            return true;
        }
    }
    return false;
}

SdfPath
WireframeSelectionHighlightSceneIndex::GetSelectionHighlightPath(const SdfPath& path) const
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        const SdfPath mirrorPath = _GetSelectionHighlightMirrorPathFromOriginal(ancestor);
        if (_selectionHighlightMirrorUseCounters.find(mirrorPath) != _selectionHighlightMirrorUseCounters.end()
            && _selectionHighlightMirrorUseCounters.at(mirrorPath) > 0) {
            return path.ReplacePrefix(ancestor, mirrorPath);
        }
    }
    return path;
}

void
WireframeSelectionHighlightSceneIndex::_ForEachPrimInHierarchy(
    const PXR_NS::SdfPath& hierarchyRoot, 
    const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation
)
{
    std::stack<SdfPath> pathsToTraverse({hierarchyRoot});
    while (!pathsToTraverse.empty()) {
        SdfPath currPath = pathsToTraverse.top();
        pathsToTraverse.pop();

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);

        // Skip processing of prototypes nested under the hierarchy, as we consider prototype hierarchies to be separate.
        if (_IsPrototype(currPrim) && _IsInstancingRoot(currPrim, currPath) && currPath != hierarchyRoot) {
            continue;
        }

        if (operation(currPath, currPrim)) {
            for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
                pathsToTraverse.push(childPath);
            }
        }
    }
}

SdfPath
WireframeSelectionHighlightSceneIndex::_FindSelectionHighlightMirrorAncestor(const SdfPath& path) const
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        if (_selectionHighlightMirrorUseCounters.find(ancestor) != _selectionHighlightMirrorUseCounters.end() 
            && _selectionHighlightMirrorUseCounters.at(ancestor) > 0) {
            return ancestor;
        }
    }
    return SdfPath::EmptyPath();
}

void
WireframeSelectionHighlightSceneIndex::_CollectSelectionHighlightMirrors(const PXR_NS::SdfPath& originalPrimPath, PXR_NS::SdfPathSet& outSelectionHighlightMirrors, PXR_NS::HdSceneIndexObserver::AddedPrimEntries& outAddedPrims)
{
    // This should never be called on selection highlight prims, only on original prims
    TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths of the instancer will be
    // processed as part of the children traversal later down this method.
    if (!_IsInstancingRoot(originalPrim, originalPrimPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(originalPrim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            _CollectSelectionHighlightMirrors(protoRootPath, outSelectionHighlightMirrors, outAddedPrims);
        }
        return;
    }
    
    SdfPath selectionHighlightPrimPath = _GetSelectionHighlightMirrorPathFromOriginal(originalPrimPath);
    
    if (outSelectionHighlightMirrors.find(selectionHighlightPrimPath) != outSelectionHighlightMirrors.end()) {
        return;
    }
    outSelectionHighlightMirrors.insert(selectionHighlightPrimPath);

    // Traverse the children of this prim to find the child instancers, and add its instancing-related
    // paths so we can process them and create selection highlight mirrors for them as well.
    SdfPathVector affectedOriginalPrimPaths;
    auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        outAddedPrims.push_back({primPath.ReplacePrefix(originalPrimPath, selectionHighlightPrimPath), prim.primType});
        if (prim.primType == HdPrimTypeTokens->instancer) {
            SdfPathVector instancingRelatedPaths = _GetInstancingRelatedPaths(prim);
            affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), instancingRelatedPaths.begin(), instancingRelatedPaths.end());
            return false; // We have found an instancer, don't process its children (nested instancers will be processed through the instancing-related paths).
        }
        return true;
    };
    _ForEachPrimInHierarchy(originalPrimPath, operation);

    for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
        _CollectSelectionHighlightMirrors(affectedOriginalPrimPath, outSelectionHighlightMirrors, outAddedPrims);
    }
}

void
WireframeSelectionHighlightSceneIndex::_AddInstancerHighlightUser(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    
    if (_instancerHighlightUsersByInstancer[instancerPath].find(userPath) != _instancerHighlightUsersByInstancer[instancerPath].end()) {
        return;
    }

    _instancerHighlightUsersByInstancer[instancerPath].insert(userPath);
    if (_selectionHighlightMirrorsByInstancer.find(instancerPath) == _selectionHighlightMirrorsByInstancer.end()) {
        SdfPathSet selectionHighlightMirrors;
        HdSceneIndexObserver::AddedPrimEntries addedPrims;
        _CollectSelectionHighlightMirrors(instancerPath, selectionHighlightMirrors, addedPrims);

        _selectionHighlightMirrorsByInstancer[instancerPath] = selectionHighlightMirrors;
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
            _selectionHighlightMirrorUseCounters[selectionHighlightMirror]++;
        }

        if (!addedPrims.empty()) {
            _SendPrimsAdded(addedPrims);
        }
    }
    else {
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
            _selectionHighlightMirrorUseCounters[selectionHighlightMirror]++;
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_RemoveInstancerHighlightUser(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
    TF_AXIOM(_instancerHighlightUsersByInstancer.find(instancerPath) != _instancerHighlightUsersByInstancer.end());
    TF_AXIOM(_instancerHighlightUsersByInstancer.at(instancerPath).find(userPath) != _instancerHighlightUsersByInstancer.at(instancerPath).end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;
    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
        TF_AXIOM(_selectionHighlightMirrorUseCounters[selectionHighlightMirror] > 0);
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]--;
        if (_selectionHighlightMirrorUseCounters[selectionHighlightMirror] == 0) {
            _selectionHighlightMirrorUseCounters.erase(selectionHighlightMirror);
            removedPrims.push_back(selectionHighlightMirror);
        }
    }

    _instancerHighlightUsersByInstancer[instancerPath].erase(userPath);
    if (_instancerHighlightUsersByInstancer[instancerPath].empty()) {
        _instancerHighlightUsersByInstancer.erase(instancerPath);
        _selectionHighlightMirrorsByInstancer.erase(instancerPath);
    }
    
    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
}

void
WireframeSelectionHighlightSceneIndex::_RebuildInstancerHighlight(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    TF_AXIOM(_instancerHighlightUsersByInstancer.find(instancerPath) != _instancerHighlightUsersByInstancer.end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    auto instancerHighlightUsers = _instancerHighlightUsersByInstancer[instancerPath];
    
    for (const auto& instancerHighlightUser : instancerHighlightUsers) {
        _RemoveInstancerHighlightUser(instancerPath, instancerHighlightUser);
    }

    for (const auto& instancerHighlightUser : instancerHighlightUsers) {
        _AddInstancerHighlightUser(instancerPath, instancerHighlightUser);
    }
}

void
WireframeSelectionHighlightSceneIndex::_DeleteInstancerHighlight(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(_instancerHighlightUsersByInstancer.find(instancerPath) != _instancerHighlightUsersByInstancer.end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    auto instancerHighlightUsers = _instancerHighlightUsersByInstancer[instancerPath];
    for (const auto& instancerHighlightUser : instancerHighlightUsers) {
        _RemoveInstancerHighlightUser(instancerPath, instancerHighlightUser);
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateInstancerHighlightsForInstancer(const HdSceneIndexPrim& instancerPrim, const SdfPath& instancerPath)
{
    auto roots = _GetPrototypeRoots(instancerPrim);
    // If there are no prototype roots, consider the whole hierarchy
    if (roots.empty()) {
        roots.push_back(SdfPath::AbsoluteRootPath());
    }
    for (const auto& root : roots) {
        // Ancestors include the instancer itself
        auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(instancerPath, root);
        for (const auto& selectedAncestor : selectedAncestors) {
            _AddInstancerHighlightUser(instancerPath, selectedAncestor);
        }
    }
}

}
