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
#include "fvpLightsManagementSceneIndex.h"

#include <flowViewport/selection/fvpPathMapperRegistry.h>

//USD/Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/glf/simpleLight.h>

//ufe
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>

//std
#include <array>


PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Helper function to extract a value from a light data source.
template <typename T>
T _GetLightData(const HdContainerDataSourceHandle& primDataSource, const TfToken& name)
{
    if (auto lightSchema = HdLightSchema::GetFromParent(primDataSource)) {
        if (auto dataSource
            = HdTypedSampledDataSource<T>::Cast(lightSchema.GetContainer()->Get(name))) {
            return dataSource->GetTypedValue(0.0f);
        }
    }

    return {};
}

void _DisableLight(HdSceneIndexPrim& prim)
{
    HdContainerDataSourceEditor editor(prim.dataSource);

    const bool isSimpleLight = prim.primType == HdPrimTypeTokens->simpleLight;

    if (isSimpleLight) {
        // A simple light contains in its params a GlfSimpleLight which needs to be disabled
        // by setting its diffuse, specular and ambient to 0
        GlfSimpleLight simpleLight
            = _GetLightData<GlfSimpleLight>(prim.dataSource, HdTokens->params);
        simpleLight.SetDiffuse(GfVec4f(0.0f));
        simpleLight.SetSpecular(GfVec4f(0.0f));
        simpleLight.SetAmbient(GfVec4f(0.0f));
        editor.Set(
            HdLightSchema::GetDefaultLocator().Append(HdTokens->params),
            HdRetainedTypedSampledDataSource<GlfSimpleLight>::New(simpleLight));
    } else {
        // We don't set the intensity to 0 as for domelights this makes the geometry disappear
        static const std::array<TfToken, 3> lightTokens
            = { HdLightTokens->ambient, HdLightTokens->diffuse, HdLightTokens->specular };
        for (const auto& token : lightTokens) {
            editor.Set(
                HdLightSchema::GetDefaultLocator().Append(token),
                HdRetainedTypedSampledDataSource<float>::New(0.0f));
        }
    }

    prim.dataSource = editor.Finish();
}

bool _IsPrimOrAncestorSelected(const SdfPath& primPath)
{
    const Ufe::Selection& ufeSelection = *Ufe::GlobalSelection::get();
    if (ufeSelection.empty()) {
        return false;
    }

    // Convert UFE selection to SdfPath
    SdfPathVector selectedSdfPath;
    for (const auto& snItem : ufeSelection) {
        auto primSelections = Fvp::ufePathToPrimSelections(snItem->path());
        for (const auto& primSelection : primSelections) {
            selectedSdfPath.push_back(primSelection.primPath);
        }
    }

    if (std::find(selectedSdfPath.cbegin(), selectedSdfPath.cend(), primPath)
        != selectedSdfPath.cend()) {
        return true;
    }

    for (const auto& selectedPath : selectedSdfPath) {
        if (primPath.HasPrefix(selectedPath)) {
            return true;
        }
    }

    return false;
}

} // end of anonymous namespace

/// This is a filtering scene index that manages lights primitives

namespace FVP_NS_DEF {

LightsManagementSceneIndex::LightsManagementSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const SdfPath& defaultLightPath) 
    : ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex)
    ,_defaultLightPath(defaultLightPath)
{
}

void LightsManagementSceneIndex::SetLightingMode(LightingMode lightingMode) 
{ 
    if (_lightingMode == lightingMode){
        return;
    }

    _lightingMode = lightingMode;
    _DirtyAllLightsPrims();
}

void LightsManagementSceneIndex::_DirtyAllLightsPrims()
{
    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath& path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        auto primType = GetInputSceneIndex()->GetPrim(path).primType;
        if (HdPrimTypeIsLight(primType)) {
            entries.push_back({ path, HdLightSchema::GetDefaultLocator() });
        }
    }
    _SendPrimsDirtied(entries);
}

bool LightsManagementSceneIndex::_IsDefaultLight(const SdfPath& primPath)const
{
    return primPath == _defaultLightPath;
}

HdSceneIndexPrim LightsManagementSceneIndex::GetPrim(const SdfPath& primPath) const
{
    auto prim = GetInputSceneIndex()->GetPrim(primPath);
    auto primType = prim.primType;

    if (!HdPrimTypeIsLight(primType)) {
        return prim; // Return any non-light primitive
    }

    if (_allLightsAreDisabled) {
        _DisableLight(prim);
        return prim;
    }

    // This is a light
    switch (_lightingMode) {
        case LightingMode::kNoLighting: 
            _DisableLight(prim); 
            break;

        case LightingMode::kSceneLighting:
            //Disable non active lights from maya native data
            if (_disabledLightsPrims.find(primPath) != _disabledLightsPrims.end()) {
                _DisableLight(prim);
            }
            break;

        case LightingMode::kDefaultLighting:
            if (!_IsDefaultLight(primPath)) {
                _DisableLight(prim);
            }
            break;

        case LightingMode::kSelectedLightsOnly: {
            const bool shouldBeUsedForLigthing = _IsPrimOrAncestorSelected(primPath);
            if (! shouldBeUsedForLigthing) {
                _DisableLight(prim);
            }
            break;
        }
    }

    return prim;
}

void LightsManagementSceneIndex::SetDisabledLightsPrims(
    const std::set<PXR_NS::SdfPath>& activeLightsPrims)
{
    if (_disabledLightsPrims != activeLightsPrims) {
        _disabledLightsPrims = activeLightsPrims;
        _DirtyAllLightsPrims();
    }
}


}//end of namespace FVP_NS_DEF
