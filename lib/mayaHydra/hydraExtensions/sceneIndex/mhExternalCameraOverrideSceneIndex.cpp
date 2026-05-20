//
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

#include "mhExternalCameraOverrideSceneIndex.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/renderProductSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usdImaging/usdImaging/usdRenderProductSchema.h>
#include <pxr/usdImaging/usdImaging/usdRenderSettingsSchema.h>

#include <algorithm>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const TfToken kExternalCameraToken("adskUsd:externalCamera");

// Special Hydra SdfPath sentinel value used when setting the path to the
// external camera. This will be detected and resolved later, by the
// ExternalCameraResolvingSceneIndex.
const SdfPath kExternalCameraPrefix("/__adskUsd__externalCamera");

// External camera paths are either through USD (already uses '/' as a
// separator), or through Maya (uses '|' as a separator, converted to '/' for
// SdfPath representation).  We also erase the UFE path segment ',' separator.
SdfPath SanitizeExternalPath(const std::string& rawValue)
{
    std::string pathStr = rawValue;

    std::replace(pathStr.begin(), pathStr.end(), '|', '/');
    pathStr.erase(std::remove(pathStr.begin(), pathStr.end(), ','), pathStr.end());

    return kExternalCameraPrefix.AppendPath(SdfPath(pathStr).MakeRelativePath(SdfPath::AbsoluteRootPath()));
}

} // anonymous namespace

namespace MAYAHYDRA_NS_DEF {

MhExternalCameraOverrideSceneIndexRefPtr
MhExternalCameraOverrideSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(
        new MhExternalCameraOverrideSceneIndex(inputSceneIndex));
}

MhExternalCameraOverrideSceneIndex::MhExternalCameraOverrideSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
{
}

HdSceneIndexPrim
MhExternalCameraOverrideSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    if (!prim.dataSource) {
        return prim;
    }

    // Do special external camera override processing only if the prim is a
    // render product or a render setting prim.
    TfToken schemaToken;
    if (prim.primType == HdPrimTypeTokens->renderSettings) {
        schemaToken = UsdImagingUsdRenderSettingsSchemaTokens->__usdRenderSettings;
    } else if (prim.primType == HdRenderProductSchemaTokens->renderProduct) {
        schemaToken = UsdImagingUsdRenderProductSchemaTokens->__usdRenderProduct;
    } else {
        return prim;
    }

    auto schemaDs = HdContainerDataSource::Cast(prim.dataSource->Get(schemaToken));
    if (!schemaDs) {
        return prim;
    }

    auto namespacedSettingsDs =
        HdContainerDataSource::Cast(schemaDs->Get(
            UsdImagingUsdRenderSettingsSchemaTokens->namespacedSettings));
    if (!namespacedSettingsDs) {
        return prim;
    }

    // If we don't have an external camera data source, nothing to do, return
    // the prim unchanged.
    auto externalCameraDs =
        HdTypedSampledDataSource<std::string>::Cast(
            namespacedSettingsDs->Get(kExternalCameraToken));
    if (!externalCameraDs) {
        return prim;
    }

    const std::string rawValue = externalCameraDs->GetTypedValue(0);
    const SdfPath sanitizedPath = SanitizeExternalPath(rawValue);

    HdDataSourceLocator cameraLocator(
        schemaToken, UsdImagingUsdRenderSettingsSchemaTokens->camera);

    // Overwrite or add the internal camera data source with the SdfPath
    // representation of the external camera path, with its sentinel prefix.
    prim.dataSource = HdContainerDataSourceEditor(prim.dataSource)
        .Set(cameraLocator,
             HdRetainedTypedSampledDataSource<SdfPath>::New(sanitizedPath))
        .Finish();

    return prim;
}

void MhExternalCameraOverrideSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsAdded(entries);
}

void MhExternalCameraOverrideSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsRemoved(entries);
}

void MhExternalCameraOverrideSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsDirtied(entries);
}

} // namespace MAYAHYDRA_NS_DEF
