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

const std::string shMirrorTag = "_SHMirror";

SdfPath _GetSelectionHighlightMirrorPathFromOriginal(const SdfPath& originalPath)
{
    return originalPath.GetParentPath().AppendElementString(originalPath.GetElementString() + shMirrorTag);
}

SdfPath _GetOriginalPathFromSelectionHighlightMirror(const SdfPath& mirrorPath)
{
    return mirrorPath.GetParentPath().AppendElementString(mirrorPath.GetElementString().substr(0, mirrorPath.GetElementString().size() - shMirrorTag.size()));
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
        // See something.cpp for comment mentioning that currently only fully selected is supported.
        if (!selection.GetFullySelected() || !selection.GetFullySelected()->GetTypedValue(0)) {
            continue;
        }
        if (!selection.GetNestedInstanceIndices()) {
            // We have a selection that has no instances, which means the whole instancer is selected :
            // this overrides any individual instance selection.
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
        HdDataSourceLocator maskLocator(TfToken("instancerTopology"), TfToken("mask"));
        VtBoolArray selectionHighlightMask = _GetSelectionHighlightMask(instancerTopology, selections);
        auto selectionHighlightMaskDataSource = HdRetainedTypedSampledDataSource<VtBoolArray>::New(selectionHighlightMask);
        editedDataSource.Set(maskLocator, selectionHighlightMaskDataSource);
    }

    editedDataSource.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());

    return editedDataSource.Finish();
}

bool _IsInstancerWithSelections(const HdSceneIndexPrim& prim)
{
    if (prim.primType != HdPrimTypeTokens->instancer) {
        return false;
    }
    auto selections = HdSelectionsSchema::GetFromParent(prim.dataSource);
    return selections.IsDefined() && selections.GetNumElements() > 0;
}

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

// ----------------------------------------------------------------------------

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

// ----------------------------------------------------------------------------

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

SdfPath
WireframeSelectionHighlightSceneIndex::_FindSelectionHighlightMirrorAncestor(const SdfPath& path) const
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        if (_shMirrorsUseCount.find(ancestor) != _shMirrorsUseCount.end()) {
            return ancestor;
        }
    }
    return SdfPath::EmptyPath();
}

SdfPath
WireframeSelectionHighlightSceneIndex::GetSelectionHighlightPath(const SdfPath& path) const
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        const SdfPath mirrorPath = _GetSelectionHighlightMirrorPathFromOriginal(ancestor);
        if (_shMirrorsUseCount.find(mirrorPath) != _shMirrorsUseCount.end()) {
            return path.ReplacePrefix(ancestor, mirrorPath);
        }
    }
    return path;
}

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

SdfPathVector
WireframeSelectionHighlightSceneIndex::_CollectAffectedOriginalPrimPaths(const SdfPath& originalPrimPath)
{
    TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);
    SdfPathVector affectedOriginalPrimPaths;

    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(originalPrim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(originalPrim.dataSource);
    HdSelectionsSchema selections = HdSelectionsSchema::GetFromParent(originalPrim.dataSource);

    if (instancerTopology.IsDefined()) {
        auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
        for (const auto& protoPath : protoPaths) {
            affectedOriginalPrimPaths.push_back(protoPath);
        }
    }

    if (instancedBy.IsDefined()) {
        auto instancerPaths = instancedBy.GetPaths()->GetTypedValue(0);
        for (const auto& instancerPath : instancerPaths) {
            affectedOriginalPrimPaths.push_back(instancerPath);
        }

        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            affectedOriginalPrimPaths.push_back(protoRootPath);
        }
    }

    return affectedOriginalPrimPaths;
}

bool
WireframeSelectionHighlightSceneIndex::_IsPrototypeRoot(const SdfPath& primPath)
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (instancedBy.IsDefined()) {
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            if (protoRootPath == primPath) {
                return true;
            }
        }
        return false;
    }
    return true;
}

bool
WireframeSelectionHighlightSceneIndex::_IsInstancingRoot(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
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

bool
WireframeSelectionHighlightSceneIndex::_IsPropagatedPrototype(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() && !instancedBy.GetPrototypeRoots()->GetTypedValue(0).empty();
}

bool
WireframeSelectionHighlightSceneIndex::_IsPrototype(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined();
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

    std::stack<SdfPath> primPathsToTraverse({ SdfPath::AbsoluteRootPath() });
    while (!primPathsToTraverse.empty()) {
        SdfPath currPrimPath = primPathsToTraverse.top();
        primPathsToTraverse.pop();

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPrimPath);
        if (_IsInstancerWithSelections(currPrim)) {
            _CreateShMirrorsForInstancer(currPrimPath);
        }

        for (const auto& childPath : inputSceneIndex->GetChildPrimPaths(currPrimPath)) {
            primPathsToTraverse.push(childPath);
        }
    }
}

bool
WireframeSelectionHighlightSceneIndex::_CollectShMirrorPaths(const PXR_NS::SdfPath& originalPrimPath, PXR_NS::SdfPathSet& outShMirrorPaths, PXR_NS::HdSceneIndexObserver::AddedPrimEntries& outCreatedPrims)
{
    // This should never be called on selection highlight mirrors, only on original prims
    TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    static std::mutex printMutex;
    {
        std::lock_guard printLock(printMutex);
        std::cout << "Processing : " << originalPrimPath.GetString() << std::endl;
    }

    if (!_IsInstancingRoot(originalPrimPath)) {
        HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(originalPrim.dataSource);
        bool result = false;
        if (instancedBy.IsDefined()) {
            auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
            for (const auto& protoRootPath : protoRootPaths) {
                result = result || _CollectShMirrorPaths(protoRootPath, outShMirrorPaths, outCreatedPrims);
            }
        }
        return result;
    }
    
    SdfPath selectionHighlightPrimPath = _GetSelectionHighlightMirrorPathFromOriginal(originalPrimPath);
    
    if (outShMirrorPaths.find(selectionHighlightPrimPath) != outShMirrorPaths.end()) {
        return false;
    }
    outShMirrorPaths.insert(selectionHighlightPrimPath);

    SdfPathVector affectedOriginalPrimPaths;
    std::stack<SdfPath> pathsToTraverse({originalPrimPath});
    while (!pathsToTraverse.empty()) {
        SdfPath currPath = pathsToTraverse.top();
        pathsToTraverse.pop();

        // TODO : Propagated protos only or all protos?
        if (_IsPropagatedPrototype(currPath) && _IsInstancingRoot(currPath) && currPath != originalPrimPath) {
            continue;
        }

        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);
        outCreatedPrims.push_back({currPath.ReplacePrefix(originalPrimPath, selectionHighlightPrimPath), currPrim.primType});
        if (currPrim.primType == HdPrimTypeTokens->instancer) {
            SdfPathVector extraAffectedOriginalPrimPaths = _CollectAffectedOriginalPrimPaths(currPath);
            std::cout << "Processing extra paths for : " << currPath.GetString() << std::endl;
            for (const auto& path : extraAffectedOriginalPrimPaths) {
                {
                    std::lock_guard printLock(printMutex);
                    std::cout << "|-----Adding : " << path.GetString() << std::endl;
                }
            }
            affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), extraAffectedOriginalPrimPaths.begin(), extraAffectedOriginalPrimPaths.end());
        }

        for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
            pathsToTraverse.push(childPath);
        }
    }

    for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
        _CollectShMirrorPaths(affectedOriginalPrimPath, outShMirrorPaths, outCreatedPrims);
    }

    return true;
}

void 
WireframeSelectionHighlightSceneIndex::_RemoveShMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);

    HdSceneIndexObserver::RemovedPrimEntries removedPrims;

    SdfPathSet oldShMirrors;
    if (_shMirrorsByInstancer.find(instancerPath) != _shMirrorsByInstancer.end()) {
        oldShMirrors = _shMirrorsByInstancer.at(instancerPath);
    }
    for (const auto& shMirror : oldShMirrors) {
        TF_AXIOM(_shMirrorsUseCount.find(shMirror) != _shMirrorsUseCount.end());
        TF_AXIOM(_shMirrorsUseCount[shMirror] > 0);
        _shMirrorsUseCount[shMirror]--;
        if (_shMirrorsUseCount[shMirror] == 0) {
            _shMirrorsUseCount.erase(shMirror);
            removedPrims.push_back(shMirror);
        }
    }

    _shMirrorsByInstancer.erase(instancerPath);
    
    if (!removedPrims.empty()) {
        _SendPrimsRemoved(removedPrims);
    }
}

void 
WireframeSelectionHighlightSceneIndex::_CreateShMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath)
{
    TF_AXIOM(GetInputSceneIndex()->GetPrim(instancerPath).primType == HdPrimTypeTokens->instancer);

    if (_shMirrorsByInstancer.find(instancerPath) != _shMirrorsByInstancer.end()) {
        _RemoveShMirrorsForInstancer(instancerPath);
    }

    SdfPathSet newShMirrors;
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    _CollectShMirrorPaths(instancerPath, newShMirrors, addedPrims);

    _shMirrorsByInstancer[instancerPath] = newShMirrors;
    for (const auto& shMirror : newShMirrors) {
        if (_shMirrorsUseCount.find(shMirror) == _shMirrorsUseCount.end()) {
            _shMirrorsUseCount[shMirror] = 1;
        } else {
            TF_AXIOM(_shMirrorsUseCount.find(shMirror) != _shMirrorsUseCount.end());
            TF_AXIOM(_shMirrorsUseCount[shMirror] > 0);
            _shMirrorsUseCount[shMirror]++;
        }
    }

    if (!addedPrims.empty()) {
        _SendPrimsAdded(addedPrims);
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
    
    SdfPath shMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!shMirrorAncestor.IsEmpty()) {
        SdfPath originalPrimPath = primPath.ReplacePrefix(shMirrorAncestor, _GetOriginalPathFromSelectionHighlightMirror(shMirrorAncestor));
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPrimPath);
        if (prim.dataSource) {
            prim.dataSource = _SelectionHighlightRepathingContainerDataSource::New(prim.dataSource, this);
        }
        if (prim.primType == HdPrimTypeTokens->mesh) {
            prim.dataSource = _HighlightSelectedPrim(prim.dataSource, originalPrimPath, sRefinedWireDisplayStyleDataSource);
        }
        if (prim.primType == HdPrimTypeTokens->instancer) {
            prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource);
        }
        return prim;
    }
    
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    if (prim.primType == HdPrimTypeTokens->mesh) {
        // Note : in the USD data model, the original prims that get propagated as prototypes have their original prim types erased.
        // Only the resulting propagated prototypes keep the original prim type. Presumably this is to avoid drawing the original
        // prim (even though it should already not be drawn due to being under an instancer, this is an additional safety? to confirm)
        if (_IsPrototype(primPath)) {
            if (_selection->IsFullySelected(primPath)) {
                prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
            }
            else if (_highlightedProtoPaths.find(primPath) != _highlightedProtoPaths.end()) {
                prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath, sRefinedWireOnSurfaceDisplayStyleDataSource);
            }
        } else {
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
    SdfPath shMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!shMirrorAncestor.IsEmpty()) {
        SdfPath originalShMirrorAncestor = _GetOriginalPathFromSelectionHighlightMirror(shMirrorAncestor);
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(shMirrorAncestor, originalShMirrorAncestor));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(originalShMirrorAncestor, shMirrorAncestor);
        }
        return childPaths;
    }

    auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector additionalChildPaths;
    for (const auto& childPath : childPaths) {
        SdfPath shPath = _GetSelectionHighlightMirrorPathFromOriginal(childPath);
        if (_shMirrorsUseCount.find(shPath) != _shMirrorsUseCount.end()) {
            additionalChildPaths.push_back(shPath);
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

    auto newEntries = entries;
    for (const auto& entry : entries) {
        // TODO : Check if instancer with selections, or if an instancee whose instancer(s) have selections?
        // Maybe only need new instancers with selections?
        // New proto subprim : handled automatically
        // New proto : means an instancer has changed -> should be reflected automatically?
        if (_IsInstancerWithSelections(GetInputSceneIndex()->GetPrim(entry.primPath))) {
            _CreateShMirrorsForInstancer(entry.primPath);
        }
    }
    _SendPrimsAdded(newEntries);
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
        // If the dirtied prim is excluded, don't provide selection
        // highlighting for it.
        if (! _IsExcluded(entry.primPath) && 
            entry.dirtyLocators.Contains(
                HdSelectionsSchema::GetDefaultLocator())) {
            TF_DEBUG(FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX)
                .Msg("    %s selections locator dirty.\n", entry.primPath.GetText());
            // All mesh prims recursively under the selection dirty prim have a
            // dirty wireframe selection highlight.
            _DirtySelectionHighlightRecursive(entry.primPath, &highlightEntries);
        }
    }

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedSelectedMeshProtos;
    for (const auto& entry : entries) {
        HdSceneIndexPrim entryPrim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (!entry.dirtyLocators.Contains(HdSelectionsSchema::GetDefaultLocator())) {
            continue;
        }
        auto ancestorsRange = entry.primPath.GetAncestorsRange();
        bool foundPropagatedProtos = false;
        for (const auto& ancestorPath : ancestorsRange) {
            HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(ancestorPath);
            UsdImagingUsdPrimInfoSchema usdPrimInfo = UsdImagingUsdPrimInfoSchema::GetFromParent(currPrim.dataSource);
            if (usdPrimInfo.IsDefined()) {
                auto propagatedProtosDataSource = usdPrimInfo.GetPiPropagatedPrototypes();
                if (propagatedProtosDataSource) {
                    auto propagatedProtoNames = propagatedProtosDataSource->GetNames();
                    for (const auto& propagatedProtoName : propagatedProtoNames) {
                        auto propagatedProtoPathDataSource = HdTypedSampledDataSource<SdfPath>::Cast(propagatedProtosDataSource->Get(propagatedProtoName));
                        if (propagatedProtoPathDataSource) {
                            SdfPath propagatedProtoPath = propagatedProtoPathDataSource->GetTypedValue(0);
                            SdfPath propagatedPrimPath = entry.primPath.ReplacePrefix(ancestorPath, propagatedProtoPath);
                            HdSceneIndexPrim propagatedPrim = GetInputSceneIndex()->GetPrim(propagatedPrimPath);
                            if (propagatedPrim.primType == HdPrimTypeTokens->mesh) {
                                auto newLocators = HdDataSourceLocatorSet(entry.dirtyLocators);
                                newLocators.append(HdLegacyDisplayStyleSchema::GetDefaultLocator());
                                dirtiedSelectedMeshProtos.push_back({propagatedPrimPath, newLocators});
                                if (HdSelectionsSchema::GetFromParent(entryPrim.dataSource).IsDefined()) {
                                    _highlightedProtoPaths.insert(propagatedPrimPath);
                                } else {
                                    _highlightedProtoPaths.erase(propagatedPrimPath);
                                }
                            }
                            foundPropagatedProtos = true;
                        }
                    }
                }
            }
            if (foundPropagatedProtos) {
                break;
            }
        }
    }
    if (!dirtiedSelectedMeshProtos.empty()) {
        _SendPrimsDirtied(dirtiedSelectedMeshProtos);
    }

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedSelectionHighlightPrims;
    for (const auto& entry : entries) {
        auto selectionHighlightPath = GetSelectionHighlightPath(entry.primPath);
        if (selectionHighlightPath != entry.primPath) {
            dirtiedSelectionHighlightPrims.emplace_back(selectionHighlightPath, entry.dirtyLocators);
        }
    }
    if (!dirtiedSelectionHighlightPrims.empty()) {
        _SendPrimsDirtied(dirtiedSelectionHighlightPrims);
    }

    HdSceneIndexObserver::AddedPrimEntries newSelectionHighlightPrims;
    HdSceneIndexObserver::RemovedPrimEntries removedSelectionHighlightPrims;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Contains(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsInstancerWithSelections(prim)) {
                _CreateShMirrorsForInstancer(entry.primPath);
            } else if (prim.primType == HdPrimTypeTokens->instancer) {
                _RemoveShMirrorsForInstancer(entry.primPath);
            }
            // TODO : Forward dirtiness to SH mirrors masks
        }
    }
    if (!newSelectionHighlightPrims.empty())
        _SendPrimsAdded(newSelectionHighlightPrims);
    if (!removedSelectionHighlightPrims.empty()) {
        _SendPrimsRemoved(removedSelectionHighlightPrims);
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

    for (const auto& entry : entries) {
        // TODO : Check if instancer with selections, or if an instancee whose instancer(s) have selections?
        // Maybe only need new instancers with selections?
        // New proto subprim : handled automatically
        // New proto : means an instancer has changed -> should be reflected automatically?
        if (_IsInstancerWithSelections(GetInputSceneIndex()->GetPrim(entry.primPath))) {
            _RemoveShMirrorsForInstancer(entry.primPath);
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

}
