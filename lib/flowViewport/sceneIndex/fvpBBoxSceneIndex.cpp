//
// Copyright 2024 Autodesk
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

//Local headers
#include "fvpBBoxSceneIndex.h"
#include "flowViewport/sceneIndex/fvpBoundsCurvesDataSource.h"

//USD/Hydra headers
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>

// This class is a filtering scene index that converts the geometries into a bounding box using the extent attribute. 
// If the extent attribute is not present, we draw nothing, so an extent attribute must exist on all primitives for this mode to be supported correctly.

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{

    //Prims on which we need to change the geometry into a bounding box
    const std::set<TfToken> kSupportedPrimTypes = {
        HdPrimTypeTokens->mesh,
        HdPrimTypeTokens->basisCurves,
    };

    bool _IsSupportedPrimType(const TfToken& primType)
    {
        return kSupportedPrimTypes.find(primType) != kSupportedPrimTypes.cend();
    }
}

BboxSceneIndex::BboxSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface) : 
    ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    TF_AXIOM(_wireframeColorInterface);
}

void BboxSceneIndex::Enable(bool enable) 
{ 
    if (enable != _enabled) {
        _enabled = enable;
        _DirtyAllPrims();
    }
}

void BboxSceneIndex::_DirtyAllPrims()
{
    if (!_IsObserved()) {
        return;
    }

    // Instead of just dirtying, let's re-add all prims
    // This forces a complete resync of the prims with the new topology
    HdSceneIndexObserver::AddedPrimEntries   addedEntries;

    for (const SdfPath& path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(path);

        // Check if this prim is supported for conversion (regardless of enabled state)
        if (prim.dataSource && !_isExcluded(path) && _IsSupportedPrimType(prim.primType)) {
            // Re-add it with the appropriate type based on current enabled state
            if (_enabled) {
                // Convert to bounding box
                addedEntries.emplace_back(path, HdPrimTypeTokens->basisCurves);
            } else {
                // Convert back to original type
                addedEntries.emplace_back(path, prim.primType);
            }
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
}

void BboxSceneIndex::_PrimsDirtied(
    const PXR_NS::HdSceneIndexBase&                         sender,
   const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) {
        return;
    }

    // When bbox mode is off (the common case), nothing needs translating.
    if (!_enabled) {
        _SendPrimsDirtied(entries);
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries transformedEntries;
    transformedEntries.reserve(entries.size());

    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);

        // Only transform entries for prims that we're actually converting
        if (prim.dataSource && _IsSupportedPrimType(prim.primType)
            && !_isExcluded(entry.primPath)) {

            // Transform the dirty locators to basisCurves equivalents
            HdDataSourceLocatorSet transformedLocators;

            for (const auto& locator : entry.dirtyLocators) {
                // Transform mesh-specific locators to basisCurves equivalents
                if (locator.HasPrefix(HdMeshSchema::GetDefaultLocator())) {
                    // Convert mesh topology locators to basisCurves topology locators
                    transformedLocators.insert(HdBasisCurvesSchema::GetDefaultLocator());
                }else {
                    // For any other locators, just keep them as-is
                    transformedLocators.insert(locator);
                }
            }

            transformedEntries.emplace_back(entry.primPath, transformedLocators);
        } else {
            // For prims we're not converting, just pass through the original entry
            transformedEntries.emplace_back(entry);
        }
    }

    _SendPrimsDirtied(transformedEntries);
}

// Modify the helper functions to take the prim as a parameter
bool BboxSceneIndex::_ShouldConvertToBoundingBox(
    const SdfPath&          primPath,
    const HdSceneIndexPrim& prim) const
{
    // Check if bounding box mode is enabled
    if (!_enabled) {
        return false;
    }

    // Check if the prim path is excluded
    if (_isExcluded(primPath)) {
        return false;
    }

    // Check if the prim has a data source
    if (!prim.dataSource) {
        return false;
    }

    // Check if the prim type is supported for conversion
    if (!_IsSupportedPrimType(prim.primType)) {
        return false;
    }

    return true;
}

// Helper function to create the bounding box data source
HdContainerDataSourceHandle BboxSceneIndex::_CreateBoundingBoxDataSource(
    const SdfPath&          primPath,
    const HdSceneIndexPrim& prim) const
{
    if (!prim.dataSource) {
        return nullptr;
    }

    // The wireframe color of this prim is only computed when read.
    return CreateBoundsCurvesPrimDataSource(prim.dataSource, _wireframeColorInterface, primPath);
}

// Update the calling functions
HdSceneIndexPrim BboxSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    // Use the helper function to check if we should convert this prim
    if (_ShouldConvertToBoundingBox(primPath, prim)) {
        prim.primType = HdPrimTypeTokens->basisCurves;
        prim.dataSource = _CreateBoundingBoxDataSource(primPath, prim);
    }

    return prim;
}

void BboxSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved())
        return;

    if (!_enabled) {
        _SendPrimsAdded(entries);
        return;
    }

    HdSceneIndexObserver::AddedPrimEntries transformedEntries;

    for (const auto& entry : entries) {
        const SdfPath& primPath = entry.primPath;
        const TfToken& originalPrimType = entry.primType;

        // Get the prim once
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

        // Check if this prim should be converted to a bounding box
        if (_ShouldConvertToBoundingBox(primPath, prim)) {
            transformedEntries.emplace_back(primPath, HdPrimTypeTokens->basisCurves);
        } else {
            transformedEntries.emplace_back(primPath, originalPrimType);
        }
    }

    _SendPrimsAdded(transformedEntries);
}

}//end of namespace FVP_NS_DEF
