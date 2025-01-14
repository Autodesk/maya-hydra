#include "niPrototypeWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
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
#include <pxr/imaging/hd/xformSchema.h>
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

SdfPath _GetNativeInstancePrototypePath(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& nativeInstancePrimPath) {
    HdSceneIndexPrim nativeInstancePrim = sceneIndex->GetPrim(nativeInstancePrimPath);
    HdInstanceSchema instanceSchema = HdInstanceSchema::GetFromParent(nativeInstancePrim.dataSource);
    auto instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    auto prototypePath = instancerTopologySchema.GetPrototypes()->GetTypedValue(0)[instanceSchema.GetPrototypeIndex()->GetTypedValue(0)];
    return prototypePath;
}

bool _IsNativePrototype(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) {
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBySchema.IsDefined()) {
        return false;
    }
    SdfPath instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0)[0];
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    return instancerTopologySchema.GetInstanceLocations() != nullptr;
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

}

namespace FVP_NS_DEF {

HdSceneIndexBaseRefPtr NiPrototypeWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new NiPrototypeWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim NiPrototypeWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);

    prim.dataSource = _RerootingSceneIndexContainerDataSource::New(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath, prim.dataSource);
    HdContainerDataSourceEditor dsEditor(prim.dataSource);

    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    HdSelectionSchema activeSelection = selectionsSchema.GetElement(std::stoul(selectionKey.second));
    HdInstanceIndicesSchema instanceIndices = activeSelection.GetNestedInstanceIndices().GetElement(0);
    auto instanceIndex = instanceIndices.GetInstanceIndices()->GetTypedValue(0).front();
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    auto instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0).front();
    //// Do it on every prim or only proto root?
    HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(instancerPrim.dataSource);
    auto instanceTransformsSchema = primvarsSchema.GetPrimvar(HdInstancerTokens->instanceTransforms);
    auto instanceTransforms = HdTypedSampledDataSource<VtArray<GfMatrix4d>>::Cast(instanceTransformsSchema.GetPrimvarValue());
    auto instanceXform = instanceTransforms->GetTypedValue(0)[instanceIndex];
    auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);
    dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(instanceXform * prototypeXform));    

    dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());

    prim.dataSource = dsEditor.Finish();
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(ConvertHydraToFvpSelection(originalPath, activeSelection)));
    }
    return prim;
};

SdfPathVector NiPrototypeWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    if (fullPrimPath == selectionPath) {
        // Return only the prototype prim we're interested in
        return {_selectionPathsToPrototypePaths.at(selectionPath)};
    }
    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        childPaths.emplace_back(originalChildPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
    }
    return childPaths;
}

NiPrototypeWhSi::NiPrototypeWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (_IsNativePrototype(GetInputSceneIndex(), primPath)) {
            // TODO : Handle path exclusions
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(primPath, std::to_string(selectionId));
                }
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void NiPrototypeWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (_IsNativePrototype(GetInputSceneIndex(), entry.primPath)) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                }
                // We just created the highlight, no need to add highlight prims
                continue;
            }
        }

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath), entry.primType);
                }
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void NiPrototypeWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itSelectedPrim->second) {
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

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
                }
            }
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void NiPrototypeWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            std::cout << "NiPrototypeWhSi::ProcessDirtiedPrims before _IsNativePrototype" << std::endl;
            if (_IsNativePrototype(GetInputSceneIndex(), entry.primPath)) {
                std::cout << "NiPrototypeWhSi::ProcessDirtiedPrims after _IsNativePrototype" << std::endl;
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    auto selectionKeysToDelete = existingSelectionKeys->second;
                    for (const auto& selectionKey : selectionKeysToDelete) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        std::cout << "NiPrototypeWhSi::ProcessDirtiedPrims before _CreateSelectionHighlight" << std::endl;
                        _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                    }
                }
                // We rebuilt the highlight, no need to do the rest
                continue;
            }
        }

        // Propagate notifications if this prim is a relevant prototype or a subprim of one
        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void NiPrototypeWhSi::_CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Setup data structures
    std::cout << "_CreateSelectionHighlight(prototypePath, selectionId)" << std::endl;
    SelectionKey selectionKey { prototypePath, selectionId };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _prototypePathsToSelections[prototypePath].emplace(selectionKey);
    _selectionPathsToPrototypePrefixes.emplace(selectionPath, prototypePath.GetParentPath());
    _selectionPathsToPrototypePaths.emplace(selectionPath, prototypePath);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, prototypePath, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(prototypePath.GetParentPath(), selectionPath), prim.primType);
        return true;
    };
    ForEachPrimInHierarchy(prototypePath, operation);

    // Send notifications
    _SendPrimsAdded(addedPrims);
}

void NiPrototypeWhSi::_DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Erase from data structures
    SelectionKey selectionKey { prototypePath, selectionId };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    auto prototypePrefix = _selectionPathsToPrototypePrefixes.at(selectionPath);
    auto itPrototypePath = _prototypePathsToSelections.upper_bound(prototypePrefix);
    if (itPrototypePath != _prototypePathsToSelections.end()) {
        itPrototypePath->second.erase(selectionKey);
    }
    _selectionPathsToPrototypePrefixes.erase(selectionPath);
    _selectionPathsToPrototypePaths.erase(selectionPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
