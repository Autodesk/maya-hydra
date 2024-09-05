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


PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _IsALight(const TfToken& primType)
{
    static const TfTokenVector & lightsToken = HdLightPrimTypeTokens();//Get all light tokens from USD
    return lightsToken.end() != std::find(lightsToken.begin(), lightsToken.end(), primType);
}

void _DisableLight(HdSceneIndexPrim& prim)
{
    //We don't set the intensity to 0 as for domelights this makes disappear the geometry
    HdContainerDataSourceEditor editor(prim.dataSource);
    editor.Set(
        HdLightSchema::GetDefaultLocator().Append(HdLightTokens->ambient),
        HdRetainedTypedSampledDataSource<float>::New(0.0f)
    );
    editor.Set(
        HdLightSchema::GetDefaultLocator().Append(HdLightTokens->diffuse),
        HdRetainedTypedSampledDataSource<float>::New(0.f)
    );
    editor.Set(
        HdLightSchema::GetDefaultLocator().Append(HdLightTokens->specular),
        HdRetainedTypedSampledDataSource<float>::New(0.f)
    );

    prim.dataSource = editor.Finish();
}

} // end of anonymous namespace

/// This is a filtering scene index that manages lights primitives

namespace FVP_NS_DEF {

LightsManagementSceneIndex::LightsManagementSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const PathInterface& pathInterface, const SdfPath& defaultLightPath) 
    : ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex)
    ,_defaultLightPath(defaultLightPath)
    , _pathInterface(pathInterface)
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
        if (_IsALight(primType)) {
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
     if (! _IsALight(primType)) {
         return prim;//return any non light primitive
     }
     
     //This is a light
     switch (_lightingMode) {
         case LightingMode::kNoLighting: {
             _DisableLight(prim);
             return prim;
         } break;
         default:
         case LightingMode::kSceneLighting: {
             return prim;
         } break;
         case LightingMode::kDefaultLighting: {
             if (! _IsDefaultLight(primPath)){
                 _DisableLight(prim);
             }
             return prim;//This is also handled in renderOverride.cpp with the _hasDefaultLighting member
         } break;
         case LightingMode::kSelectedLightsOnly: {
             const Ufe::GlobalSelection::Ptr& globalUfeSelection = Ufe::GlobalSelection::get();
             const Ufe::Selection&            ufeSelection = *globalUfeSelection;
             if (0 == ufeSelection.size() > 0) {
                 // Nothing is selected
                 _DisableLight(prim);
                 return prim;
             }
             
             //Convert ufe selection to SdfPath
             SdfPathVector selectedLightsSdfPath;
             for (const auto& snItem : ufeSelection) {
                 auto primSelections = _pathInterface.UfePathToPrimSelections(snItem->path());
                 for (const auto& primSelection : primSelections) {
                     selectedLightsSdfPath.push_back(primSelection.primPath);
                 }
             }
             const bool isSelected = selectedLightsSdfPath.end()
                 != std::find(selectedLightsSdfPath.begin(),
                              selectedLightsSdfPath.end(),
                              primPath);

             if (! isSelected) {
                 _DisableLight(prim);
             }

             return prim;
         } break;
     }

     return prim;
}

}//end of namespace FVP_NS_DEF
