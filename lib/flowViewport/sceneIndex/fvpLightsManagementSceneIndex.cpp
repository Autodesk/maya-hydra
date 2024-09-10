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

void _DisableLight(HdSceneIndexPrim& prim)
{
    HdContainerDataSourceEditor editor(prim.dataSource);
    //We don't set the intensity to 0 as for domelights this makes the geometry disappear
    for (const auto& t : { HdLightTokens->ambient, HdLightTokens->diffuse, HdLightTokens->specular }) {
        editor.Set(
            HdLightSchema::GetDefaultLocator().Append(t),
            HdRetainedTypedSampledDataSource<float>::New(0.0f));
    }
    
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
     if (! HdPrimTypeIsLight(primType)) {
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
             return prim;
         } break;
         case LightingMode::kSelectedLightsOnly: {
             const Ufe::Selection& ufeSelection = *Ufe::GlobalSelection::get();
             if (ufeSelection.empty()) {
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
