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

#if PXR_VERSION >= 2403
#include <pxr/imaging/hd/geomSubsetSchema.h>
#endif
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <unordered_set>
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

const HdDataSourceLocator reprSelectorLocator(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdLegacyDisplayStyleSchemaTokens->reprSelector);

const HdDataSourceLocator primvarsOverrideWireframeColorLocator(
        HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor));

const std::string selectionHighlightMirrorTag = "_SelectionHighlight";

SdfPath _GetSelectionHighlightMirrorPathFromOriginal(const SdfPath& originalPath)
{
    if (originalPath == SdfPath::AbsoluteRootPath()) {
        return originalPath; //Avoid a warning in Hydra
    }
    return originalPath.ReplaceName(TfToken(originalPath.GetName() + selectionHighlightMirrorTag));
}

SdfPath _GetOriginalPathFromSelectionHighlightMirror(const SdfPath& mirrorPath)
{
    const std::string primName = mirrorPath.GetName();
    return mirrorPath.ReplaceName(TfToken(primName.substr(0, primName.size() - selectionHighlightMirrorTag.size())));
}

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

    if ((direction & Fvp::SelectionHighlightsCollectionDirection::Instancers)
        && instancedBy.IsDefined()) {
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

// We consider prototypes that have child prims to be different hierarchies,
// separate from each other and from the "root" hierarchy.
VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
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
            _CreateSelectionHighlightsForInstancer(prim, primPath);
        }
        else if (prim.primType == HdPrimTypeTokens->mesh) {
            _CreateSelectionHighlightsForMesh(prim, primPath);
        }
#if PXR_VERSION >= 2403
        else if (prim.primType == HdPrimTypeTokens->geomSubset) {
            _CreateSelectionHighlightsForGeomSubset(primPath);
        }
#endif
        return true;
    };
    _ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

// Computes the mask to use for an instancer's selection highlight mirror
// based on the instancer's topology and its selections. This allows
// highlighting only specific instances in the case of instance selections.
VtBoolArray
WireframeSelectionHighlightSceneIndex::_GetSelectionHighlightMask(const HdInstancerTopologySchema& originalInstancerTopology, const HdSelectionsSchema& selections) const
{
    // Schema getters were made const in USD 24.05 (specifically Hydra API version 66).
    // We work around this for previous versions by const casting.
    VtBoolArray originalMask = 
#if HD_API_VERSION < 66
    const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetMask()->GetTypedValue(0);
#else
    originalInstancerTopology.GetMask()->GetTypedValue(0);
#endif

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
    if (!TF_VERIFY(originalMask.empty() || originalMask.size() == nbInstances, "Instancer mask has incorrect size.")) {
        return originalMask;
    }
    VtBoolArray selectionHighlightMask = [&]() {
        if (!selections.IsDefined()) {
            return originalMask.empty() ? VtBoolArray(nbInstances, true) : originalMask;
        }
        return VtBoolArray(nbInstances, false);
    }();

    if (!selections.IsDefined()) {
        // There are no selections on this instancer highlight mirror; this means it was created 
        // in order to propagate the selection highlight mirror of at least one of its prototypes.
        // Since we don't want to highlight non-selected prototypes, hide all instances for which`
        // their prototype has no selection highlight.
        auto protos = 
#if HD_API_VERSION < 66
        const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetPrototypes()->GetTypedValue(0);
#else
        originalInstancerTopology.GetPrototypes()->GetTypedValue(0);
#endif
        for (size_t iProto = 0; iProto < protos.size(); iProto++) {
            auto protoPath = protos[iProto];
            auto protoHighlightPath = GetSelectionHighlightPath(protoPath);
            if (protoHighlightPath == protoPath) {
                // No selection highlight for this prototype; disable its instances
                auto protoInstanceIndices = 
#if HD_API_VERSION < 66
                const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetInstanceIndices().GetElement(iProto)->GetTypedValue(0);
#else
                originalInstancerTopology.GetInstanceIndices().GetElement(iProto)->GetTypedValue(0);
#endif
                for (const auto& protoInstanceIndex : protoInstanceIndices) {
                    selectionHighlightMask[protoInstanceIndex] = false;
                }
            }
        }
        return selectionHighlightMask;
    }

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
// This replaces the mask data source.
HdContainerDataSourceHandle
WireframeSelectionHighlightSceneIndex::_GetSelectionHighlightInstancerDataSource(const HdContainerDataSourceHandle& originalDataSource) const
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

    return editedDataSource.Finish();
}

HdSceneIndexPrim
WireframeSelectionHighlightSceneIndex::GetPrim(const SdfPath &primPath) const
{
    TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
        .Msg("WireframeSelectionHighlightSceneIndex::GetPrim(%s) called.\n", primPath.GetText());

    if (_IsExcluded(primPath)) {
        return GetInputSceneIndex()->GetPrim(primPath);
    }
    
    // If this prim is part of a selection highlight mirror hierarchy, tweak the prim's data source accordingly.
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
#if PXR_VERSION >= 2403
                selectionHighlightPrim.dataSource = _TrimMeshForSelectedGeomSubsets(selectionHighlightPrim.dataSource, originalPrimPath);
#endif
            }
#if PXR_VERSION >= 2403
            else if (selectionHighlightPrim.primType == HdPrimTypeTokens->geomSubset) {
                // If we returned the geomSubset prims unchanged, they could contain face indices that exceed
                // the trimmed mesh's number of faces, which prints a warning. We don't need the geomSubset
                // highlight mirrors anyways, so just return nothing.
                selectionHighlightPrim.dataSource = {};
            }
#endif

            // Block out the selections data source as we don't actually select a highlight
            if (selectionHighlightPrim.dataSource) {
                HdContainerDataSourceEditor dataSourceEditor = HdContainerDataSourceEditor(selectionHighlightPrim.dataSource);
                dataSourceEditor.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());
                selectionHighlightPrim.dataSource = dataSourceEditor.Finish();
            }
        }
        return selectionHighlightPrim;
    }
    
    // This prim is not in a selection highlight mirror hierarchy; just pass-through our input.
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    // When within a selection highlight mirror hierarchy, query the corresponding original prim's children
    SdfPath selectionHighlightMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!selectionHighlightMirrorAncestor.IsEmpty()) {
        SdfPath originalAncestor = _GetOriginalPathFromSelectionHighlightMirror(selectionHighlightMirrorAncestor);
        auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(selectionHighlightMirrorAncestor, originalAncestor));
        SdfPathVector implicitSelectionHighlightChildPaths;
        for (const auto& originalChildPath : originalChildPaths) {
            SdfPath explicitSelectionHighlightChildPath = _GetSelectionHighlightMirrorPathFromOriginal(originalChildPath);
            if (_selectionHighlightMirrorUseCounters.find(explicitSelectionHighlightChildPath) != _selectionHighlightMirrorUseCounters.end()
                && _selectionHighlightMirrorUseCounters.at(explicitSelectionHighlightChildPath) > 0) {
                // There already exists an explicit selection highlight mirror for this child (e.g. point instance prototypes),
                // so don't create a duplicate implicit one.
                continue;
            }
            implicitSelectionHighlightChildPaths.push_back(originalChildPath.ReplacePrefix(originalAncestor, selectionHighlightMirrorAncestor));
        }
        return implicitSelectionHighlightChildPaths;
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
    edited.Set(primvarsOverrideWireframeColorLocator,
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

#if PXR_VERSION >= 2403
// Edits the mesh topology to only only contain its selected GeomSubsets
HdContainerDataSourceHandle
WireframeSelectionHighlightSceneIndex::_TrimMeshForSelectedGeomSubsets(const HdContainerDataSourceHandle& originalDataSource, const SdfPath& originalPrimPath) const
{
    HdMeshSchema meshSchema = HdMeshSchema::GetFromParent(originalDataSource);
    if (!meshSchema.IsDefined()) {
        return originalDataSource;
    }
    HdMeshTopologySchema meshTopologySchema = meshSchema.GetTopology();
    if (!meshTopologySchema.IsDefined()) {
        return originalDataSource;
    }
    HdDataSourceLocator pointsValueLocator = HdDataSourceLocator(HdPrimvarsSchemaTokens->primvars, HdPrimvarsSchemaTokens->points, HdPrimvarSchemaTokens->primvarValue);
    auto pointsValueDataSource = HdTypedSampledDataSource<VtArray<GfVec3f>>::Cast(HdContainerDataSource::Get(originalDataSource, pointsValueLocator));
    if (!pointsValueDataSource) {
        return originalDataSource;
    }

    // Collect faces to keep based on selected GeomSubsets
    std::unordered_set<int> faceIndicesToKeep;
    for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(originalPrimPath)) {
        HdSceneIndexPrim childPrim = GetInputSceneIndex()->GetPrim(childPath);
        if (childPrim.primType != HdPrimTypeTokens->geomSubset) {
            continue;
        }

        HdGeomSubsetSchema geomSubsetSchema = HdGeomSubsetSchema(childPrim.dataSource);
        if (!geomSubsetSchema.IsDefined() || geomSubsetSchema.GetType()->GetTypedValue(0) != HdGeomSubsetSchemaTokens->typeFaceSet) {
            continue;
        }

        HdSelectionsSchema geomSubsetSelections = HdSelectionsSchema::GetFromParent(childPrim.dataSource);
        if (!geomSubsetSelections.IsDefined() || geomSubsetSelections.GetNumElements() == 0) {
            continue;
        }

        VtArray<int> faceIndices = geomSubsetSchema.GetIndices()->GetTypedValue(0);
        for (const auto& faceIndex : faceIndices) {
            faceIndicesToKeep.insert(faceIndex);
        }
    }
    if (faceIndicesToKeep.empty()) {
        // If there are no selected geomSubsets, don't trim the mesh.
        return originalDataSource;
    }

    // Edit the mesh topology
    HdContainerDataSourceEditor dataSourceEditor = HdContainerDataSourceEditor(originalDataSource);
    VtArray<int> originalFaceVertexCounts = meshTopologySchema.GetFaceVertexCounts()->GetTypedValue(0);
    VtArray<int> originalFaceVertexIndices = meshTopologySchema.GetFaceVertexIndices()->GetTypedValue(0);
    VtArray<int> trimmedFaceVertexCounts;
    VtArray<int> trimmedFaceVertexIndices;
    int maxVertexIndex = 0;
    size_t iFaceCounts = 0;
    size_t iFaceIndices = 0;
    while (iFaceCounts < originalFaceVertexCounts.size() && iFaceIndices < originalFaceVertexIndices.size()) {
        int currFaceCount = originalFaceVertexCounts[iFaceCounts];

        if (faceIndicesToKeep.find(iFaceCounts) != faceIndicesToKeep.end()) {
            trimmedFaceVertexCounts.push_back(currFaceCount);
            for (int faceIndicesOffset = 0; faceIndicesOffset < currFaceCount; faceIndicesOffset++) {
                int vertexIndex = originalFaceVertexIndices[iFaceIndices + faceIndicesOffset];
                trimmedFaceVertexIndices.push_back(vertexIndex);
                if (vertexIndex > maxVertexIndex) {
                    maxVertexIndex = vertexIndex;
                }
            }
        }

        iFaceCounts++;
        iFaceIndices += currFaceCount;
    }
    auto faceVertexCountsLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexCounts);
    auto faceVertexIndicesLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexIndices);
    
    dataSourceEditor.Set(faceVertexCountsLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexCounts));
    dataSourceEditor.Set(faceVertexIndicesLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexIndices));

    // We reduce the points primvar so that it has only the exact number of points required by the trimmed topology;
    // this avoids a warning from USD.
    VtArray<GfVec3f> points = pointsValueDataSource->GetTypedValue(0);
    points.resize(maxVertexIndex + 1);
    dataSourceEditor.Set(pointsValueLocator, HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>::New(points));

    return dataSourceEditor.Finish();
}
#endif

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
            _CreateSelectionHighlightsForInstancer(prim, entry.primPath);
        }
        else if (prim.primType == HdPrimTypeTokens->mesh) {
            _CreateSelectionHighlightsForMesh(prim, entry.primPath);
        }
#if PXR_VERSION >= 2403
        else if (prim.primType == HdPrimTypeTokens->geomSubset) {
            _CreateSelectionHighlightsForGeomSubset(entry.primPath);
        }
#endif
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
    std::vector<SdfPath> selectionHighlightsToRebuild;
    std::vector<std::pair<SdfPath, SdfPath>> selectionHighlightUsersToAdd;
    std::vector<std::pair<SdfPath, SdfPath>> selectionHighlightUsersToRemove;

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
            && _selectionHighlightMirrorsByPrim.find(entry.primPath) != _selectionHighlightMirrorsByPrim.end()) {
            // An instancer with a selection highlight was changed; rebuild its selection highlight.
            // We do not need to check for instancedBy dirtying. If an instancedBy data source is dirtied,
            // then either :
            // 1) A new instancer was added : this is handled in _PrimsAdded.
            // 2) An existing instancer's instancerTopology data source was dirtied : this is handled here.
            selectionHighlightsToRebuild.push_back(entry.primPath);
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

#if PXR_VERSION >= 2403
            // If a geomSubset's selection changes, dirty the selection highlight mesh to trim it appropriately.
            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                SdfPath meshPath = entry.primPath.GetParentPath();
                SdfPath selectionHighlightMeshPath = GetSelectionHighlightPath(meshPath);
                if (selectionHighlightMeshPath != meshPath) {
                    dirtiedPrims.emplace_back(selectionHighlightMeshPath, HdDataSourceLocator(HdMeshSchemaTokens->mesh, HdMeshSchemaTokens->topology));
                }
            }
#endif
            
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            _DirtySelectionHighlightRecursive(entry.primPath, &dirtiedPrims);
            if (selectionHighlightPath != entry.primPath) {
                _DirtySelectionHighlightRecursive(selectionHighlightPath, &dirtiedPrims);
            }

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            bool isSelected = selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;

#if PXR_VERSION >= 2403
            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                if (isSelected) {
                    selectionHighlightUsersToAdd.push_back({entry.primPath.GetParentPath(), entry.primPath});
                } else {
                    selectionHighlightUsersToRemove.push_back({entry.primPath.GetParentPath(), entry.primPath});
                }
            }
#endif

            // Update child selection highlights for ancestor-based selection highlighting
            // (i.e. selecting one or more of an instancer's parents should highlight the instancer,
            // same thing for meshes)
            auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                if ((prim.primType == HdPrimTypeTokens->instancer && !_IsPrototype(prim))
                    || prim.primType == HdPrimTypeTokens->mesh) {
                    if (isSelected) {
                        selectionHighlightUsersToAdd.push_back({primPath,entry.primPath});
                    } else {
                        selectionHighlightUsersToRemove.push_back({primPath,entry.primPath});
                    }
                }
                return true;
            };
            _ForEachPrimInHierarchy(entry.primPath, operation);
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

    for (const auto& selectionHighlightToRebuild : selectionHighlightsToRebuild) {
        _RebuildSelectionHighlight(selectionHighlightToRebuild);
    }
    for (const auto& selectionHighlightUserToAdd : selectionHighlightUsersToAdd) {
        _AddSelectionHighlightUser(selectionHighlightUserToAdd.first, selectionHighlightUserToAdd.second);
    }
    for (const auto& selectionHighlightUserToRemove : selectionHighlightUsersToRemove) {
        _RemoveSelectionHighlightUser(selectionHighlightUserToRemove.first, selectionHighlightUserToRemove.second);
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
        // Collect and delete selection highlights for all prims rooted under the removed prim
        // (or if the removed prim itself has a highlight)
        SdfPathVector selectionHighlightsToDelete;
        for (const auto& selectionHighlightMirrorsForPrim : _selectionHighlightMirrorsByPrim) {
            SdfPath primPath = selectionHighlightMirrorsForPrim.first;
            if (primPath.HasPrefix(entry.primPath)) {
                selectionHighlightsToDelete.push_back(primPath);
            }
        }
        for (const auto& selectionHighlightToDelete : selectionHighlightsToDelete) {
            _DeleteSelectionHighlight(selectionHighlightToDelete);
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

std::string
WireframeSelectionHighlightSceneIndex::GetSelectionHighlightMirrorTag() const
{
    return selectionHighlightMirrorTag;
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

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetSelectionHighlightMirrorPaths() const
{
    SdfPathVector mirrorPaths;
    for (const auto& selectionHighlightMirrorKvp : _selectionHighlightMirrorUseCounters) {
        mirrorPaths.push_back(selectionHighlightMirrorKvp.first);
    }
    return mirrorPaths;
}

void
WireframeSelectionHighlightSceneIndex::_ForEachPrimInHierarchy(
    const PXR_NS::SdfPath& hierarchyRoot, 
    const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation
)
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
WireframeSelectionHighlightSceneIndex::_CollectSelectionHighlightMirrors(const PXR_NS::SdfPath& originalPrimPath, SelectionHighlightsCollectionDirection direction, PXR_NS::SdfPathSet& outSelectionHighlightMirrors, PXR_NS::HdSceneIndexObserver::AddedPrimEntries& outAddedPrims)
{
    // This should never be called on selection highlight prims, only on original prims
    TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths will be processed as part
    // of the children traversal later down this method.
    if (_IsPrototypeSubPrim(originalPrim, originalPrimPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(originalPrim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            _CollectSelectionHighlightMirrors(protoRootPath, direction, outSelectionHighlightMirrors, outAddedPrims);
        }
        return;
    }
    
    SdfPath selectionHighlightPrimPath = _GetSelectionHighlightMirrorPathFromOriginal(originalPrimPath);
    
    if (outSelectionHighlightMirrors.find(selectionHighlightPrimPath) != outSelectionHighlightMirrors.end()) {
        return;
    }
    outSelectionHighlightMirrors.insert(selectionHighlightPrimPath);

    // Traverse the children of this prim to find the affected child prims, and process their instancing-related
    // paths so we can create selection highlight mirrors for these prims as well.
    SdfPathVector affectedPrototypePaths;
    SdfPathVector affectedInstancerPaths;
    auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        outAddedPrims.push_back({primPath.ReplacePrefix(originalPrimPath, selectionHighlightPrimPath), prim.primType});
        if (prim.primType == HdPrimTypeTokens->instancer || prim.primType == HdPrimTypeTokens->mesh) {
            if (direction & SelectionHighlightsCollectionDirection::Prototypes) {
                auto prototypePaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection::Prototypes);
                affectedPrototypePaths.insert(affectedPrototypePaths.end(), prototypePaths.begin(), prototypePaths.end());
            }
            if (direction & SelectionHighlightsCollectionDirection::Instancers) {
                auto instancerPaths = _GetInstancingRelatedPaths(prim, SelectionHighlightsCollectionDirection::Instancers);
                affectedInstancerPaths.insert(affectedInstancerPaths.end(), instancerPaths.begin(), instancerPaths.end());
            }
            // We hit an instancing-related prim, don't process its children (nested instancing will be processed through the instancing-related paths).
            return false;
        }
        return true;
    };
    _ForEachPrimInHierarchy(originalPrimPath, operation);

    for (const auto& affectedPrototypePath : affectedPrototypePaths) {
        _CollectSelectionHighlightMirrors(affectedPrototypePath, SelectionHighlightsCollectionDirection::Prototypes, outSelectionHighlightMirrors, outAddedPrims);
    }
    for (const auto& affectedInstancerPath : affectedInstancerPaths) {
        _CollectSelectionHighlightMirrors(affectedInstancerPath, SelectionHighlightsCollectionDirection::Instancers, outSelectionHighlightMirrors, outAddedPrims);
    }
}

void
WireframeSelectionHighlightSceneIndex::_IncrementSelectionHighlightMirrorUseCounter(const PXR_NS::SdfPath& selectionHighlightMirrorPath)
{
    _selectionHighlightMirrorUseCounters[selectionHighlightMirrorPath]++;
}

void
WireframeSelectionHighlightSceneIndex::_DecrementSelectionHighlightMirrorUseCounter(const PXR_NS::SdfPath& selectionHighlightMirrorPath)
{
    TF_AXIOM(_selectionHighlightMirrorUseCounters[selectionHighlightMirrorPath] > 0);
    _selectionHighlightMirrorUseCounters[selectionHighlightMirrorPath]--;
    if (_selectionHighlightMirrorUseCounters[selectionHighlightMirrorPath] == 0) {
        _selectionHighlightMirrorUseCounters.erase(selectionHighlightMirrorPath);
        _SendPrimsRemoved({selectionHighlightMirrorPath});
    }
}

void
WireframeSelectionHighlightSceneIndex::_AddSelectionHighlightUser(const PXR_NS::SdfPath& primPath, const SdfPath& userPath)
{
    auto primType = GetInputSceneIndex()->GetPrim(primPath).primType;
    TF_AXIOM(primType == HdPrimTypeTokens->instancer || primType == HdPrimTypeTokens->mesh);

    if (_selectionHighlightUsersByPrim[primPath].find(userPath) != _selectionHighlightUsersByPrim[primPath].end()) {
        return;
    }

    SelectionHighlightsCollectionDirection selectionHighlightsCollectionDirection = SelectionHighlightsCollectionDirection::None;
    if (primType == HdPrimTypeTokens->instancer) {
        selectionHighlightsCollectionDirection = SelectionHighlightsCollectionDirection::Bidirectional;
    } else if (primType == HdPrimTypeTokens->mesh) {
        selectionHighlightsCollectionDirection = SelectionHighlightsCollectionDirection::Instancers;
    }

    _selectionHighlightUsersByPrim[primPath].insert(userPath);
    if (_selectionHighlightMirrorsByPrim.find(primPath) == _selectionHighlightMirrorsByPrim.end()) {
        SdfPathSet selectionHighlightMirrors;
        HdSceneIndexObserver::AddedPrimEntries addedPrims;
        _CollectSelectionHighlightMirrors(primPath, selectionHighlightsCollectionDirection, selectionHighlightMirrors, addedPrims);

        _selectionHighlightMirrorsByPrim[primPath] = selectionHighlightMirrors;
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByPrim[primPath]) {
            _IncrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
        }

        if (!addedPrims.empty()) {
            _SendPrimsAdded(addedPrims);
        }
    }
    else {
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByPrim[primPath]) {
            _IncrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_RemoveSelectionHighlightUser(const PXR_NS::SdfPath& primPath, const SdfPath& userPath)
{
    TF_AXIOM(_selectionHighlightUsersByPrim.find(primPath) != _selectionHighlightUsersByPrim.end());
    TF_AXIOM(_selectionHighlightUsersByPrim.at(primPath).find(userPath) != _selectionHighlightUsersByPrim.at(primPath).end());
    TF_AXIOM(_selectionHighlightMirrorsByPrim.find(primPath) != _selectionHighlightMirrorsByPrim.end());

    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByPrim[primPath]) {
        _DecrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
    }

    _selectionHighlightUsersByPrim[primPath].erase(userPath);
    if (_selectionHighlightUsersByPrim[primPath].empty()) {
        _selectionHighlightUsersByPrim.erase(primPath);
        _selectionHighlightMirrorsByPrim.erase(primPath);
    }
}

void
WireframeSelectionHighlightSceneIndex::_RebuildSelectionHighlight(const PXR_NS::SdfPath& primPath)
{
    auto primType = GetInputSceneIndex()->GetPrim(primPath).primType;
    TF_AXIOM(primType == HdPrimTypeTokens->instancer || primType == HdPrimTypeTokens->mesh);
    TF_AXIOM(_selectionHighlightUsersByPrim.find(primPath) != _selectionHighlightUsersByPrim.end());
    TF_AXIOM(_selectionHighlightMirrorsByPrim.find(primPath) != _selectionHighlightMirrorsByPrim.end());

    auto selectionHighlightUsers = _selectionHighlightUsersByPrim[primPath];
    
    for (const auto& selectionHighlightUser : selectionHighlightUsers) {
        _RemoveSelectionHighlightUser(primPath, selectionHighlightUser);
    }

    for (const auto& selectionHighlightUser : selectionHighlightUsers) {
        _AddSelectionHighlightUser(primPath, selectionHighlightUser);
    }
}

void
WireframeSelectionHighlightSceneIndex::_DeleteSelectionHighlight(const PXR_NS::SdfPath& primPath)
{
    TF_AXIOM(_selectionHighlightUsersByPrim.find(primPath) != _selectionHighlightUsersByPrim.end());
    TF_AXIOM(_selectionHighlightMirrorsByPrim.find(primPath) != _selectionHighlightMirrorsByPrim.end());

    auto selectionHighlightUsers = _selectionHighlightUsersByPrim[primPath];
    for (const auto& selectionHighlightUser : selectionHighlightUsers) {
        _RemoveSelectionHighlightUser(primPath, selectionHighlightUser);
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightsForInstancer(const HdSceneIndexPrim& instancerPrim, const SdfPath& instancerPath)
{
    auto roots = _GetHierarchyRoots(instancerPrim);
    for (const auto& root : roots) {
        // Ancestors include the instancer itself
        auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(instancerPath, root);
        for (const auto& selectedAncestor : selectedAncestors) {
            _AddSelectionHighlightUser(instancerPath, selectedAncestor);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightsForMesh(const HdSceneIndexPrim& meshPrim, const SdfPath& meshPath)
{
    auto roots = _GetHierarchyRoots(meshPrim);
    for (const auto& root : roots) {
        // Ancestors include the mesh itself
        auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(meshPath, root);
        for (const auto& selectedAncestor : selectedAncestors) {
            _AddSelectionHighlightUser(meshPath, selectedAncestor);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightsForGeomSubset(const SdfPath& geomSubsetPath)
{
    if (_selection->IsFullySelected(geomSubsetPath)) {
        _AddSelectionHighlightUser(geomSubsetPath.GetParentPath(), geomSubsetPath);
    }
}

}
