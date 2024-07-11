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
#include "fvpWireframeSelectionHighlightSceneIndex.h"

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
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
#include <pxr/usd/sdf/path.h>
#include <iostream>

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
    if (!TF_VERIFY(originalMask.empty() || originalMask.size() == nbInstances, "Instancer mask has incorrect size.")) {
        return originalMask;
    }
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
            _CreateInstancerHighlightsForInstancer(prim, primPath);
        } else if (prim.primType == HdPrimTypeTokens->mesh) {
            _CreateInstancerHighlightsForMesh(prim, primPath);
        } else if (prim.primType == HdPrimTypeTokens->geomSubset) {
            _CreateInstancerHighlightsForGeomSubset(primPath);
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
                selectionHighlightPrim.dataSource = _TrimMeshForSelectedGeomSubsets(selectionHighlightPrim.dataSource, originalPrimPath);
            } else if (selectionHighlightPrim.primType == HdPrimTypeTokens->geomSubset) {
                // If we returned the geomSubset prims unchanged, they could contain face indices that exceed
                // the trimmed mesh's number of faces, which prints a warning. We don't need the geomSubset
                // highlight mirrors anyways, so just return nothing.
                selectionHighlightPrim.dataSource = {};
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
        // If there no selected geomSubsets, don't trim the mesh.
        return originalDataSource;
    }

    // Edit the mesh topology
    HdContainerDataSourceEditor dataSourceEditor = HdContainerDataSourceEditor(originalDataSource);
    VtArray<int> originalFaceVertexCounts = meshTopologySchema.GetFaceVertexCounts()->GetTypedValue(0);
    VtArray<int> originalFaceVertexIndices = meshTopologySchema.GetFaceVertexIndices()->GetTypedValue(0);
    VtArray<int> trimmedFaceVertexCounts;
    VtArray<int> trimmedFaceVertexIndices;
    size_t iFaceCounts = 0;
    size_t iFaceIndices = 0;
    while (iFaceCounts < originalFaceVertexCounts.size() && iFaceIndices < originalFaceVertexIndices.size()) {
        int currFaceCount = originalFaceVertexCounts[iFaceCounts];

        if (faceIndicesToKeep.find(iFaceCounts) != faceIndicesToKeep.end()) {
            trimmedFaceVertexCounts.push_back(currFaceCount);
            for (size_t faceIndicesOffset = 0; faceIndicesOffset < currFaceCount; faceIndicesOffset++) {
                trimmedFaceVertexIndices.push_back(originalFaceVertexIndices[iFaceIndices + faceIndicesOffset]);
            }
        }

        iFaceCounts++;
        iFaceIndices += currFaceCount;
    }
    auto faceVertexCountsLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexCounts);
    auto faceVertexIndicesLocator = HdMeshTopologySchema::GetDefaultLocator().Append(HdMeshTopologySchemaTokens->faceVertexIndices);
    
    dataSourceEditor.Set(faceVertexCountsLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexCounts));
    dataSourceEditor.Set(faceVertexIndicesLocator, HdRetainedTypedSampledDataSource<VtIntArray>::New(trimmedFaceVertexIndices));

    return dataSourceEditor.Finish();
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
        } else if (prim.primType == HdPrimTypeTokens->mesh) {
            _CreateInstancerHighlightsForMesh(prim, entry.primPath);
        } else if (prim.primType == HdPrimTypeTokens->geomSubset) {
            _CreateInstancerHighlightsForGeomSubset(entry.primPath);
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
            // We do not need to check for instancedBy dirtying. If an instancedBy data source is dirtied,
            // then either :
            // 1) A new instancer was added : this is handled in _PrimsAdded.
            // 2) An existing instancer's instancerTopology data source was dirtied : this is handled here.
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

            // If a geomSubset's selection changes, dirty the selection highlight mesh to trim it appropriately.
            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                SdfPath meshPath = entry.primPath.GetParentPath();
                SdfPath selectionHighlightMeshPath = GetSelectionHighlightPath(meshPath);
                if (selectionHighlightMeshPath != meshPath) {
                    dirtiedPrims.emplace_back(selectionHighlightMeshPath, HdDataSourceLocator(HdMeshSchemaTokens->mesh, HdMeshSchemaTokens->topology));
                }
            }
            
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            _DirtySelectionHighlightRecursive(entry.primPath, &dirtiedPrims);
            if (selectionHighlightPath != entry.primPath) {
                _DirtySelectionHighlightRecursive(selectionHighlightPath, &dirtiedPrims);
            }

            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            bool isSelected = selectionsSchema.IsDefined() && selectionsSchema.GetNumElements() > 0;

            if (prim.primType == HdPrimTypeTokens->geomSubset) {
                if (isSelected) {
                    instancerHighlightUsersToAdd.push_back({entry.primPath.GetParentPath(), entry.primPath});
                } else {
                    instancerHighlightUsersToRemove.push_back({entry.primPath.GetParentPath(), entry.primPath});
                }
            }

            // Update child instancer highlights for ancestor-based selection highlighting
            // (i.e. selecting one or more of an instancer's parents should highlight the instancer)
            auto operation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                if (prim.primType == HdPrimTypeTokens->instancer || prim.primType == HdPrimTypeTokens->mesh) {
                    if (isSelected) {
                        instancerHighlightUsersToAdd.push_back({primPath,entry.primPath});
                    } else {
                        instancerHighlightUsersToRemove.push_back({primPath,entry.primPath});
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
WireframeSelectionHighlightSceneIndex::_CollectSelectionHighlightMirrors(const PXR_NS::SdfPath& originalPrimPath, PXR_NS::SdfPathSet& outSelectionHighlightMirrors, PXR_NS::HdSceneIndexObserver::AddedPrimEntries& outAddedPrims)
{
    // This should never be called on selection highlight prims, only on original prims
    TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths of the instancer will be
    // processed as part of the children traversal later down this method.
    if (_IsPrototypeSubPrim(originalPrim, originalPrimPath)) {
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
        if (prim.primType == HdPrimTypeTokens->instancer || prim.primType == HdPrimTypeTokens->mesh) {
            SdfPathVector instancingRelatedPaths = _GetInstancingRelatedPaths(prim);
            affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), instancingRelatedPaths.begin(), instancingRelatedPaths.end());
            // We hit an instancing-related prim, don't process its children (nested instancing will be processed through the instancing-related paths).
            return false;
        }
        return true;
    };
    _ForEachPrimInHierarchy(originalPrimPath, operation);

    for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
        _CollectSelectionHighlightMirrors(affectedOriginalPrimPath, outSelectionHighlightMirrors, outAddedPrims);
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
WireframeSelectionHighlightSceneIndex::_AddInstancerHighlightUser(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
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
            _IncrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
        }

        if (!addedPrims.empty()) {
            _SendPrimsAdded(addedPrims);
        }
    }
    else {
        for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
            _IncrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_RemoveInstancerHighlightUser(const PXR_NS::SdfPath& instancerPath, const SdfPath& userPath)
{
    TF_AXIOM(_instancerHighlightUsersByInstancer.find(instancerPath) != _instancerHighlightUsersByInstancer.end());
    TF_AXIOM(_instancerHighlightUsersByInstancer.at(instancerPath).find(userPath) != _instancerHighlightUsersByInstancer.at(instancerPath).end());
    TF_AXIOM(_selectionHighlightMirrorsByInstancer.find(instancerPath) != _selectionHighlightMirrorsByInstancer.end());

    for (const auto& selectionHighlightMirror : _selectionHighlightMirrorsByInstancer[instancerPath]) {
        _DecrementSelectionHighlightMirrorUseCounter(selectionHighlightMirror);
    }

    _instancerHighlightUsersByInstancer[instancerPath].erase(userPath);
    if (_instancerHighlightUsersByInstancer[instancerPath].empty()) {
        _instancerHighlightUsersByInstancer.erase(instancerPath);
        _selectionHighlightMirrorsByInstancer.erase(instancerPath);
    }
}

void
WireframeSelectionHighlightSceneIndex::_RebuildInstancerHighlight(const PXR_NS::SdfPath& instancerPath)
{
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
    auto roots = _GetHierarchyRoots(instancerPrim);
    for (const auto& root : roots) {
        // Ancestors include the instancer itself
        auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(instancerPath, root);
        for (const auto& selectedAncestor : selectedAncestors) {
            _AddInstancerHighlightUser(instancerPath, selectedAncestor);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateInstancerHighlightsForMesh(const HdSceneIndexPrim& meshPrim, const SdfPath& meshPath)
{
    auto roots = _GetHierarchyRoots(meshPrim);
    for (const auto& root : roots) {
        // Ancestors include the instancer itself
        auto selectedAncestors = _selection->FindFullySelectedAncestorsInclusive(meshPath, root);
        for (const auto& selectedAncestor : selectedAncestors) {
            _AddInstancerHighlightUser(meshPath, selectedAncestor);
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateInstancerHighlightsForGeomSubset(const SdfPath& geomSubsetPath)
{
    if (_selection->IsFullySelected(geomSubsetPath)) {
        _AddInstancerHighlightUser(geomSubsetPath.GetParentPath(), geomSubsetPath);
    }
}

}
