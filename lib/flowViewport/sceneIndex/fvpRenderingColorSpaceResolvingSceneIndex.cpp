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

#include "fvpRenderingColorSpaceResolvingSceneIndex.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/renderSettingsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneGlobalsSchema.h>
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

RenderingColorSpaceResolvingSceneIndex::RenderingColorSpaceResolvingSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
{
}

/// When resolving the rendering color space, the following logic is applied:
/// 1. If the rendering color space is authored on the active render settings prim, use that value.
///    a. If authored but different from the value set in the application  preferences, print a warning message.
///    Use the authored value.
///    b. If authored but unrecognized (no corresponding value in config.ocio), print a warning
///    message. Use the application prefs value.
/// 2. If the field is empty or all other case, fallback to application prefs.
///
HdSceneIndexPrim RenderingColorSpaceResolvingSceneIndex::GetPrim(const SdfPath& primPath) const
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

    const std::string applicationRenderingColorSpace = GetApplicationRenderingColorSpace();
    if (applicationRenderingColorSpace.empty()) {
        return prim;
    }

    const TfToken authoredRenderingColorSpace = renderingColorSpaceDs->GetTypedValue(0.0f);
    if (!authoredRenderingColorSpace.IsEmpty()) {
        // If rendering color space is authored, resolve it using 1., 1.a. and 1.b.
        if (UseAuthoredRenderingColorSpace(authoredRenderingColorSpace, applicationRenderingColorSpace)) {
            return prim;
        }
    }

    // Rendering color space is not authored or the authored one is invalid.
    // Use the one from application preferences.
    HdDataSourceLocator renderingColorSpaceLocator(
        HdRenderSettingsSchemaTokens->renderSettings,
        HdRenderSettingsSchemaTokens->renderingColorSpace);

    prim.dataSource = HdContainerDataSourceEditor(prim.dataSource)
                          .Set(
                              renderingColorSpaceLocator,
                              HdRetainedTypedSampledDataSource<TfToken>::New(
                                  TfToken(applicationRenderingColorSpace)))
                          .Finish();

    return prim;
}

void RenderingColorSpaceResolvingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved())
        return;
    _SendPrimsAdded(entries);
}

void RenderingColorSpaceResolvingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved())
        return;
    _SendPrimsRemoved(entries);
}

void RenderingColorSpaceResolvingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved())
        return;
    _SendPrimsDirtied(entries);
}

} // namespace FVP_NS_DEF
