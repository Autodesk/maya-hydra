// Copyright 2026 Autodesk
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
// Maps each DirtyNotifier::dirty*() call to the matching Hydra schema locator;
// static helpers bundle locators for connectivity and smooth-mesh display edits.
// flush() forwards the deduplicated HdDataSourceLocatorSet via DirtyPrims().
//
#include "fvpDirtyNotifier.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>

#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/collectionsSchema.h>
#include <pxr/imaging/hd/extComputationPrimvarsSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/subdivisionTagsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

// See beginDirtyBatch / commitDirtyBatch methods.
static HdSceneIndexObserver::DirtiedPrimEntries _sPendingDirtyEntries;
static TfWeakPtr<HdRetainedSceneIndex> _sBatchingSceneIndex = nullptr;

DirtyNotifier::DirtyNotifier(HdRetainedSceneIndex& sceneIndex, const SdfPath& primPath)
    : _sceneIndex(TfCreateWeakPtr(&sceneIndex))
    , _primPath(primPath)
{
}

DirtyNotifier::~DirtyNotifier()
{
    flush();
}

void DirtyNotifier::flush()
{
    if (_locators.IsEmpty()) {
        return;
    }

    if (_sBatchingSceneIndex) {
        _sPendingDirtyEntries.push_back({ _primPath, _locators });
    } else if (_sceneIndex) {
        _sceneIndex->DirtyPrims({ { _primPath, _locators } });
    } else {
        TF_CODING_ERROR(
            "DirtyNotifier for prim (%s): scene index no longer valid; "
            "dropping pending dirty locators. The scene index passed to the "
            "constructor must outlive this DirtyNotifier.",
            _primPath.GetText());
    }

    _locators = HdDataSourceLocatorSet();
}

void DirtyNotifier::beginDirtyBatch(PXR_NS::HdRetainedSceneIndex& batchingSceneIndex)
{
    _sBatchingSceneIndex = TfCreateWeakPtr(&batchingSceneIndex);
}

void DirtyNotifier::commitDirtyBatch()
{
    if (_sBatchingSceneIndex) {
        _sBatchingSceneIndex->DirtyPrims(_sPendingDirtyEntries);
        _sBatchingSceneIndex = nullptr;
        _sPendingDirtyEntries.clear();
    }
}

DirtyNotifier::DirtyBatchGuard::DirtyBatchGuard(HdRetainedSceneIndex& batchingSceneIndex)
{
    DirtyNotifier::beginDirtyBatch(batchingSceneIndex);
}

DirtyNotifier::DirtyBatchGuard::~DirtyBatchGuard()
{
    DirtyNotifier::commitDirtyBatch();
}

DirtyNotifier& DirtyNotifier::_append(const HdDataSourceLocator& locator)
{
    _locators.append(locator);
    return *this;
}

// ---- Rprim / geometry ----

DirtyNotifier& DirtyNotifier::dirtyTransform()
{
    return _append(HdXformSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyVisibility()
{
    return _append(HdVisibilitySchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyPurpose()
{
    return _append(HdPurposeSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyPoints()
{
    return _append(HdPrimvarsSchema::GetPointsLocator());
}

DirtyNotifier& DirtyNotifier::dirtyNormals()
{
    return _append(HdPrimvarsSchema::GetNormalsLocator());
}

DirtyNotifier& DirtyNotifier::dirtyPrimvar(const TfToken& name)
{
    return _append(HdPrimvarsSchema::GetDefaultLocator().Append(name));
}

DirtyNotifier& DirtyNotifier::dirtyDisplayColor()
{
    return dirtyPrimvar(HdTokens->displayColor);
}

DirtyNotifier& DirtyNotifier::dirtyVertexColors()
{
    // Per-vertex color sets and the per-object display color are mutually exclusive but share
    // the primvars/displayColor locator — the interpolation mode is part of the primvar data.
    return dirtyPrimvar(HdTokens->displayColor);
}

DirtyNotifier& DirtyNotifier::dirtyPrimvars()
{
    return _append(HdPrimvarsSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyExtComputationPrimvars()
{
    return _append(HdExtComputationPrimvarsSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyMeshTopology()
{
    // The translator always emits both for DirtyTopology on a mesh.
    _append(HdMeshSchema::GetSubdivisionSchemeLocator());
    return _append(HdMeshTopologySchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyBasisCurvesTopology()
{
    return _append(HdBasisCurvesTopologySchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyTopology(const TfToken& primType)
{
    if (primType == HdPrimTypeTokens->basisCurves) {
        return dirtyBasisCurvesTopology();
    }
    if (primType == HdPrimTypeTokens->mesh) {
        return dirtyMeshTopology();
    }
    TF_WARN(
        "DirtyNotifier::dirtyTopology: unsupported prim type '%s'; "
        "no topology locators emitted.",
        primType.GetText());
    return *this;
}

void DirtyNotifier::DirtyRprimConnectivityLocators(
    DirtyNotifier& notifier,
    const TfToken&     primType)
{
    if (primType == HdPrimTypeTokens->basisCurves) {
        notifier.dirtyBasisCurvesTopology().dirtyPrimvars().dirtyPoints().dirtyExtent();
        return;
    }
    if (primType == HdPrimTypeTokens->mesh) {
        notifier.dirtyMeshTopology().dirtyPrimvars().dirtyPoints().dirtyExtent();
        return;
    }
    TF_WARN(
        "DirtyNotifier::DirtyRprimConnectivityLocators: unsupported prim type '%s'; "
        "no connectivity locators emitted.",
        primType.GetText());
}

void DirtyNotifier::DirtySmoothMeshDisplayLocators(DirtyNotifier& notifier)
{
    notifier.dirtyDisplayStyle().dirtyMeshTopology().dirtySubdivision();
}

DirtyNotifier& DirtyNotifier::dirtyExtent()
{
    return _append(HdExtentSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyDoubleSided()
{
    return _append(HdMeshSchema::GetDoubleSidedLocator());
}

DirtyNotifier& DirtyNotifier::dirtyCullStyle()
{
    return _append(HdLegacyDisplayStyleSchema::GetCullStyleLocator());
}

DirtyNotifier& DirtyNotifier::dirtySubdivision()
{
    return _append(HdSubdivisionTagsSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyDisplayStyle()
{
    return _append(HdLegacyDisplayStyleSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyMaterialBinding()
{
    return _append(HdMaterialBindingsSchema::GetDefaultLocator());
}

// ---- Sprim / lights ----

DirtyNotifier& DirtyNotifier::dirtyLightParams()
{
    return _append(HdLightSchema::GetDefaultLocator());
}

DirtyNotifier& DirtyNotifier::dirtyCollections()
{
    return _append(HdCollectionsSchema::GetDefaultLocator());
}

// ---- Sprim / camera ----

DirtyNotifier& DirtyNotifier::dirtyCameraParams()
{
    return _append(HdCameraSchema::GetDefaultLocator());
}

// ---- Sprim / material ----

DirtyNotifier& DirtyNotifier::dirtyMaterial()
{
    return _append(HdMaterialSchema::GetDefaultLocator());
}

// ---- Instancer ----

DirtyNotifier& DirtyNotifier::dirtyInstancer()
{
    _append(HdInstancedBySchema::GetDefaultLocator());
    return _append(HdInstancerTopologySchema::GetDefaultLocator());
}

} // namespace FVP_NS_DEF
