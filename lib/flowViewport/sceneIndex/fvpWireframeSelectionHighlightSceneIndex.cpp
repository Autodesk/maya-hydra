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
#include "flowViewport/colorPreferences/fvpColorPreferences.h"
#include "flowViewport/colorPreferences/fvpColorPreferencesTokens.h"
#include "flowViewport/fvpUtils.h"

#include "flowViewport/debugCodes.h"
#include "fvpWireframeSelectionHighlightSceneIndex.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/diagnosticLite.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/primOriginSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/schemaTypeDefs.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usdImaging/usdImaging/rerootingSceneIndex.h>
#include <pxr/usdImaging/usdImaging/usdPrimInfoSchema.h>
#include <algorithm>
#include <numeric>
#include <stack>
#include <iostream>
#include <mutex>

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

VtBoolArray _GetSelectionHighlightMask(const HdInstancerTopologySchema& originalInstancerTopology, const HdSelectionsSchema& selections) 
{
    if (!selections.IsDefined()) {
        return originalInstancerTopology.GetMask()->GetTypedValue(0);
    }

    size_t nbInstances = 0;
    auto instanceIndices = originalInstancerTopology.GetInstanceIndices();
    for (size_t iInstanceIndex = 0; iInstanceIndex < instanceIndices.GetNumElements(); iInstanceIndex++) {
        auto protoInstances = instanceIndices.GetElement(iInstanceIndex)->GetTypedValue(0);
        nbInstances += protoInstances.size();
    }
    VtBoolArray originalMask = originalInstancerTopology.GetMask()->GetTypedValue(0);
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
            return originalInstancerTopology.GetMask()->GetTypedValue(0);
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

bool _IsInstancerWithSelections(const HdSceneIndexPrim& prim)
{
    if (prim.primType != HdPrimTypeTokens->instancer) {
        return false;
    }
    auto selections = HdSelectionsSchema::GetFromParent(prim.dataSource);
    return selections.IsDefined() && selections.GetNumElements() > 0;
}

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

    std::stack<SdfPath> pathsToTraverse({ SdfPath::AbsoluteRootPath() });
    while (!pathsToTraverse.empty()) {
        SdfPath currPath = pathsToTraverse.top();
        pathsToTraverse.pop();

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);
        if (currPrim.primType == HdPrimTypeTokens->instancer) {
            auto roots = _GetPrototypeRoots(currPrim);
            if (roots.empty()) {
                roots.push_back(SdfPath::AbsoluteRootPath());
            }
            for (const auto& root : roots) {
                auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(currPath, root);
                for (const auto& selectedAncestor : selectedAncestors) {
                    _AddSelectionHighlightMirrorsUsageForInstancer(currPath, selectedAncestor);
                }
            }
        }
        else {
            for (const auto& childPath : inputSceneIndex->GetChildPrimPaths(currPath)) {
                pathsToTraverse.push(childPath);
            }
        }
    }
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
            selectionHighlightPrim.dataSource = _SelectionHighlightRepathingContainerDataSource::New(selectionHighlightPrim.dataSource, this);
        }
        if (selectionHighlightPrim.primType == HdPrimTypeTokens->mesh) {
            selectionHighlightPrim.dataSource = _HighlightSelectedPrim(selectionHighlightPrim.dataSource, originalPrimPath, sRefinedWireDisplayStyleDataSource);
        }
        if (selectionHighlightPrim.primType == HdPrimTypeTokens->instancer) {
            selectionHighlightPrim.dataSource = _GetSelectionHighlightInstancerDataSource(selectionHighlightPrim.dataSource);
        }
        return selectionHighlightPrim;
    }
    
    // We may be dealing with a prototype selection, a regular selection, or no selection at all.
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        // Note : in the USD data model, the original prims that get propagated as prototypes have their original prim types erased.
        // Only the resulting propagated prototypes keep the original prim type. Presumably this is to avoid drawing the original
        // prim (even though it should already not be drawn due to being under an instancer, this is an additional safety? to confirm)
        if (_IsPrototype(prim)) {
            // Prototype selection
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() && !instancedBy.GetPrototypeRoots()->GetTypedValue(0).empty()) {
                auto protoRoots = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
                for (const auto& protoRoot : protoRoots) {
                    if (_selection->HasFullySelectedAncestorInclusive(primPath, protoRoot)) {
                        prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
                        break;
                    }
                }
            }
            else {
                if (_selection->HasFullySelectedAncestorInclusive(primPath)) {
                    prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
                }
            }
        } else {
            // Regular selection
            if (_selection->HasFullySelectedAncestorInclusive(primPath)) {
                prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
            }
        }
    }
    return prim;
}

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    // When within a selection highlight mirror hierarchy, query the corresponding original prim
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

    // TODO HYDRA-1047 : Handle the different wireframe selection highlight colors
    GfVec4f wireframeColor;
    ColorPreferences::getInstance().getColor(FvpColorPreferencesTokens->wireframeSelection, wireframeColor);
    edited.Set(HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor),
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(VtVec4fArray{wireframeColor}),
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
            auto roots = _GetPrototypeRoots(prim);
            if (roots.empty()) {
                roots.push_back(SdfPath::AbsoluteRootPath());
            }
            for (const auto& root : roots) {
                auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(entry.primPath, root);
                for (const auto& selectedAncestor : selectedAncestors) {
                    _AddSelectionHighlightMirrorsUsageForInstancer(entry.primPath, selectedAncestor);
                }
            }
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
    std::vector<std::pair<SdfPath, SdfPath>> selectedInstancerHighlightUsages;
    std::vector<std::pair<SdfPath, SdfPath>> deselectedInstancerHighlightUsages;
    for (const auto& entry : entries) {
        if (_IsExcluded(entry.primPath)) {
            // If the dirtied prim is excluded, don't provide selection
            // highlighting for it.
            continue;
        }

        auto selectionHighlightPath = GetSelectionHighlightPath(entry.primPath);
        if (selectionHighlightPath != entry.primPath) {
            dirtiedPrims.emplace_back(selectionHighlightPath, entry.dirtyLocators);
        }

        if (entry.dirtyLocators.Contains(HdSelectionsSchema::GetDefaultLocator())) {
            TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
                .Msg("    %s selections locator dirty.\n", entry.primPath.GetText());
            
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            _DirtySelectionHighlightRecursive(entry.primPath, &dirtiedPrims);
            if (selectionHighlightPath != entry.primPath) {
                _DirtySelectionHighlightRecursive(entry.primPath, &dirtiedPrims);
            }

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            bool isSelected = selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;

            std::stack<SdfPath> pathsToTraverse({entry.primPath});
            while (!pathsToTraverse.empty()) {
                SdfPath currPath = pathsToTraverse.top();
                pathsToTraverse.pop();

                HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);
                
                if (currPrim.primType == HdPrimTypeTokens->instancer) {
                    if (isSelected) {
                        selectedInstancerHighlightUsages.push_back({currPath,entry.primPath});
                    } else {
                        deselectedInstancerHighlightUsages.push_back({currPath,entry.primPath});
                    }
                }
                else if (!_IsPrototype(currPrim)) {
                    for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
                        pathsToTraverse.push(childPath);
                    }
                }
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

    for (const auto& selectedInstancerHighlightUsage : selectedInstancerHighlightUsages) {
        _AddSelectionHighlightMirrorsUsageForInstancer(selectedInstancerHighlightUsage.first, selectedInstancerHighlightUsage.second);
    }
    for (const auto& deselectedInstancerHighlightUsage : deselectedInstancerHighlightUsages) {
        _RemoveSelectionHighlightMirrorsUsageForInstancer(deselectedInstancerHighlightUsage.first, deselectedInstancerHighlightUsage.second);
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
        auto instancerPathsCopy = _selectionHighlightMirrorsByInstancer;
        for (const auto& selectionHighlightMirrorsForInstancer : instancerPathsCopy) {
            auto instancerPath = selectionHighlightMirrorsForInstancer.first;
            if (instancerPath.HasPrefix(entry.primPath)) {
                _DeleteSelectionHighlightMirrorsUsageForInstancer(instancerPath);
            }
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

    SdfPathVector affectedOriginalPrimPaths;
    std::stack<SdfPath> pathsToTraverse({originalPrimPath});
    while (!pathsToTraverse.empty()) {
        SdfPath currPath = pathsToTraverse.top();
        pathsToTraverse.pop();

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);

        // Skip processing of prototypes nested under the prim's hierarchy, as prototypes should be processed by
        // traversing the instancing-related paths of instancers and instancees. And if they aren't, then it means
        // the prototype is either not instanced in the first place, or is part of a different instancing hierarchy.
        if (_IsPrototype(currPrim) && _IsInstancingRoot(currPrim, currPath) && currPath != originalPrimPath) {
            continue;
        }

        outAddedPrims.push_back({currPath.ReplacePrefix(originalPrimPath, selectionHighlightPrimPath), currPrim.primType});

        if (currPrim.primType == HdPrimTypeTokens->instancer) {
            SdfPathVector instancingRelatedPaths = _GetInstancingRelatedPaths(currPrim);
            affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), instancingRelatedPaths.begin(), instancingRelatedPaths.end());
        }

        for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
            pathsToTraverse.push(childPath);
        }
    }

    for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
        _CollectSelectionHighlightMirrors(affectedOriginalPrimPath, outSelectionHighlightMirrors, outAddedPrims);
    }
}

void 
WireframeSelectionHighlightSceneIndex::_RemoveSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;

    SdfPathSet oldSelectionHighlightMirrors;
    if (_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end()) {
        oldSelectionHighlightMirrors = _selectionHighlightMirrorsByInstancer.at(instancerPath);
    }
    for (const auto& selectionHighlightMirror : oldSelectionHighlightMirrors) {
        TF_AXIOM(_selectionHighlightMirrorUseCounters.find(selectionHighlightMirror) != _selectionHighlightMirrorUseCounters.end());
        TF_AXIOM(_selectionHighlightMirrorUseCounters.at(selectionHighlightMirror) > 0);
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]--;
        if (_selectionHighlightMirrorUseCounters.at(selectionHighlightMirror) == 0) {
            _selectionHighlightMirrorUseCounters.erase(selectionHighlightMirror);
            removedPrims.push_back(selectionHighlightMirror);
        }
    }

    _selectionHighlightMirrorsByInstancer.erase(instancerPath);
    
    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
}

void 
WireframeSelectionHighlightSceneIndex::_UpdateSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;
    if (_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end()) {
        removedPrims = _DecrementSelectionHighlightMirrorsForInstancer(instancerPath);
    }

    SdfPathSet selectionHighlightMirrors;
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    _CollectSelectionHighlightMirrors(instancerPath, selectionHighlightMirrors, addedPrims);

    _selectionHighlightMirrorsByInstancer[instancerPath] = selectionHighlightMirrors;

    _IncrementSelectionHighlightMirrorsForInstancer(instancerPath);

    SdfPathSet removedAndAddedPrims;
    for (const auto& removedPrim : removedPrims) {
        for (const auto& addedPrim : addedPrims) {
            if (removedPrim.primPath == addedPrim.primPath) {
                removedAndAddedPrims.insert(removedPrim.primPath);
            }
        }
    }
    removedPrims.erase(std::remove_if(removedPrims.begin(), removedPrims.end(), 
        [&removedAndAddedPrims](const HdSceneIndexObserver::RemovedPrimEntry& removedPrim) -> bool { 
        return removedAndAddedPrims.find(removedPrim.primPath) != removedAndAddedPrims.end(); 
    }));
    addedPrims.erase(std::remove_if(addedPrims.begin(), addedPrims.end(), 
        [&removedAndAddedPrims](const HdSceneIndexObserver::AddedPrimEntry& addedPrim) -> bool { 
        return removedAndAddedPrims.find(addedPrim.primPath) != removedAndAddedPrims.end(); 
    }));

    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
    if (!addedPrims.empty()) {
        _SendPrimsAdded(addedPrims);
    }
}

HdSceneIndexObserver::RemovedPrimEntries
WireframeSelectionHighlightSceneIndex::_DecrementSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;

    SdfPathSet selectionHighlightMirrors = _selectionHighlightMirrorsByInstancer.at(instancerPath);
    for (const auto& selectionHighlightMirror : selectionHighlightMirrors) {
        TF_AXIOM(_selectionHighlightMirrorUseCounters.find(selectionHighlightMirror) != _selectionHighlightMirrorUseCounters.end());
        TF_AXIOM(_selectionHighlightMirrorUseCounters.at(selectionHighlightMirror) > 0);
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]--;
        if (_selectionHighlightMirrorUseCounters.at(selectionHighlightMirror) == 0) {
            removedPrims.push_back(selectionHighlightMirror);
        }
    }
    
    return removedPrims;
}

void
WireframeSelectionHighlightSceneIndex::_IncrementSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());
    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer.at(instancerPath)) {
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]++;
    }
}

void
WireframeSelectionHighlightSceneIndex::_RemoveSelectionHighlightMirrorsUsageForInstancer(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    TF_AXIOM(_instancerHighlightUsers.find(instancerPath) != _instancerHighlightUsers.end());
    TF_AXIOM(_instancerHighlightUsers.at(instancerPath).find(userPath) != _instancerHighlightUsers.at(instancerPath).end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    std::cout << "Removing usage for " << instancerPath.GetString() << std::endl;

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;
    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
        TF_AXIOM(_selectionHighlightMirrorUseCounters[selectionHighlightMirror] > 0);
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]--;
        if (_selectionHighlightMirrorUseCounters[selectionHighlightMirror] == 0) {
            removedPrims.push_back(selectionHighlightMirror);
        }
    }

    _instancerHighlightUsers[instancerPath].erase(userPath);
    if (_instancerHighlightUsers[instancerPath].empty()) {
        _selectionHighlightMirrorsByInstancer.erase(instancerPath);
    }
    
    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
}

void
WireframeSelectionHighlightSceneIndex::_DeleteSelectionHighlightMirrorsUsageForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    TF_AXIOM(_instancerHighlightUsers.find(instancerPath) != _instancerHighlightUsers.end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    std::cout << "Removing usage for " << instancerPath.GetString() << std::endl;

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;
    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
        TF_AXIOM(_selectionHighlightMirrorUseCounters[selectionHighlightMirror] > 0);
        _selectionHighlightMirrorUseCounters[selectionHighlightMirror]--;
        if (_selectionHighlightMirrorUseCounters[selectionHighlightMirror] == 0) {
            removedPrims.push_back(selectionHighlightMirror);
        }
    }

    _instancerHighlightUsers[instancerPath].clear();
    _selectionHighlightMirrorsByInstancer.erase(instancerPath);
    
    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
}

void
WireframeSelectionHighlightSceneIndex::_AddSelectionHighlightMirrorsUsageForInstancer(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);
    
    std::cout << "Adding usage for " << instancerPath.GetString() << std::endl;

    if (_instancerHighlightUsers[instancerPath].find(userPath) != _instancerHighlightUsers[instancerPath].end()) {
        return;
    }

    _instancerHighlightUsers[instancerPath].insert(userPath);
    if (_selectionHighlightMirrorsByInstancer.find(instancerPath) == _selectionHighlightMirrorsByInstancer.end()) {
        SdfPathSet selectionHighlightMirrors;
        HdSceneIndexObserver::AddedPrimEntries addedPrims;
        _CollectSelectionHighlightMirrors(instancerPath, selectionHighlightMirrors, addedPrims);

        _selectionHighlightMirrorsByInstancer[instancerPath] = selectionHighlightMirrors;
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
            _selectionHighlightMirrorUseCounters[selectionHighlightMirror]++;
        }

        // addedPrims.erase(std::remove_if(addedPrims.begin(), addedPrims.end(), [this](const HdSceneIndexObserver::AddedPrimEntry entry) -> bool {
        //     return _selectionHighlightMirrorUseCounters[entry.primPath] != 1;
        // }));
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

}
