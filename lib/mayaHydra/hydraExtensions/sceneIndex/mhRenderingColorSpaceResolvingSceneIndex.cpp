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

#include "mhRenderingColorSpaceResolvingSceneIndex.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneGlobalsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <ufe/colorManagementHandler.h>

#include <ufeExtensions/Global.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MhRenderingColorSpaceResolvingSceneIndexRefPtr
MhRenderingColorSpaceResolvingSceneIndex::New(const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(new MhRenderingColorSpaceResolvingSceneIndex(inputSceneIndex));
}

MhRenderingColorSpaceResolvingSceneIndex::MhRenderingColorSpaceResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : Fvp::RenderingColorSpaceResolvingSceneIndex(inputSceneIndex)
{
}

/// When resolving the rendering color space, the following logic is applied:
/// 1. If the rendering color space is authored on the active render settings prim, use that value.
///    a. If authored but different from the value set in Maya prefs, print a warning message.
///    Use the authored value.
///    b. If authored but unrecognized (no corresponding value in config.ocio), print a warning
///    message. Use the Maya prefs value.
/// 2. If the field is empty or all other case, fallback to Maya prefs.
///
HdSceneIndexPrim MhRenderingColorSpaceResolvingSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);

    if (!prim.dataSource) {
        return prim;
    }

    auto sceneGlobals = HdSceneGlobalsSchema::GetFromSceneIndex(GetInputSceneIndex());
    if (!sceneGlobals.IsDefined()) {
        return prim;
    }

    // Check if the argument prim is the active render settings prim.
    auto activeRenderSettingsDs = sceneGlobals.GetActiveRenderSettingsPrim();
    if (!activeRenderSettingsDs || activeRenderSettingsDs->GetTypedValue(0) != primPath) {
        return prim;
    }

    auto usdRenderSettingsDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(HdRenderSettingsSchemaTokens->renderSettings));
    if (!usdRenderSettingsDs) {
        return prim;
    }

    auto renderingColorSpaceDs = HdTokenDataSource::Cast(
        usdRenderSettingsDs->Get(HdRenderSettingsSchemaTokens->renderingColorSpace));
    if (!renderingColorSpaceDs) {
        return prim;
    }

    const std::string renderingColorSpaceFromMayaPrefs = GetRenderingColorSpaceFromDCC();
    if (renderingColorSpaceFromMayaPrefs.empty()) {
        return prim;
    }

    const TfToken rcs = renderingColorSpaceDs->GetTypedValue(0.0f);
    if (!rcs.IsEmpty()) {
        // If rendering color space is authored and valid with the OCIO config file, use it.
        if (IsKnownColorSpace(rcs.GetString())) {
            // Authored rendering color space doesn't match the one from Maya prefs.
            // Let the user know that we're using the one from the active USD render settings node.
            if (rcs.GetString() != renderingColorSpaceFromMayaPrefs) {
                TF_WARN(
                    "Authored rendering color space '%s' in the active USD render settings prim "
                    "doesn't match the value from Maya preferences '%s'. "
                    "Using the value from the active USD render settings prim.",
                    rcs.GetText(),
                    renderingColorSpaceFromMayaPrefs.c_str());
                return prim;
            }
            return prim;
        }
        // If rendering color space is authored but not valid with the OCIO config file, let the
        // user know that we're using the one from Maya prefs.
        TF_WARN(
            "Authored rendering color space '%s' in the active USD render settings prim is not "
            "known to the OCIO config file. "
            "Using the value from Maya preferences '%s'.",
            rcs.GetText(),
            renderingColorSpaceFromMayaPrefs.c_str());
    }

    // Rendering color space is not authored or the authored one is invalid.
    // Use the one from Maya prefs.
    HdDataSourceLocator renderingColorSpaceLocator(
        HdRenderSettingsSchemaTokens->renderSettings,
        HdRenderSettingsSchemaTokens->renderingColorSpace);

    prim.dataSource = HdContainerDataSourceEditor(prim.dataSource)
                          .Set(
                              renderingColorSpaceLocator,
                              HdRetainedTypedSampledDataSource<TfToken>::New(
                                  TfToken(renderingColorSpaceFromMayaPrefs)))
                          .Finish();

    return prim;
}

/// If color management is not enabled in Maya, this will return the default rendering color space
/// from the OCIO config file.
std::string MhRenderingColorSpaceResolvingSceneIndex::GetRenderingColorSpaceFromDCC() const
{
    const auto colorManagement
        = Ufe::ColorManagementHandler::colorManagementHandler(UfeExtensions::getMayaRunTimeId());
    if (!TF_VERIFY(colorManagement, "No color management handler registered for Maya runtime"))
        return {};

    return colorManagement->getRenderingSpaceName();
}

/// Check if the given color space is known to the OCIO config file.
bool MhRenderingColorSpaceResolvingSceneIndex::IsKnownColorSpace(
    const std::string& colorSpace) const
{
    const auto colorManagement
        = Ufe::ColorManagementHandler::colorManagementHandler(UfeExtensions::getMayaRunTimeId());
    if (!TF_VERIFY(colorManagement, "No color management handler registered for Maya runtime"))
        return false;
    return colorManagement->isKnownColorSpace(colorSpace);
}

} // namespace MAYAHYDRA_NS_DEF
