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

//
// Example: demonstrates how a render delegate's scene index translates
// mayaCustomDagNode prims into renderer-specific types.
// This code has ZERO dependency on maya-hydra. It only uses standard OpenUSD
// APIs. The token names are a documented convention.
//
// Arnold (aiPhotometricLight -> sphereLight) is used as a concrete
// illustration. Any other render delegate (RenderMan, custom engines, etc.) would
// follow the exact same pattern: create its own scene index plugin,
// register it for its renderer name, and handle the Maya node types it
// recognises while passing through the rest.
//

#include "genericNodeTranslationSceneIndex.h"

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

// Documented convention tokens -- no maya-hydra dependency
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (mayaCustomDagNode)
    (mayaNode)
    (mayaTypeName)
    (mayaAttributes)
    ((aiPhotometricLight, "aiPhotometricLight"))
);

HdGenericNodeTranslationSceneIndexRefPtr
HdGenericNodeTranslationSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    return TfCreateRefPtr(
        new HdGenericNodeTranslationSceneIndex(inputSceneIndex));
}

HdGenericNodeTranslationSceneIndex::HdGenericNodeTranslationSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
{
}

HdGenericNodeTranslationSceneIndex::~HdGenericNodeTranslationSceneIndex() = default;

// Intercepts mayaCustomDagNode prims: reads the mayaNode data source to
// determine the Maya type name, then dispatches to the appropriate
// translation function. Prims with unrecognized types are passed through.
HdSceneIndexPrim
HdGenericNodeTranslationSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
    if (prim.primType != _tokens->mayaCustomDagNode) {
        return prim;
    }

    if (!prim.dataSource) {
        return prim;
    }

    auto mayaNodeDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(_tokens->mayaNode));
    if (!mayaNodeDs) {
        return prim;
    }

    auto typeNameDs = HdTypedSampledDataSource<TfToken>::Cast(
        mayaNodeDs->Get(_tokens->mayaTypeName));
    if (!typeNameDs) {
        return prim;
    }
    TfToken mayaTypeName = typeNameDs->GetTypedValue(0.0f);

    auto attrsDs = HdTypedSampledDataSource<VtDictionary>::Cast(
        mayaNodeDs->Get(_tokens->mayaAttributes));
    VtDictionary attrs;
    if (attrsDs) {
        attrs = attrsDs->GetTypedValue(0.0f);
    }

    if (mayaTypeName == _tokens->aiPhotometricLight) {
        return _TranslatePhotometricLight(prim, attrs);
    }

    return prim;
}

SdfPathVector
HdGenericNodeTranslationSceneIndex::GetChildPrimPaths(
    const SdfPath& primPath) const
{
    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

// Maps aiPhotometricLight Maya attributes to UsdLux tokens and re-types the
// prim as sphereLight. The Arnold render delegate's getLightType() detects
// inputs:shaping:ies:file and automatically creates a photometric_light node.
// The original xform and visibility data sources from maya-hydra are preserved
// via HdOverlayContainerDataSource.
HdSceneIndexPrim
HdGenericNodeTranslationSceneIndex::_TranslatePhotometricLight(
    const HdSceneIndexPrim& inputPrim,
    const VtDictionary& mayaAttrs) const
{
    auto getFloat = [&](const char* key, float def) -> float {
        auto it = mayaAttrs.find(key);
        if (it != mayaAttrs.end() && it->second.IsHolding<float>())
            return it->second.UncheckedGet<float>();
        if (it != mayaAttrs.end() && it->second.IsHolding<double>())
            return static_cast<float>(it->second.UncheckedGet<double>());
        return def;
    };
    auto getVec3f = [&](const char* key, GfVec3f def) -> GfVec3f {
        auto it = mayaAttrs.find(key);
        if (it != mayaAttrs.end() && it->second.IsHolding<GfVec3f>())
            return it->second.UncheckedGet<GfVec3f>();
        return def;
    };
    auto getString = [&](const char* key, std::string def) -> std::string {
        auto it = mayaAttrs.find(key);
        if (it != mayaAttrs.end() && it->second.IsHolding<std::string>())
            return it->second.UncheckedGet<std::string>();
        return def;
    };
    auto getBool = [&](const char* key, bool def) -> bool {
        auto it = mayaAttrs.find(key);
        if (it != mayaAttrs.end() && it->second.IsHolding<bool>())
            return it->second.UncheckedGet<bool>();
        return def;
    };

    float       intensity   = getFloat("intensity", 1.0f);
    GfVec3f     color       = getVec3f("color", GfVec3f(1.0f));
    float       exposure    = getFloat("aiExposure", 0.0f);
    std::string iesFile     = getString("aiFilename", "");
    float       radius      = getFloat("aiRadius", 0.0f);
    bool        normalize   = getBool("aiNormalize", true);
    float       diffuse     = getFloat("aiDiffuse", 1.0f);
    float       specular    = getFloat("aiSpecular", 1.0f);
    bool        castShadows = getBool("aiCastShadows", true);
    GfVec3f     shadowColor = getVec3f("aiShadowColor", GfVec3f(0.0f));

    HdSceneIndexPrim result;
    result.primType = HdPrimTypeTokens->sphereLight;

    // Build UsdLux-compatible data source overlaid on the original
    // (keeps xform, visibility from maya-hydra).
    result.dataSource = HdOverlayContainerDataSource::New(
        HdRetainedContainerDataSource::New(
            UsdLuxTokens->inputsIntensity,
            HdRetainedTypedSampledDataSource<float>::New(intensity),
            UsdLuxTokens->inputsColor,
            HdRetainedTypedSampledDataSource<GfVec3f>::New(color),
            UsdLuxTokens->inputsExposure,
            HdRetainedTypedSampledDataSource<float>::New(exposure),
            UsdLuxTokens->inputsShapingIesFile,
            HdRetainedTypedSampledDataSource<SdfAssetPath>::New(
                SdfAssetPath(iesFile)),
            UsdLuxTokens->inputsRadius,
            HdRetainedTypedSampledDataSource<float>::New(radius),
            UsdLuxTokens->inputsNormalize,
            HdRetainedTypedSampledDataSource<bool>::New(normalize),
            UsdLuxTokens->inputsDiffuse,
            HdRetainedTypedSampledDataSource<float>::New(diffuse),
            UsdLuxTokens->inputsSpecular,
            HdRetainedTypedSampledDataSource<float>::New(specular),
            UsdLuxTokens->inputsShadowEnable,
            HdRetainedTypedSampledDataSource<bool>::New(castShadows),
            UsdLuxTokens->inputsShadowColor,
            HdRetainedTypedSampledDataSource<GfVec3f>::New(shadowColor)),
        inputPrim.dataSource);

    return result;
}

// For added prims, translate the prim type so that downstream observers
// (including the render delegate) see the translated type rather than
// the raw mayaCustomDagNode type.
void HdGenericNodeTranslationSceneIndex::_PrimsAdded(
    const HdSceneIndexBase& /*sender*/,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    if (!_IsObserved()) return;

    HdSceneIndexObserver::AddedPrimEntries translated;
    translated.reserve(entries.size());
    for (const auto& entry : entries) {
        if (entry.primType == _tokens->mayaCustomDagNode) {
            auto prim = GetPrim(entry.primPath);
            translated.push_back({ entry.primPath, prim.primType });
        } else {
            translated.push_back(entry);
        }
    }
    _SendPrimsAdded(translated);
}

void HdGenericNodeTranslationSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase& /*sender*/,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsRemoved(entries);
}

void HdGenericNodeTranslationSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase& /*sender*/,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    if (!_IsObserved()) return;
    _SendPrimsDirtied(entries);
}

} // namespace MAYAHYDRA_NS_DEF
