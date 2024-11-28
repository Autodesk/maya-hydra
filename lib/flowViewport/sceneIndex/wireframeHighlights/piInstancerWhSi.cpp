#include "piInstancerWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/pathTable.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

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

Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
    Fvp::PrimSelection primSelection;
    primSelection.primPath = primPath;

    HdInstanceIndicesVectorSchema nestedInstanceIndicesSchema = selectionSchema.GetNestedInstanceIndices();
    //std::cout << "nestedInstanceIndicesSchema.GetNumElements() = " << nestedInstanceIndicesSchema.GetNumElements() << std::endl;
    for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
        //std::cout << "iNestedInstanceIndices : " << iNestedInstanceIndices << std::endl;
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

SdfPath SelectionPathFromKey(const Fvp::SelectionKey& selectionKey) {
    return selectionKey.first.AppendElementString("Selection_" + std::to_string(selectionKey.second));
}

class _RerootingSceneIndexPathDataSource : public HdPathDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathDataSource)

    _RerootingSceneIndexPathDataSource(
        const SdfPath &srcPrefix,
        const SdfPath &dstPrefix,
        HdPathDataSourceHandle const &inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
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
        return srcPath.ReplacePrefix(_srcPrefix, _dstPrefix);
    }

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdPathDataSourceHandle const _inputDataSource;
};

// ----------------------------------------------------------------------------

class _RerootingSceneIndexPathArrayDataSource : public HdPathArrayDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexPathArrayDataSource)

    _RerootingSceneIndexPathArrayDataSource(
        const SdfPath& srcPrefix,
        const SdfPath& dstPrefix,
        HdPathArrayDataSourceHandle const & inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
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

        const size_t n = result.size();

        if (n == 0) {
            return result;
        }

        size_t i = 0;

        // If _srcPrefix is absolute root path, we know that we
        // need to translate every path.
        if (!_srcPrefix.IsAbsoluteRootPath()) {
            // Find the first element where we need to change the path.
            //
            // Use const & so that paths[i] does not trigger VtArray
            // to make a copy.
            const VtArray<SdfPath> &paths = result.AsConst();
            while (!paths[i].HasPrefix(_srcPrefix)) {
                ++i;
                if (i == n) {
                    // No need to modify result if no path needed
                    // to be changed.
                    return result;
                }
            }
        }

        // Starting with the first element where the path matched the
        // prefix, process it and all following elements.
        for (; i < n; i++) {
            SdfPath &path = result[i];
            path = path.ReplacePrefix(_srcPrefix, _dstPrefix);
        }

        return result;
    }

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdPathArrayDataSourceHandle const _inputDataSource;
};

// ----------------------------------------------------------------------------

class _RerootingSceneIndexContainerDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_RerootingSceneIndexContainerDataSource)

    _RerootingSceneIndexContainerDataSource(
        const SdfPath &srcPrefix,
        const SdfPath &dstPrefix,
        HdContainerDataSourceHandle const &inputDataSource)
      : _srcPrefix(srcPrefix)
      , _dstPrefix(dstPrefix)
      , _inputDataSource(inputDataSource)
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
            return New(_srcPrefix, _dstPrefix, std::move(childContainer));
        }

        if (auto childPathDataSource =
                HdTypedSampledDataSource<SdfPath>::Cast(childSource)) {
            return _RerootingSceneIndexPathDataSource::New(
                _srcPrefix, _dstPrefix, childPathDataSource);
        }

        if (auto childPathArrayDataSource =
                HdTypedSampledDataSource<VtArray<SdfPath>>::Cast(
                    childSource)) {
            return _RerootingSceneIndexPathArrayDataSource::New(
                _srcPrefix, _dstPrefix, childPathArrayDataSource);
        }

        return childSource;
    }

private:
    const SdfPath _srcPrefix;
    const SdfPath _dstPrefix;
    HdContainerDataSourceHandle const _inputDataSource;
};

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
HdContainerDataSourceHandle MakeWireframe2(const HdContainerDataSourceHandle& dataSource, const GfVec4f& color)
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

HdSceneIndexBaseRefPtr PiInstancerWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PiInstancerWhSi(inputSceneIndex, wireframeColorInterface));
}

HdSceneIndexPrim PiInstancerWhSi::GetPrim(const SdfPath &primPath) const
{
    if (_selectionPaths.empty()) {
        return {};
    }

    auto it = _selectionPaths.upper_bound(primPath);
    bool isRealPrim = it != _selectionPaths.begin() && primPath.HasPrefix(*std::prev(it)) && primPath != *std::prev(it);
    if (!isRealPrim) {
        // This is a path leading up to a selection path, a selection path itself, or a leftover path
        return {};
    }

    auto selectionPath = primPath;
    while (_selectionPaths.find(selectionPath) == _selectionPaths.end()) {
        selectionPath = selectionPath.GetParentPath();
    }

    auto selectedPrimPath = selectionPath.GetParentPath();
    auto selectionId = std::stoul(selectionPath.GetElementString().substr(std::string("Selection_").size()));
    auto primSelection = _selections.at(SelectionKey(selectedPrimPath, selectionId))._primSelection;

    auto originalPath = primPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = _RerootingSceneIndexContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = MakeWireframe2(prim.dataSource, _wireframeColorInterface->getWireframeColor(primSelection));
    }
    return prim;
};

SdfPathVector PiInstancerWhSi::GetChildPrimPaths(const SdfPath &primPath) const
{
    // SdfPathVector childPaths;
    // for (const auto& [selectedPrimPath, selection] : _primPathsToSelections) {
    //     if (primPath != selectedPrimPath && selectedPrimPath.HasPrefix(primPath)) {
    //         childPaths.emplace_back(selectedPrimPath.GetPrefixes()[primPath.GetPathElementCount()]);
    //     }
    // }
    // if (!childPaths.empty()) {
    //     return childPaths;
    // }
    

    // auto selections = _primPathsToSelections.find(primPath);
    // if (selections != _primPathsToSelections.end()) {
    //     for (const auto& selection : selections->second) {
    //         childPaths.emplace_back(selection.first.AppendChild(TfToken("Selection_" + std::to_string(selection.second))));
    //     }
    // }
    // if (!childPaths.empty()) {
    //     return childPaths;
    // }
    
    //return _backingSceneIndex->GetChildPrimPaths(primPath);

    if (_selectionPaths.empty()) {
        return {};
    }

    SdfPathVector childPaths;

    // To return the paths leading up to and including selection paths
    auto it = _selectionPaths.upper_bound(primPath);
    while (it != _selectionPaths.end() && it->HasPrefix(primPath)) {
        childPaths.emplace_back(it->GetPrefixes()[primPath.GetPathElementCount()]);
        it++;
    }
    if (!childPaths.empty()) {
        return childPaths;
    }

    // To return the original scene's paths based on the selection path
    
    // Find the selection path
    auto selectionPath = primPath;
    while (_selectionPaths.find(selectionPath) == _selectionPaths.end()) {
        selectionPath = selectionPath.GetParentPath();
    }

    auto originalPath = primPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        auto itInstancer = _instancerPathsToSelections.upper_bound(originalChildPath);
        bool isInstancerRelevantPath = (itInstancer != _instancerPathsToSelections.end() && itInstancer->first.HasPrefix(originalChildPath)) || (itInstancer != _instancerPathsToSelections.begin() && originalChildPath.HasPrefix((--itInstancer)->first));
        auto itPrototype = _prototypePathsToSelections.upper_bound(originalChildPath);
        bool isPrototypeRelevantPath = (itPrototype != _prototypePathsToSelections.end() && itPrototype->first.HasPrefix(originalChildPath)) || (itPrototype != _prototypePathsToSelections.begin() && originalChildPath.HasPrefix((--itPrototype)->first));
        if (isInstancerRelevantPath || isPrototypeRelevantPath) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
        }
    }
    return childPaths;

    // SdfPathVector originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    // SdfPathVector prunedChildPaths;
    // for (const auto& originalChildPath : originalChildPaths) {
    //     for (const auto& instancerPath : _instancerPaths) {
    //         if (instancerPath.HasPrefix(originalChildPath)) {
    //             prunedChildPaths.push_back(originalChildPath);
    //             break;
    //         }
    //     }

    //     for (const auto& prototypePath : _prototypePaths) {
    //         if (prototypePath.HasPrefix(originalChildPath) || originalChildPath.HasPrefix(prototypePath)) {
    //             prunedChildPaths.push_back(originalChildPath);
    //             break;
    //         }
    //     }
    // }
    // return prunedChildPaths;
}

PiInstancerWhSi::PiInstancerWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : HdSingleInputFilteringSceneIndexBase(inputSceneIndex),
    InputSceneIndexUtils(inputSceneIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    //_CollectInstancingPaths(instancerPrimPath, SelectionHighlightsCollectionDirection2::Bidirectional2, _instancerPaths, _prototypePaths);

    // std::cout << "_primPathsToConserve" << std::endl;
    // for (const auto& path : _primPathsToConserve) {
    //     std::cout << "--- " + path.GetString() << std::endl;
    // }
    // std::cout << std::endl;

    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (prim.primType == HdPrimTypeTokens->instancer) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(primPath, selectionId);
                }
            }
        }
        return true;
    };
    _ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void PiInstancerWhSi::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
    HdSceneIndexObserver::AddedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (entry.primType == HdPrimTypeTokens->instancer) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(entry.primPath, selectionId);
                }
                // We just created the highlight, no need to add highlight prims
                continue;
            }
        }

        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
            }
        }

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
            }
        }
    }
    _SendPrimsAdded(filteredEntries);
}

void PiInstancerWhSi::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itSelectedPrim->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        auto itInstancerParentRemoval = _instancerPathsToSelections.lower_bound(entry.primPath);
        if (itInstancerParentRemoval != _instancerPathsToSelections.end()) {
            if (itInstancerParentRemoval->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itInstancerParentRemoval->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        auto itPrototypeParentRemoval = _prototypePathsToSelections.lower_bound(entry.primPath);
        if (itPrototypeParentRemoval != _prototypePathsToSelections.end()) {
            if (itPrototypeParentRemoval->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itPrototypeParentRemoval->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
            }
        }

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
            }
        }
    }
    _SendPrimsRemoved(filteredEntries);
}

void PiInstancerWhSi::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (prim.primType == HdPrimTypeTokens->instancer) {
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    for (const auto& selectionKey : existingSelectionKeys->second) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        _CreateSelectionHighlight(entry.primPath, selectionId);
                    }
                }
                // We rebuilt the highlight, no need to do the rest
                continue;
            }
        }

        if ((_instancerPathsToSelections.find(entry.primPath) != _instancerPathsToSelections.end() || _prototypePathsToSelections.find(entry.primPath) != _prototypePathsToSelections.end()) 
            && entry.dirtyLocators.Intersects(HdInstancerTopologySchema::GetDefaultLocator().Append(HdInstancerTopologySchemaTokens->prototypes))) {
            // Instancing topology was changed : rebuild the highlights, since we don't know exactly how it was changed
            auto instancerSelectionKeysToRebuild = _instancerPathsToSelections.find(entry.primPath);
            if (instancerSelectionKeysToRebuild != _instancerPathsToSelections.end()) {
                for (const auto& selectionKey : instancerSelectionKeysToRebuild->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            auto prototypeSelectionKeysToRebuild = _prototypePathsToSelections.find(entry.primPath);
            if (prototypeSelectionKeysToRebuild != _prototypePathsToSelections.end()) {
                for (const auto& selectionKey : prototypeSelectionKeysToRebuild->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            // No need to dirty in this case since we'll have removed and re-added prims, skip to next entries
            continue;
        }

        // Propagate notifications if this prim is a relevant instancer or a subprim of one
        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }

        // Propagate notifications if this prim is a relevant prototype or a subprim of one
        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    filteredEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(filteredEntries);
}

void PiInstancerWhSi::_CreateSelectionHighlight(const SdfPath& primPath, size_t selectionId)
{
    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    _CollectInstancingPaths(primPath, SelectionHighlightsCollectionDirection2::Bidirectional2, instancerPaths, prototypePaths);

    // Setup data structures
    SelectionKey selectionKey { primPath, selectionId };
    SelectionData selectionData;
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    selectionData._primSelection = ConvertHydraToFvpSelection(primPath, selectionsSchema.GetElement(selectionId));
    selectionData._instancerPaths = instancerPaths;
    selectionData._prototypePaths = prototypePaths;
    _selections[selectionKey] = selectionData;
    _primPathsToSelections[primPath].emplace(selectionKey);
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.emplace(selectionPath);
    for (const auto& instancerPath : instancerPaths) {
        _instancerPathsToSelections[instancerPath].emplace(selectionKey);
    }
    for (const auto& prototypePath : prototypePaths) {
        _prototypePathsToSelections[prototypePath].emplace(selectionKey);
    }

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), prim.primType);
        return true;
    };
    for (const auto& instancerPath : instancerPaths) {
        _ForEachPrimInHierarchy(instancerPath, operation);
    }
    for (const auto& prototypePath : prototypePaths) {
        _ForEachPrimInHierarchy(prototypePath, operation);
    }
    _SendPrimsAdded(addedPrims);
}

void PiInstancerWhSi::_DeleteSelectionHighlight(const SdfPath& primPath, size_t selectionId)
{
    // Collect paths
    SelectionKey selectionKey { primPath, selectionId };
    if (_selections.find(selectionKey) == _selections.end()) {
        return;
    }
    SelectionData selectionData = _selections.at(selectionKey);

    // Erase from data structures
    _selections.erase(selectionKey);
    _primPathsToSelections[primPath].erase(selectionKey);
    if (_primPathsToSelections[primPath].empty()) {
        _primPathsToSelections.erase(primPath);
    }
    SdfPath selectionPath = SelectionPathFromKey(selectionKey);
    _selectionPaths.erase(selectionPath);
    for (const auto& instancerPath : selectionData._instancerPaths) {
        _instancerPathsToSelections[instancerPath].erase(selectionKey);
        if (_instancerPathsToSelections[instancerPath].empty()) {
            _instancerPathsToSelections.erase(instancerPath);
        }
    }
    for (const auto& prototypePath : selectionData._prototypePaths) {
        _prototypePathsToSelections[prototypePath].erase(selectionKey);
        if (_prototypePathsToSelections[prototypePath].empty()) {
            _prototypePathsToSelections.erase(prototypePath);
        }
    }

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

// bool PiInstancerWhSi::_IsInstancerPath(const PXR_NS::SdfPath& primPath) const
// {
//     return _instancerPaths.find(primPath) != _instancerPaths.end();
//     // for (const auto& instancerPath : _instancerPaths) {
//     //     // Use direct path rather than prefix?
//     //     if (primPath.HasPrefix(instancerPath)) {
//     //         return true;
//     //     }
//     // }
//     // return false;
// }

// bool PiInstancerWhSi::_IsPrototypePath(const PXR_NS::SdfPath& primPath) const
// {
//     for (const auto& prototypePath : _prototypePaths) {
//         if (primPath.HasPrefix(prototypePath)) {
//             return true;
//         }
//     }
//     return false;
// }

// bool PiInstancerWhSi::_IsRelevantPath(const PXR_NS::SdfPath& primPath) const
// {
//     return _IsInstancerPath(primPath) || _IsPrototypePath(primPath);
// }

void
PiInstancerWhSi::_CollectInstancingPaths(const PXR_NS::SdfPath& primPath, SelectionHighlightsCollectionDirection2 direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // If this is a prototype sub-prim, redirect the call to the prototype root, so that the prototype root
    // becomes the actual selection highlight mirror. The instancing-related paths will be processed as part
    // of the children traversal later down this method.
    if (_IsPrototypeSubPrim(prim, primPath)) {
        HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
        auto protoRootPaths = instancedBy.GetPrototypeRoots()->GetTypedValue(0);
        for (const auto& protoRootPath : protoRootPaths) {
            _CollectInstancingPaths(protoRootPath, direction, outInstancerPaths, outPrototypePaths);
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
    _ForEachPrimInHierarchy(primPath, operation);

    for (const auto& affectedPrototypePath : affectedPrototypePaths) {
        _CollectInstancingPaths(affectedPrototypePath, SelectionHighlightsCollectionDirection2::Prototypes2, outInstancerPaths, outPrototypePaths);
    }
    for (const auto& affectedInstancedByPath : affectedInstancedByPaths) {
        _CollectInstancingPaths(affectedInstancedByPath, SelectionHighlightsCollectionDirection2::InstancedBy2, outInstancerPaths, outPrototypePaths);
    }
}

void
PiInstancerWhSi::_ForEachPrimInHierarchy(
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

}
