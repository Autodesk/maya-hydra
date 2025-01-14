#include "piPrototypeWhSi.h"
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

bool _IsPointInstancePrototype(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) {
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBySchema.IsDefined()) {
        return false;
    }
    SdfPath instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0)[0];
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    return instancerTopologySchema.GetInstanceLocations() == nullptr;
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

HdSceneIndexBaseRefPtr PiPrototypeWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PiPrototypeWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim PiPrototypeWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    // SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    // auto primSelection = _selections.at(selectionKey)._primSelection;

    // auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    // HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    // prim.dataSource = _RerootingSceneIndexContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    // if (prim.primType == HdPrimTypeTokens->mesh) {
    //     prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(primSelection));
    // }
    // if (prim.primType == HdPrimTypeTokens->instancer && originalPath == selectionKey.first) {
    //     HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    //     HdSelectionSchema activeSelection = selectionsSchema.GetElement(std::stoul(selectionKey.second));
    //     prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource, activeSelection);
    // }
    // return prim;


    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    auto primSelection = _selections.at(selectionKey)._primSelection;

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = _RerootingSceneIndexContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(primSelection));
    }
    if (prim.primType == HdPrimTypeTokens->instancer && originalPath == selectionKey.first) {
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        HdSelectionSchema activeSelection = selectionsSchema.GetElement(std::stoul(selectionKey.second));
        //prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource, activeSelection);
    }
    return prim;
};

SdfPathVector PiPrototypeWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    // if (fullPrimPath == selectionPath) {
    //     // Return only the prototype prim we're interested in
    //     return {_selectionPathsToPrototypePaths.at(selectionPath)};
    // }
    // SdfPathVector childPaths;
    // auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    // auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    // for (const auto& originalChildPath : originalChildPaths) {
    //     childPaths.emplace_back(originalChildPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
    // }
    // return childPaths;


    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
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
}

PiPrototypeWhSi::PiPrototypeWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (_IsPointInstancePrototype(GetInputSceneIndex(), primPath)) {
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

void PiPrototypeWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    // for (const auto& entry : entries) {
    //     if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
    //         HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
    //         HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    //         if (selectionsSchema.IsDefined()) {
    //             for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
    //                 _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
    //             }
    //             // We just created the highlight, no need to add highlight prims
    //             continue;
    //         }
    //     }

    //     auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
    //     if (itPrototype != _prototypePathsToSelections.begin()) {
    //         --itPrototype;
    //         if (entry.primPath.HasPrefix(itPrototype->first)) {
    //             for (const auto& selectionKey : itPrototype->second) {
    //                 auto selectionPath = SelectionPathFromKey(selectionKey);
    //                 highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath), entry.primType);
    //             }
    //         }
    //     }
    // }
    // _SendPrimsAdded(highlightEntries);





    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
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

        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
            }
        }

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void PiPrototypeWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    //HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    //for (const auto& entry : entries) {
    //    auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
    //    if (itSelectedPrim != _primPathsToSelections.end()) {
    //        if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
    //            for (const auto& selectionKey : itSelectedPrim->second) {
    //                _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
    //            }
    //        }
    //    }

    //    auto itPrototypeParentRemoval = _prototypePathsToSelections.lower_bound(entry.primPath);
    //    if (itPrototypeParentRemoval != _prototypePathsToSelections.end()) {
    //        if (itPrototypeParentRemoval->first.HasPrefix(entry.primPath)) {
    //            for (const auto& selectionKey : itPrototypeParentRemoval->second) {
    //                _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
    //            }
    //        }
    //    }

    //    auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
    //    if (itPrototype != _prototypePathsToSelections.begin()) {
    //        --itPrototype;
    //        if (entry.primPath.HasPrefix(itPrototype->first)) {
    //            for (const auto& selectionKey : itPrototype->second) {
    //                auto selectionPath = SelectionPathFromKey(selectionKey);
    //                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
    //            }
    //        }
    //    }
    //}
    //_SendPrimsRemoved(highlightEntries);




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
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
            }
        }

        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
            }
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void PiPrototypeWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    //for (const auto& entry : entries) {
    //    if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
    //        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
    //        //std::cout << "PiPrototypeWhSi::ProcessDirtiedPrims before _IsNativePrototype" << std::endl;
    //        if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
    //            //std::cout << "PiPrototypeWhSi::ProcessDirtiedPrims after _IsNativePrototype" << std::endl;
    //            auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
    //            if (existingSelectionKeys != _primPathsToSelections.end()) {
    //                auto selectionKeysToDelete = existingSelectionKeys->second;
    //                for (const auto& selectionKey : selectionKeysToDelete) {
    //                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
    //                }
    //            }
    //            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    //            if (selectionsSchema.IsDefined()) {
    //                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
    //                    //std::cout << "PiPrototypeWhSi::ProcessDirtiedPrims before _CreateSelectionHighlight" << std::endl;
    //                    _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
    //                }
    //            }
    //            // We rebuilt the highlight, no need to do the rest
    //            continue;
    //        }
    //    }
//
    //    // Propagate notifications if this prim is a relevant prototype or a subprim of one
    //    auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
    //    if (itPrototype != _prototypePathsToSelections.begin()) {
    //        --itPrototype;
    //        if (entry.primPath.HasPrefix(itPrototype->first)) {
    //            for (const auto& selectionKey : itPrototype->second) {
    //                auto selectionPath = SelectionPathFromKey(selectionKey);
    //                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
    //            }
    //        }
    //    }
    //}
    //_SendPrimsDirtied(highlightEntries);


    //HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    for (const auto& selectionKey : existingSelectionKeys->second) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                    }
                }
                // We rebuilt the highlight, no need to do the rest
                continue;
            }
        }

        if ((_instancerPathsToSelections.find(entry.primPath) != _instancerPathsToSelections.end() || _prototypePathsToSelections.find(entry.primPath) != _prototypePathsToSelections.end()) 
            && entry.dirtyLocators.Intersects(HdInstancerTopologySchema::GetDefaultLocator())) {
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
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
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
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void PiPrototypeWhSi::_CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Setup data structures
    //std::cout << "_CreateSelectionHighlight(prototypePath, selectionId)" << std::endl;
    //SelectionKey selectionKey { prototypePath, selectionId };
    //SdfPath selectionPath = RegisterSelection(selectionKey);

    //_prototypePathsToSelections[prototypePath].emplace(selectionKey);
    //_selectionPathsToPrototypePrefixes.emplace(selectionPath, prototypePath.GetParentPath());
    //_selectionPathsToPrototypePaths.emplace(selectionPath, prototypePath);

    // Send notifications
    //HdSceneIndexObserver::AddedPrimEntries addedPrims;
    //auto operation = [&addedPrims, prototypePath, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
    //    addedPrims.emplace_back(primPath.ReplacePrefix(prototypePath.GetParentPath(), selectionPath), prim.primType);
    //    return true;
    //};
    //ForEachPrimInHierarchy(prototypePath, operation);
//
    //// Send notifications
    //for (const auto& prim : addedPrims) {
    //    std::cout << prim.primPath << std::endl;
    //    if (prim.primPath.GetString().find("FlowViewportSelectionHighlights/FlowViewportSelectionHighlights") != std::string::npos) {
    //        std::cout << "WRONG" << std::endl;
    //    }
    //}
    //_SendPrimsAdded(addedPrims);





    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    CollectInstancingPaths(prototypePath, SelectionHighlightsCollectionDirection2::Bidirectional2, instancerPaths, prototypePaths);

    // Setup data structures
    SelectionKey selectionKey { prototypePath, selectionId };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(prototypePath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    SelectionData2 selectionData;
    selectionData._primSelection = ConvertHydraToFvpSelection(prototypePath, selectionsSchema.GetElement(std::stoul(selectionId)));
    selectionData._instancerPaths = instancerPaths;
    selectionData._prototypePaths = prototypePaths;
    _selections[selectionKey] = selectionData;
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
        ForEachPrimInHierarchy(instancerPath, operation);
    }
    for (const auto& prototypePath : prototypePaths) {
        ForEachPrimInHierarchy(prototypePath, operation);
    }
    _SendPrimsAdded(addedPrims);
}

void PiPrototypeWhSi::_DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Erase from data structures
    //SelectionKey selectionKey { prototypePath, selectionId };
    //SdfPath selectionPath = UnregisterSelection(selectionKey);
    //auto prototypePrefix = _selectionPathsToPrototypePrefixes.at(selectionPath);
    //auto itPrototypePath = _prototypePathsToSelections.upper_bound(prototypePrefix);
    //if (itPrototypePath != _prototypePathsToSelections.end()) {
    //    itPrototypePath->second.erase(selectionKey);
    //}
    //_selectionPathsToPrototypePrefixes.erase(selectionPath);
    //_selectionPathsToPrototypePaths.erase(selectionPath);
//
    //// Send notifications
    //_SendPrimsRemoved({selectionPath});




    // Collect paths
    SelectionKey selectionKey { prototypePath, selectionId };
    if (_selections.find(selectionKey) == _selections.end()) {
        return;
    }
    SelectionData2 selectionData = _selections.at(selectionKey);

    // Erase from data structures
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    _selections.erase(selectionKey);
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

}
