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
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/subdivisionTagsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

namespace {
// Tokens for the granular per-primvar convenience methods. These must match what
// the Maya adapters advertise in GetPrimvarDescriptors (st, tangents, displayColor).
// Intentionally kept as local constants: flowViewport must not depend upward on
// MayaHydraAdapterTokens (that would invert the library hierarchy).
const TfToken kStToken("st");
const TfToken kTangentsToken("tangents");
const TfToken kDisplayColorToken("displayColor");
} // namespace

FvpDirtyNotifier::FvpDirtyNotifier(HdRetainedSceneIndex& sceneIndex, const SdfPath& primPath)
    : _sceneIndex(sceneIndex)
    , _primPath(primPath)
{
}

FvpDirtyNotifier::~FvpDirtyNotifier()
{
    if (!_locators.IsEmpty()) {
        // TF_CODING_ERROR is compiled out in release builds, so use TF_WARN to ensure the
        // message is visible in all configurations. Dirty state is intentionally NOT flushed
        // here: an automatic flush in the destructor would hide the bug at the call site.
        TF_WARN(
            "FvpDirtyNotifier for prim (%s) destroyed with pending dirty locators; "
            "flush() was never called. Dirty state is lost.",
            _primPath.GetText());
    }
}

void FvpDirtyNotifier::flush()
{
    if (_locators.IsEmpty()) {
        return;
    }
    _sceneIndex.DirtyPrims({ { _primPath, _locators } });
    _locators = {};
}

FvpDirtyNotifier& FvpDirtyNotifier::_append(const HdDataSourceLocator& locator)
{
    _locators.append(locator);
    return *this;
}

// ---- Rprim / geometry ----

FvpDirtyNotifier& FvpDirtyNotifier::dirtyTransform()
{
    return _append(HdXformSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyVisibility()
{
    return _append(HdVisibilitySchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyPoints()
{
    return _append(HdPrimvarsSchema::GetPointsLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyNormals()
{
    return _append(HdPrimvarsSchema::GetNormalsLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyPrimvar(const TfToken& name)
{
    return _append(HdPrimvarsSchema::GetDefaultLocator().Append(name));
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyUVs() { return dirtyPrimvar(kStToken); }

FvpDirtyNotifier& FvpDirtyNotifier::dirtyTangents() { return dirtyPrimvar(kTangentsToken); }

FvpDirtyNotifier& FvpDirtyNotifier::dirtyDisplayColor()
{
    return dirtyPrimvar(kDisplayColorToken);
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyVertexColors()
{
    // Per-vertex color sets and the per-object display color are mutually exclusive but share
    // the primvars/displayColor locator — the interpolation mode is part of the primvar data.
    return dirtyPrimvar(kDisplayColorToken);
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyPrimvars()
{
    return _append(HdPrimvarsSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyExtComputationPrimvars()
{
    return _append(HdExtComputationPrimvarsSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyTopology()
{
    // The translator always emits both for DirtyTopology on a mesh.
    _append(HdMeshSchema::GetSubdivisionSchemeLocator());
    return _append(HdMeshTopologySchema::GetDefaultLocator());
}

void FvpDirtyNotifier::DirtyRprimConnectivityLocators(FvpDirtyNotifier& notifier, bool useMayaNormals)
{
    notifier.dirtyTopology().dirtyPrimvars().dirtyPoints().dirtyExtent();
    if (useMayaNormals) {
        notifier.dirtyNormals();
    }
}

void FvpDirtyNotifier::DirtySmoothMeshDisplayLocators(FvpDirtyNotifier& notifier, bool useMayaNormals)
{
    notifier.dirtyDisplayStyle().dirtyTopology().dirtySubdivision();
    if (useMayaNormals) {
        notifier.dirtyNormals();
    }
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyExtent()
{
    return _append(HdExtentSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyDoubleSided()
{
    return _append(HdMeshSchema::GetDoubleSidedLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyCullStyle()
{
    return _append(HdLegacyDisplayStyleSchema::GetCullStyleLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtySubdivision()
{
    return _append(HdSubdivisionTagsSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyDisplayStyle()
{
    return _append(HdLegacyDisplayStyleSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyMaterialBinding()
{
    return _append(HdMaterialBindingsSchema::GetDefaultLocator());
}

// ---- Sprim / lights ----

FvpDirtyNotifier& FvpDirtyNotifier::dirtyLightParams()
{
    return _append(HdLightSchema::GetDefaultLocator());
}

FvpDirtyNotifier& FvpDirtyNotifier::dirtyCollections()
{
    return _append(HdCollectionsSchema::GetDefaultLocator());
}

// ---- Sprim / camera ----

FvpDirtyNotifier& FvpDirtyNotifier::dirtyCameraParams()
{
    return _append(HdCameraSchema::GetDefaultLocator());
}

// ---- Sprim / material ----

FvpDirtyNotifier& FvpDirtyNotifier::dirtyMaterial()
{
    return _append(HdMaterialSchema::GetDefaultLocator());
}

// ---- Instancer ----

FvpDirtyNotifier& FvpDirtyNotifier::dirtyInstancer()
{
    _append(HdInstancedBySchema::GetDefaultLocator());
    return _append(HdInstancerTopologySchema::GetDefaultLocator());
}

} // namespace FVP_NS_DEF
