#include "meshWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
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

// We consider prototypes that have child prims to be different hierarchies,
// separate from each other and from the "root" hierarchy.
VtArray<SdfPath> _GetHierarchyRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
}

// Fvp::PrimSelection ConvertHydraToFvpSelection(const SdfPath& primPath, const HdSelectionSchema& selectionSchema) {
//     Fvp::PrimSelection primSelection;
//     primSelection.primPath = primPath;

//     HdInstanceIndicesVectorSchema nestedInstanceIndicesSchema = selectionSchema.GetNestedInstanceIndices();
//     //std::cout << "nestedInstanceIndicesSchema.GetNumElements() = " << nestedInstanceIndicesSchema.GetNumElements() << std::endl;
//     for (size_t iNestedInstanceIndices = 0; iNestedInstanceIndices < nestedInstanceIndicesSchema.GetNumElements(); iNestedInstanceIndices++) {
//         //std::cout << "iNestedInstanceIndices : " << iNestedInstanceIndices << std::endl;
//         HdInstanceIndicesSchema instanceIndicesSchema = nestedInstanceIndicesSchema.GetElement(iNestedInstanceIndices);
//         auto instanceIndices = instanceIndicesSchema.GetInstanceIndices()->GetTypedValue(0);
//         primSelection.nestedInstanceIndices.push_back(
//             {
//                 instanceIndicesSchema.GetInstancer()->GetTypedValue(0),
//                 instanceIndicesSchema.GetPrototypeIndex()->GetTypedValue(0),
//                 std::vector<int>(instanceIndices.begin(), instanceIndices.end())
//             }
//         );
//     }

//     return primSelection;
// }

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

HdSceneIndexBaseRefPtr MeshWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new MeshWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim MeshWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = _RerootingSceneIndexContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
    return prim;
};

SdfPathVector MeshWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        auto itPath = _highlightedMeshPaths.upper_bound(originalChildPath);
        bool isRelevantPath = (itPath != _highlightedMeshPaths.end() && itPath->HasPrefix(originalChildPath)) || (itPath != _highlightedMeshPaths.begin() && originalChildPath.HasPrefix(*std::prev(itPath)));
        if (isRelevantPath) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
        }
    }
    return childPaths;
}

MeshWhSi::MeshWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(primPath);
                }
            }
        }
        if (prim.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(primPath);
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);

    for (const auto& meshPath : _meshPaths) {
        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(meshPath);
        if (itSelectedParentPath != _fullySelectedPaths.begin() && meshPath.HasPrefix(*std::prev(itSelectedParentPath))) {
            _CreateSelectionHighlight(meshPath);
        }
    }
}

void MeshWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    // no-op? what instancing related stuff we need to port over from fvpWireframeSelectionHighlightSceneIndex.cpp::PrimsAdded?
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        bool isMesh = false;
        bool isSelected = false;
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (entry.primType == HdPrimTypeTokens->mesh) {
            HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
            if (!instancedBy.IsDefined()) {
                _meshPaths.emplace(entry.primPath);
                isMesh = true;
            }
        }
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(entry.primPath);
                    isSelected = true;
                }
            }
        }
        if (isMesh) {
            auto itSelectedParent = _fullySelectedPaths.upper_bound(entry.primPath);
            if (itSelectedParent != _fullySelectedPaths.begin() && entry.primPath.HasPrefix(*std::prev(itSelectedParent))) {
                if (_highlightedMeshPaths.find(entry.primPath) == _highlightedMeshPaths.end()) {
                    _CreateSelectionHighlight(entry.primPath);
                }
            }
        }
        if (isSelected) {
            auto itMeshChild = _meshPaths.lower_bound(entry.primPath);
            while (itMeshChild != _meshPaths.end() && itMeshChild->HasPrefix(entry.primPath)) {
                if (_highlightedMeshPaths.find(*itMeshChild) == _highlightedMeshPaths.end()) {
                    _CreateSelectionHighlight(*itMeshChild);
                }
                itMeshChild++;
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void MeshWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        auto itMesh = _meshPaths.lower_bound(entry.primPath);
        while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
            if (_highlightedMeshPaths.find(*itMesh) != _highlightedMeshPaths.end()) {
                _DeleteSelectionHighlight(*itMesh);
            }
            itMesh = _meshPaths.erase(itMesh);
        }

        auto itSelectedPath = _fullySelectedPaths.lower_bound(entry.primPath);
        while (itSelectedPath != _fullySelectedPaths.end() && itSelectedPath->HasPrefix(entry.primPath)) {
            itSelectedPath = _fullySelectedPaths.erase(itSelectedPath);
            // Child mesh highlights will have already been delete above, since deleting a selected parent
            // implies deleting child meshes.
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void MeshWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            bool isFullySelected = false;
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                        isFullySelected = true;
                    }
                }
            }
            if (isFullySelected && _fullySelectedPaths.find(entry.primPath) == _fullySelectedPaths.end()) {
                _fullySelectedPaths.emplace(entry.primPath);
                auto itMesh = _meshPaths.lower_bound(entry.primPath);
                while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
                    if (_highlightedMeshPaths.find(*itMesh) == _highlightedMeshPaths.end()) {
                        _CreateSelectionHighlight(*itMesh);
                    }
                    itMesh++;
                }
            }
            else if (!isFullySelected && _fullySelectedPaths.find(entry.primPath) != _fullySelectedPaths.end()) {
                _fullySelectedPaths.erase(entry.primPath);
                auto itMesh = _meshPaths.lower_bound(entry.primPath);
                while (itMesh != _meshPaths.end() && itMesh->HasPrefix(entry.primPath)) {
                    if (_highlightedMeshPaths.find(*itMesh) != _highlightedMeshPaths.end()) {
                        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(*itMesh);
                        if (itSelectedParentPath == _fullySelectedPaths.begin() && !itMesh->HasPrefix(*std::prev(itSelectedParentPath))) {
                            // No selected parent.
                            _DeleteSelectionHighlight(*itMesh);
                        }
                    }
                    itMesh++;
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void MeshWhSi::_CreateSelectionHighlight(const SdfPath& primPath)
{
    if (_highlightedMeshPaths.find(primPath) != _highlightedMeshPaths.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { primPath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _highlightedMeshPaths.emplace(primPath);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    addedPrims.emplace_back(primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), GetInputSceneIndex()->GetPrim(primPath).primType);
    _SendPrimsAdded(addedPrims);
}

void MeshWhSi::_DeleteSelectionHighlight(const SdfPath& primPath)
{
    if (_highlightedMeshPaths.find(primPath) == _highlightedMeshPaths.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { primPath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    
    _highlightedMeshPaths.erase(primPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
