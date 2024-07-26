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

#include "flowViewport/sceneIndex/fvpDefaultMaterialSceneIndex.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/retainedDataSource.h>

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace{
    static const TfToken purposes[] = { HdMaterialBindingsSchemaTokens->allPurpose };
}

// static
DefaultMaterialSceneIndexRefPtr
DefaultMaterialSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    const PXR_NS::SdfPath& defaultMaterialPath,
    const PXR_NS::SdfPathVector& defaultMaterialExclusionList)
{    
    return TfCreateRefPtr(
        new DefaultMaterialSceneIndex(inputSceneIndex, defaultMaterialPath, defaultMaterialExclusionList));
}

DefaultMaterialSceneIndex::DefaultMaterialSceneIndex(
    HdSceneIndexBaseRefPtr const &inputSceneIndex, 
    const PXR_NS::SdfPath& defaultMaterialPath,
    const PXR_NS::SdfPathVector& defaultMaterialExclusionList)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex),
    _defaultMaterialPath(defaultMaterialPath),
    _defaultMaterialExclusionList(defaultMaterialExclusionList)
{   
}

HdSceneIndexPrim DefaultMaterialSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (! _isEnabled){
        return prim;
    }

    _SetDefaultMaterial(prim);

    return prim;
}

bool DefaultMaterialSceneIndex::_ShouldWeApplyTheDefaultMaterial(const HdSceneIndexPrim& prim)const
{
    // Only for meshes so far
    if (HdPrimTypeTokens->mesh != prim.primType) {
        return false;
    }

    // Check if it has a material which is not in the exclusion list
    HdMaterialBindingsSchema bindings = HdMaterialBindingsSchema::GetFromParent(prim.dataSource);
    HdMaterialBindingSchema  binding = bindings.GetMaterialBinding();
    HdPathDataSourceHandle   bindingPathDS = binding.GetPath();
    if (bindingPathDS) { // If a mesh prim has no material( bindingPathDS is empty), apply anyway the default material
        const SdfPath materialPath = bindingPathDS->GetTypedValue(0.0f);
        auto    foundExcludedMaterialPath = std::find(
            _defaultMaterialExclusionList.cbegin(),
            _defaultMaterialExclusionList.cend(),
            materialPath);
        if (foundExcludedMaterialPath != _defaultMaterialExclusionList.cend()) {
            return false; // materialPath is in the exclusion list !
        }
    }

    return true;
}

void DefaultMaterialSceneIndex::_SetDefaultMaterial(HdSceneIndexPrim& inoutPrim)const
{
    static HdDataSourceBaseHandle const materialBindingSources[]
        = { HdMaterialBindingSchema::Builder()
                .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(_defaultMaterialPath))
                .Build() };
    
    if (_ShouldWeApplyTheDefaultMaterial(inoutPrim)) {
        inoutPrim.dataSource = HdContainerDataSourceEditor(inoutPrim.dataSource)
            .Set(
                HdMaterialBindingsSchema::GetDefaultLocator(),
                HdMaterialBindingsSchema::BuildRetained(TfArraySize(purposes), purposes, materialBindingSources)
                )
            .Finish();
    }
}

void DefaultMaterialSceneIndex::Enable(bool enable) 
{ 
    if (_isEnabled == enable){
        return;
    }
    
    _isEnabled = enable; 
    _MarkMaterialsDirty();
}

void DefaultMaterialSceneIndex::_MarkMaterialsDirty()
{ 
    static const auto locator = HdMaterialBindingsSchema::GetDefaultLocator();

    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
        // Dirty only prims where we should apply the default material
        if (_ShouldWeApplyTheDefaultMaterial(prim)) {
            entries.push_back({ primPath, locator });
        }
    }
    _SendPrimsDirtied(entries);
}

} //end of namespace FVP_NS_DEF
