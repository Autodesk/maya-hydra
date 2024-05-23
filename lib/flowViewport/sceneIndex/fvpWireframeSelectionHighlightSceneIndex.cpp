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

#include <pxr/base/gf/matrix4d.h>
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

const HdRetainedContainerDataSourceHandle sRefinedWireOnSurfaceDisplayStyleDataSource
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

const std::string testInstancer = "ParentPointInstancer";

const std::string shMirrorTag = "_SHMirror";

class SelectionHighlightDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(SelectionHighlightDataSource);

    SelectionHighlightDataSource(const HdContainerDataSourceHandle& originalDataSource) : _originalDataSource(originalDataSource)
    {
    }

    TfTokenVector GetNames() override {
        TfTokenVector names = _originalDataSource->GetNames();
        /*names.erase(std::remove_if(names.begin(), names.end(), [](const TfToken& token) -> bool {
            return token == HdSelectionsSchemaTokens->selections;
        }));*/
        return names;
    }

    HdDataSourceBaseHandle Get(const TfToken &name) override
    {
        return _originalDataSource->Get(name);
    }

private:
    HdContainerDataSourceHandle _originalDataSource;
};

bool _IsSelectionHighlightMirrorPath(const SdfPath& path)
{
    return path.GetElementString().find(shMirrorTag) != std::string::npos;
}

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

HdContainerDataSourceHandle _CreateSelectionHighlightInstancer(const SdfPath& originalPath, const HdContainerDataSourceHandle& originalDataSource)
{

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

    // HdDataSourceLocator prototypesLocator(TfToken("instancerTopology"), TfToken("prototypes"));
    // VtArray<SdfPath> originalPrototypes = instancerTopology.GetPrototypes()->GetTypedValue(0);
    // VtArray<SdfPath> selectionHighlightPrototypes = originalPrototypes;
    // for (auto& shProtoPath : selectionHighlightPrototypes) {
    //     shProtoPath = _GetSelectionHighlightMirrorPathFromOriginal(shProtoPath);
    // }
    // editedDataSource.Set(prototypesLocator, HdRetainedTypedSampledDataSource<VtArray<SdfPath>>::New(selectionHighlightPrototypes));

    editedDataSource.Set(HdSelectionsSchema::GetDefaultLocator(), HdBlockDataSource::New());

    return editedDataSource.Finish();
}



// HdContainerDataSourceHandle _FilterInstances(const HdContainerDataSourceHandle& dataSource)
// {
//     auto edited = HdContainerDataSourceEditor(dataSource);
//     HdDataSourceLocator maskLocator(TfToken("instancerTopology"), TfToken("mask"));
//     auto replacementDataSource = HdRetainedTypedSampledDataSource<VtBoolArray>::New(VtBoolArray({true, false, true, false, true, true, false, false}));
//     edited.Set(maskLocator, replacementDataSource);

    
//     // //Is the prim in refined displayStyle (meaning shaded) ?
//     // if (HdLegacyDisplayStyleSchema styleSchema =
//     //         HdLegacyDisplayStyleSchema::GetFromParent(dataSource)) {

//     //     if (HdTokenArrayDataSourceHandle ds =
//     //             styleSchema.GetReprSelector()) {
//     //         VtArray<TfToken> ar = ds->GetTypedValue(0.0f);
//     //         TfToken refinedToken = ar[0];
//     //         if(HdReprTokens->refined == refinedToken){
//     //             //Is in refined display style, apply the wire on top of shaded reprselector
//     //             return HdOverlayContainerDataSource::New({ edited.Finish(), sRefinedWireOnSurfaceDisplayStyleDataSource});
//     //         }
//     //     }else{
//     //         //No reprSelector found, assume it's in the Collection that we have set HdReprTokens->refined
//     //         return HdOverlayContainerDataSource::New({ edited.Finish(), sRefinedWireOnSurfaceDisplayStyleDataSource});
//     //     }
//     // }

//     //For the other case, we are only updating the wireframe color assuming we are already drawing lines
//     return edited.Finish();
// }

bool _IsInstancerWithSelections(const HdSceneIndexPrim& prim)
{
    if (prim.primType != HdPrimTypeTokens->instancer) {
        return false;
    }
    auto selections = HdSelectionsSchema::GetFromParent(prim.dataSource);
    return selections.IsDefined() && selections.GetNumElements() > 0;
}

bool _IsConcretePrototype(const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots();
}

bool _IsPartOfSelectionHighlightMirror(const SdfPath& path)
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        if (_IsSelectionHighlightMirrorPath(ancestor)) {
            return true;
        }
    }
    return false;
}

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
WireframeSelectionHighlightSceneIndex::_RepathToSelectionHighlightMirror(const SdfPath& path) const
{
    auto ancestorsRange = path.GetAncestorsRange();
    for (auto ancestor : ancestorsRange) {
        const SdfPath mirrorPath = _GetSelectionHighlightMirrorPathFromOriginal(ancestor);
        if (_shMirrorsUseCount.find(mirrorPath) != _shMirrorsUseCount.end()) {
            return path.ReplacePrefix(ancestor, mirrorPath);
        }
    }
    return SdfPath::EmptyPath();
}

class WireframeSelectionHighlightSceneIndex::_RerootingSceneIndexPathDataSource : public HdPathDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathDataSource)

    _RerootingSceneIndexPathDataSource(
        HdPathDataSourceHandle const &inputDataSource,
        WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
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

        return _inputDataSource->GetContributingSampleTimesForInterval(
                startTime, endTime, outSampleTimes);
    }

    SdfPath GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return SdfPath();
        }

        const SdfPath srcPath = _inputDataSource->GetTypedValue(shutterOffset);
        const SdfPath mirrorPath =_inputSceneIndex->_RepathToSelectionHighlightMirror(srcPath);
        if (!mirrorPath.IsEmpty()) {
            return mirrorPath;
        }
        // for (const auto& shSceneIndex : _inputSceneIndex->_shSceneIndices) {
        //     const SdfPath originalShSceneIndexPath = _GetOriginalPathFromSelectionHighlightMirror(shSceneIndex.first);
        //     if (srcPath.HasPrefix(originalShSceneIndexPath)) {
        //         return srcPath.ReplacePrefix(originalShSceneIndexPath, shSceneIndex.first);
        //     }
        // }
        return srcPath;
    }

private:
    HdPathDataSourceHandle const _inputDataSource;
    WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

// ----------------------------------------------------------------------------

class WireframeSelectionHighlightSceneIndex::_RerootingSceneIndexPathArrayDataSource : public HdPathArrayDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathArrayDataSource)

    _RerootingSceneIndexPathArrayDataSource(
        HdPathArrayDataSourceHandle const & inputDataSource,
        WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
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

        return _inputDataSource->GetContributingSampleTimesForInterval(
            startTime, endTime, outSampleTimes);
    }

    VtArray<SdfPath> GetTypedValue(const Time shutterOffset) override
    {
        if (!_inputDataSource) {
            return {};
        }

        VtArray<SdfPath> result
            = _inputDataSource->GetTypedValue(shutterOffset);

        for (size_t i = 0; i < result.size(); i++) {
            const SdfPath mirrorPath = _inputSceneIndex->_RepathToSelectionHighlightMirror(result[i]);
            if (!mirrorPath.IsEmpty()) {
                result[i] = mirrorPath;
            }
            // for (const auto& shSceneIndex : _inputSceneIndex->_shSceneIndices) {
            //     const SdfPath originalShSceneIndexPath = _GetOriginalPathFromSelectionHighlightMirror(shSceneIndex.first);
            //     if (result[i].HasPrefix(originalShSceneIndexPath)) {
            //         result[i] = result[i].ReplacePrefix(originalShSceneIndexPath, shSceneIndex.first);
            //     }
            // }
        }

        return result;
    }

private:
    HdPathArrayDataSourceHandle const _inputDataSource;
    WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

// ----------------------------------------------------------------------------

class WireframeSelectionHighlightSceneIndex::_RerootingSceneIndexContainerDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexContainerDataSource)

    _RerootingSceneIndexContainerDataSource(
        HdContainerDataSourceHandle const &inputDataSource,
        WireframeSelectionHighlightSceneIndex const * const inputSceneIndex)
      : _inputDataSource(inputDataSource),
        _inputSceneIndex(inputSceneIndex)
    {
    }

    TfTokenVector GetNames() override
    {
        if (!_inputDataSource) {
            return {};
        }

        return _inputDataSource->GetNames();
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (!_inputDataSource) {
            return nullptr;
        }

        // wrap child containers so that we can wrap their children
        HdDataSourceBaseHandle const childSource = _inputDataSource->Get(name);
        if (!childSource) {
            return nullptr;
        }

        if (auto childContainer =
                HdContainerDataSource::Cast(childSource)) {
            return New(std::move(childContainer), _inputSceneIndex);
        }

        if (auto childPathDataSource =
                HdTypedSampledDataSource<SdfPath>::Cast(childSource)) {
            return _RerootingSceneIndexPathDataSource::New(childPathDataSource, _inputSceneIndex);
        }

        if (auto childPathArrayDataSource =
                HdTypedSampledDataSource<VtArray<SdfPath>>::Cast(
                    childSource)) {
            return _RerootingSceneIndexPathArrayDataSource::New(childPathArrayDataSource, _inputSceneIndex);
        }

        return childSource;
    }

private:
    HdContainerDataSourceHandle const _inputDataSource;
    WireframeSelectionHighlightSceneIndex const * const _inputSceneIndex;
};

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
WireframeSelectionHighlightSceneIndex::_IsInstancingRoot(const SdfPath& primPath)
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
WireframeSelectionHighlightSceneIndex::_IsPropagatedPrototype(const SdfPath& primPath)
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() && !instancedBy.GetPrototypeRoots()->GetTypedValue(0).empty();
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightMirror(const SdfPath& originalPrimPath, HdSceneIndexObserver::AddedPrimEntries& createdPrims)
{
    // This should never be called on selection highlight mirrors, only on original prims
    // TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    // if (!_IsPrototypeRoot(originalPrimPath)) {
    //     HdSceneIndexPrim originalPrim = GetInputSceneIndex()->GetPrim(originalPrimPath);
    //     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(originalPrim.dataSource);
    //     if (instancedBy.IsDefined()) {
    //         auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
    //         for (const auto& protoRootPath : protoRootPaths) {
    //             _CreateSelectionHighlightMirror(protoRootPath, createdPrims);
    //         }
    //     }
    //     return;
    // }
    
    // SdfPath selectionHighlightPrimPath = _GetSelectionHighlightMirrorPathFromOriginal(originalPrimPath);
    
    // bool isNewShMirror = _shMirrorPathsUseCount.find(selectionHighlightPrimPath) == _shMirrorPathsUseCount.end();
    // if (isNewShMirror) {
    //     TF_AXIOM(_shMirrorPathsUseCount.find(selectionHighlightPrimPath) == _shMirrorPathsUseCount.end());
    //     _shMirrorPathsUseCount[selectionHighlightPrimPath] = 1;
    //     TF_AXIOM(_shSceneIndices.find(selectionHighlightPrimPath) == _shSceneIndices.end());
    //     _shSceneIndices[selectionHighlightPrimPath] = UsdImagingRerootingSceneIndex::New(GetInputSceneIndex(), originalPrimPath, selectionHighlightPrimPath);
    //     createdPrims.push_back({selectionHighlightPrimPath, GetInputSceneIndex()->GetPrim(originalPrimPath).primType});
    // }
    // else {
    //     TF_AXIOM(_shMirrorPathsUseCount.find(selectionHighlightPrimPath) != _shMirrorPathsUseCount.end());
    //     TF_AXIOM(_shMirrorPathsUseCount[selectionHighlightPrimPath] > 0);
    //     TF_AXIOM(_shSceneIndices.find(selectionHighlightPrimPath) != _shSceneIndices.end());
    //     _shMirrorPathsUseCount[selectionHighlightPrimPath]++;
    // }
    // TF_AXIOM(_shMirrorPathsUseCount[selectionHighlightPrimPath] > 0);

    // SdfPathVector affectedOriginalPrimPaths = _CollectAffectedOriginalPrimPaths(originalPrimPath);

    // std::stack<SdfPath> childPaths;
    // for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(originalPrimPath)) {
    //     childPaths.push(childPath);
    // }
    // while (!childPaths.empty()) {
    //     SdfPath currPath = childPaths.top();
    //     childPaths.pop();

    //     HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);
    //     if (isNewShMirror) {
    //         createdPrims.push_back({currPath.ReplacePrefix(originalPrimPath, selectionHighlightPrimPath), currPrim.primType});
    //     }
    //     if (currPrim.primType == HdPrimTypeTokens->instancer) {
    //         SdfPathVector extraAffectedPaths = _CollectAffectedOriginalPrimPaths(currPath);
    //         affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), extraAffectedPaths.begin(), extraAffectedPaths.end());
    //     }

    //     for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
    //         childPaths.push(childPath);
    //     }
    // }

    // for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
    //     _CreateSelectionHighlightMirror(affectedOriginalPrimPath, createdPrims);
    // }
}

void
WireframeSelectionHighlightSceneIndex::_InvalidateSelectionHighlightMirror(const SdfPath& originalPrimPath, HdSceneIndexObserver::RemovedPrimEntries& removedPrims)
{
    // This should never be called on selection highlight mirrors, only on original prims
    // TF_AXIOM(_FindSelectionHighlightMirrorAncestor(originalPrimPath).IsEmpty());

    // if (!_IsPrototypeRoot(originalPrimPath)) {
    //     return;
    // }

    // SdfPath selectionHighlightPrimPath = _GetSelectionHighlightMirrorPathFromOriginal(originalPrimPath);

    // if (_shMirrorPathsUseCount.find(selectionHighlightPrimPath) == _shMirrorPathsUseCount.end()) {
    //     return;
    // }
    // TF_AXIOM(_shMirrorPathsUseCount.find(selectionHighlightPrimPath) != _shMirrorPathsUseCount.end());
    // TF_AXIOM(_shMirrorPathsUseCount[selectionHighlightPrimPath] > 0);
    // TF_AXIOM(_shSceneIndices.find(selectionHighlightPrimPath) != _shSceneIndices.end());

    // _shMirrorPathsUseCount[selectionHighlightPrimPath]--;
    // bool shouldDeleteShMirror = _shMirrorPathsUseCount[selectionHighlightPrimPath] == 0;
    // if (shouldDeleteShMirror) {
    //     _shMirrorPathsUseCount.erase(selectionHighlightPrimPath);
    //     _shSceneIndices.erase(selectionHighlightPrimPath);
    //     removedPrims.push_back({selectionHighlightPrimPath});
    // }
    
    // SdfPathVector affectedOriginalPrimPaths = _CollectAffectedOriginalPrimPaths(originalPrimPath);

    // std::stack<SdfPath> childPaths;
    // for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(originalPrimPath)) {
    //     childPaths.push(childPath);
    // }
    // while (!childPaths.empty()) {
    //     SdfPath currPath = childPaths.top();
    //     childPaths.pop();

    //     HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPath);
    //     if (currPrim.primType == HdPrimTypeTokens->instancer) {
    //         SdfPathVector extraAffectedPaths = _CollectAffectedOriginalPrimPaths(currPath);
    //         affectedOriginalPrimPaths.insert(affectedOriginalPrimPaths.end(), extraAffectedPaths.begin(), extraAffectedPaths.end());
    //     }

    //     for (const auto& childPath : GetInputSceneIndex()->GetChildPrimPaths(currPath)) {
    //         childPaths.push(childPath);
    //     }
    // }

    // for (const auto& affectedOriginalPrimPath : affectedOriginalPrimPaths) {
    //     _InvalidateSelectionHighlightMirror(affectedOriginalPrimPath, removedPrims);
    // }
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightMirrorForInstancer(const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    TF_AXIOM(instancerTopology.IsDefined());


    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (instancedBy.IsDefined()) {
        for (const auto& instancedByPath : instancedBy.GetPaths()->GetTypedValue(0)) {
            // TODO : GetPrototypeRoots? -> goes into overlay, same as new instancedBy paths
            _CreateSelectionHighlightMirrorForInstancer(instancedByPath, GetInputSceneIndex()->GetPrim(instancedByPath));
        }
    }
}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightMirrorForMesh(const SdfPath& primPath, const HdSceneIndexPrim& prim)
{

}

void
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightMirrorForArbitraryPrim(const SdfPath& primPath, const HdSceneIndexPrim& prim)
{

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

void 
WireframeSelectionHighlightSceneIndex::_CreateSelectionHighlightInstancer(const PXR_NS::SdfPath& originalPath, const PXR_NS::HdContainerDataSourceHandle& originalDataSource)
{

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

    // if (_IsSelectionHighlightMirrorPath(primPath)) {
    //     HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(_GetOriginalPathFromSelectionHighlightMirror(primPath));
    //     return {prim.primType, {}};
    // }

    // auto ancestorsRange = primPath.GetAncestorsRange();
    // for (auto ancestor : ancestorsRange) {
    //     if (_IsSelectionHighlightMirrorPath(ancestor)) {
    //         auto originalPath = primPath.ReplacePrefix(ancestor, _GetOriginalPathFromSelectionHighlightMirror(ancestor));
    //         HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    //         return {prim.primType, {}};
    //     }
    // }
    if (primPath.GetElementString() == "TopInstancer") {
        //return {};
    }
    static std::mutex printMutex;
    std::lock_guard printLock(printMutex);
    SdfPath shMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    //std::cout << "For " << primPath.GetString() << " , SH is : " << shMirrorAncestor.GetString() << std::endl;
    if (!shMirrorAncestor.IsEmpty()) {
        // TODO : Use _shSceneIndices?
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath.ReplacePrefix(shMirrorAncestor, _GetOriginalPathFromSelectionHighlightMirror(shMirrorAncestor)));
        if (prim.dataSource) {
            prim.dataSource = WireframeSelectionHighlightSceneIndex::_RerootingSceneIndexContainerDataSource::New(prim.dataSource, this);
        }
        if (prim.primType == HdPrimTypeTokens->mesh) {
            prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath.ReplacePrefix(shMirrorAncestor, _GetOriginalPathFromSelectionHighlightMirror(shMirrorAncestor)));
        }
        if (prim.primType == HdPrimTypeTokens->instancer) {
            prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource);
        }
        // if (prim.primType == HdPrimTypeTokens->instancer) {
        //     HdSelectionsSchema selections = HdSelectionsSchema::GetFromParent(prim.dataSource);
        //     if (selections.IsDefined()) {
        //         HdInstanceIndicesSchema::Builder instanceIndicesBuilder;
        //         instanceIndicesBuilder.SetInstancer(HdRetainedTypedSampledDataSource<SdfPath>::New(primPath));
        //         instanceIndicesBuilder.SetInstanceIndices(HdRetainedTypedSampledDataSource<VtArray<int>>::New({0}));
        //         instanceIndicesBuilder.SetPrototypeIndex(HdRetainedTypedSampledDataSource<int>::New(0));
        //         HdSelectionSchema::Builder selectionBuilder;
        //         selectionBuilder.SetFullySelected(HdRetainedTypedSampledDataSource<bool>::New(true));
        //         auto newInstanceIndices = HdDataSourceBase::Cast(instanceIndicesBuilder.Build());
        //         selectionBuilder.SetNestedInstanceIndices(HdRetainedSmallVectorDataSource::New(1, &newInstanceIndices));
        //         auto newSelection = HdDataSourceBase::Cast(selectionBuilder.Build());
        //         auto newSelections = HdRetainedSmallVectorDataSource::New(1, &newSelection);
        //         HdContainerDataSourceEditor editor(prim.dataSource);
        //         editor.Set(HdSelectionsSchema::GetDefaultLocator(), newSelections);

        //         editor.Set(HdInstancerTopologySchema::GetDefaultLocator().Append(TfToken("mask")), HdRetainedTypedSampledDataSource<VtArray<bool>>::New(VtArray<bool>({true, false})));
        //         prim.dataSource = editor.Finish();
        //     }
        // }
        return prim;
    }
    // TODO: 
    //else {
        // do as before with isancestorselected
    //}


    
    // for (const auto& shSceneIndex : _shSceneIndices) {
    //     if (primPath.HasPrefix(shSceneIndex.first)) {
    //         //HdSceneIndexPrim prim = shSceneIndex.second->GetPrim(primPath);
    //         HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath.ReplacePrefix(shSceneIndex.first, _GetOriginalPathFromSelectionHighlightMirror(shSceneIndex.first)));
    //         if (prim.dataSource) {
    //             prim.dataSource = WireframeSelectionHighlightSceneIndex::_RerootingSceneIndexContainerDataSource::New(prim.dataSource, this);
    //         }
    //         if (prim.primType == HdPrimTypeTokens->mesh) {
    //             prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath.ReplacePrefix(shSceneIndex.first, _GetOriginalPathFromSelectionHighlightMirror(shSceneIndex.first)));
    //         }
    //         return prim;
    //     }
    // }
    //HdSceneIndexPrim prim;
    //if (_IsSelectionHighlightMirrorPath(primPath)) {
    //    prim = GetInputSceneIndex()->GetPrim(_GetOriginalFromMirrorPath(primPath));
    //} else {
    //    prim = GetInputSceneIndex()->GetPrim(primPath);
    //}
//
    //if (_IsSelectionHighlightMirrorPath(primPath)) {
    //    if (prim.primType == HdPrimTypeTokens->instancer) {
    //        prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource);
    //    }
    //    else if (prim.primType == HdPrimTypeTokens->mesh) {
    //        prim.dataSource = _HighlightSelectedPrim(prim.dataSource, _GetOriginalFromMirrorPath(primPath));
    //    }
    //}

    // if (_selectionHighlightDataSourceOverlays.find(primPath) != _selectionHighlightDataSourceOverlays.end()) {
    //     prim.dataSource = HdOverlayContainerDataSource::New(_selectionHighlightDataSourceOverlays.at(primPath), prim.dataSource);
    //     return prim;
    // }

    // If prim is in excluded hierarchy, don't provide selection highlighting
    // for it.  Selection highlighting is done only on meshes.  HYDRA-569: this
    // means that HdsiImplicitSurfaceSceneIndex must be used before this scene
    // index to convert implicit surfaces (e.g. USD cube / cone / sphere /
    // capsule primitive types) to meshes.
    // if (! _IsExcluded(primPath) && prim.primType == HdPrimTypeTokens->mesh) {
    //     // TODO : Check that this is not an instanced mesh (check there is no instancedBy schema)
    //     if (_selection->HasFullySelectedAncestorInclusive(primPath)) {
    //         prim.dataSource = _HighlightSelectedPrim(prim.dataSource, primPath);
    //     }
    // }

    // if (!_IsSelectionHighlightMirror(primPath)) {
    //     return prim;
    // }

    // if (prim.primType == HdPrimTypeTokens->instancer) {
    //     prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource);
    // } else if (prim.primType == HdPrimTypeTokens->mesh) {
    //     SdfPath originalPath = _GetOriginalFromMirrorPath(primPath);
    //     prim.dataSource = _HighlightSelectedPrim(prim.dataSource, originalPath);
    // }

    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector
WireframeSelectionHighlightSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    // if (primPath.GetElementString().find("ForInstancer") != std::string::npos &&
    // GetInputSceneIndex()->GetPrim(primPath).primType == HdPrimTypeTokens->instancer) {
    //     return {};
    // }
    // if (primPath.GetElementString().find("ForInstancer") != std::string::npos &&
    // GetInputSceneIndex()->GetPrim(primPath).primType == HdPrimTypeTokens->instancer) {
    //     static std::mutex printMutex;
    //     SdfPath rerootedPath = primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), SdfPath("Potato"));
    //     std::lock_guard printLock(printMutex);
    //     std::cout << rerootedPath << std::endl;
    // }
    // auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    // if (_extraChildPaths.find(primPath) != _extraChildPaths.end()) {
    //     SdfPathVector extraChildPaths = _extraChildPaths.at(primPath);
    //     childPaths.insert(childPaths.end(), extraChildPaths.begin(), extraChildPaths.end());
    // }
    // SdfPathVector additionalChildPaths;
    // for (const auto& childPath : childPaths) {
    //     if (_originalPrimPathsToSelectionHighlightMirrorPrimPaths.find(childPath) != _originalPrimPathsToSelectionHighlightMirrorPrimPaths.end()) {
    //         additionalChildPaths.push_back(_originalPrimPathsToSelectionHighlightMirrorPrimPaths.at(childPath));
    //     }
    // }
    // childPaths.insert(childPaths.end(), additionalChildPaths.begin(), additionalChildPaths.end());
    // SdfPathVector additionalChildPaths;
    // for (auto& childPath : childPaths) {
    //     if (GetInputSceneIndex()->GetPrim(childPath).primType == HdPrimTypeTokens->instancer
    //     || GetInputSceneIndex()->GetPrim(childPath).primType == HdPrimTypeTokens->mesh) {
    //         SdfPath mirrorPath = _GetSelectionHighlightMirrorPath(childPath);
    //         additionalChildPaths.push_back(mirrorPath);
    //     }
    // }
    // childPaths.insert(childPaths.end(), additionalChildPaths.begin(), additionalChildPaths.end());
    
    //SdfPathVector additionalChildPaths;
    //for (const auto& childPath : childPaths) {
    //    if (_topLevelInstancers.find(childPath) != _topLevelInstancers.end()) {
    //        additionalChildPaths.push_back(_GetSelectionHighlightMirrorPath(childPath));
    //    }
    //}
    //childPaths.insert(childPaths.end(), additionalChildPaths.begin(), additionalChildPaths.end());
    // auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    // SdfPathVector additionalChildPaths;
    // for (const auto& childPath : childPaths) {
    //     HdSceneIndexPrim childPrim = GetInputSceneIndex()->GetPrim(childPath);
    //     HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(childPrim.dataSource);
    //     HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(childPrim.dataSource);

    // }
    SdfPath shMirrorAncestor = _FindSelectionHighlightMirrorAncestor(primPath);
    if (!shMirrorAncestor.IsEmpty()) {
        SdfPath originalShMirrorAncestor = _GetOriginalPathFromSelectionHighlightMirror(shMirrorAncestor);
        auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(shMirrorAncestor, originalShMirrorAncestor));
        for (auto& childPath : childPaths) {
            childPath = childPath.ReplacePrefix(originalShMirrorAncestor, shMirrorAncestor);
        }
        return childPaths;
    }
    // for (const auto& shSceneIndex : _shSceneIndices) {
    //     if (primPath.HasPrefix(shSceneIndex.first)) {
    //         //return shSceneIndex.second->GetChildPrimPaths(primPath);
    //         auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath.ReplacePrefix(shSceneIndex.first, _GetOriginalPathFromSelectionHighlightMirror(shSceneIndex.first)));
    //         for (auto& childPath : childPaths) {
    //             childPath = childPath.ReplacePrefix(const SdfPath &oldPrefix, const SdfPath &newPrefix)
    //         }
    //     }
    // }

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
HdContainerDataSourceHandle WireframeSelectionHighlightSceneIndex::_HighlightSelectedPrim(const HdContainerDataSourceHandle& dataSource, const SdfPath& primPath)const
{
    //Always edit its override wireframe color
    auto edited = HdContainerDataSourceEditor(dataSource);
    edited.Set(HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor),
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(VtVec4fArray{_wireframeColorInterface->getWireframeColor(primPath)}),
                            HdPrimvarSchemaTokens->constant,
                            HdPrimvarSchemaTokens->color));

    HdXformSchema::Builder xformBuilder;
    GfMatrix4d matrix;
    matrix.SetScale(1.25);
    xformBuilder.SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(matrix));
    //edited.Set(HdXformSchema::GetDefaultLocator(), xformBuilder.Build());
    
    //Is the prim in refined displayStyle (meaning shaded) ?
    if (HdLegacyDisplayStyleSchema styleSchema =
            HdLegacyDisplayStyleSchema::GetFromParent(dataSource)) {

        if (HdTokenArrayDataSourceHandle ds =
                styleSchema.GetReprSelector()) {
            VtArray<TfToken> ar = ds->GetTypedValue(0.0f);
            TfToken refinedToken = ar[0];
            if(HdReprTokens->refined == refinedToken){
                //Is in refined display style, apply the wire on top of shaded reprselector
                return HdOverlayContainerDataSource::New({ edited.Finish(), sRefinedWireOnSurfaceDisplayStyleDataSource});
            }
        }else{
            //No reprSelector found, assume it's in the Collection that we have set HdReprTokens->refined
            return HdOverlayContainerDataSource::New({ edited.Finish(), sRefinedWireOnSurfaceDisplayStyleDataSource});
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
    // for (auto& entry : entries) {
    //     if (entry.primType == HdPrimTypeTokens->instancer) {

    //         SdfPath mirrorPath = _GetSelectionHighlightMirrorPath(entry.primPath);
    //         newEntries.push_back({mirrorPath, entry.primType});
    //     }
    // }
    for (const auto& entry : entries) {
        // TODO : Check if instancer with selections, or if an instancee whose instancer(s) have selections?
        // Maybe only need new instancers with selections?
        // New proto subprim : handled automatically
        // New proto : means an instancer has changed -> should be reflected automatically?
        if (_IsInstancerWithSelections(GetInputSceneIndex()->GetPrim(entry.primPath))) {
            _CreateShMirrorsForInstancer(entry.primPath);
        }
    }
    // for (const auto& entry : entries) {
    //     if (_originalPrimPathsToSelectionHighlightMirrorPrimPaths.find(entry.primPath) != _originalPrimPathsToSelectionHighlightMirrorPrimPaths.end()) {
    //         newEntries.push_back({_originalPrimPathsToSelectionHighlightMirrorPrimPaths.at(entry.primPath), entry.primType});
    //     }
    // }
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

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedShMirrors;
    for (const auto& entry : entries) {
        auto shMirrorPath = _RepathToSelectionHighlightMirror(entry.primPath);
        if (!shMirrorPath.IsEmpty()) {
            dirtiedShMirrors.emplace_back(shMirrorPath, entry.dirtyLocators);
        }
    }
    _SendPrimsDirtied(dirtiedShMirrors);

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
            // Forward dirtiness to SH mirrors masks
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
