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
#include "fvpReprSelectorSceneIndex.h"
#include "flowViewport/fvpUtils.h"

//USD/Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>

// This class is a filtering scene index that that applies a different RepSelector on geometries (such as wireframe or wireframe on shaded)
// and also applies an overrideWireframecolor for HdStorm 

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
//Handle primsvars:overrideWireframeColor in Storm for wireframe selection highlighting color
TF_DEFINE_PRIVATE_TOKENS(
     _primVarsTokens,
 
     (overrideWireframeColor)    // Works in HdStorm to override the wireframe color
 );

//refined means that it supports a "refineLevel" attribute in the displayStyle to get a more refined drawing, valid range is from 0 to 8

//Wireframe on surface refined
const HdRetainedContainerDataSourceHandle sRefinedWireframeOnShadedDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWireOnSurf, TfToken(), TfToken() })));

//Wireframe on surface not refined
const HdRetainedContainerDataSourceHandle sdWireframeOnShadedDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->wireOnSurf, TfToken(), TfToken() })));

//Wireframe refined
const HdRetainedContainerDataSourceHandle sWireframeDisplayStyleDataSource
    = HdRetainedContainerDataSource::New(
        HdLegacyDisplayStyleSchemaTokens->displayStyle,
        HdRetainedContainerDataSource::New(
            HdLegacyDisplayStyleSchemaTokens->reprSelector,
            HdRetainedTypedSampledDataSource<VtArray<TfToken>>::New(
                { HdReprTokens->refinedWire, TfToken(), TfToken() })));


}//End of namespace

ReprSelectorSceneIndex::ReprSelectorSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface) 
    : ParentClass(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex),
    _wireframeColorInterface(wireframeColorInterface)
{
    TF_AXIOM(_wireframeColorInterface);
}

void ReprSelectorSceneIndex::SetReprType(RepSelectorType reprType, bool needsReprChanged)
{
    switch (reprType){
        case RepSelectorType::WireframeRefined:
            _wireframeTypeDataSource = sWireframeDisplayStyleDataSource;
            break;
        case RepSelectorType::WireframeOnSurface:
            _wireframeTypeDataSource = sdWireframeOnShadedDisplayStyleDataSource;
            break;
        case RepSelectorType::WireframeOnSurfaceRefined:
            _wireframeTypeDataSource = sRefinedWireframeOnShadedDisplayStyleDataSource;
            break;
        case RepSelectorType::Default:
            break;
    }
    
    const HdDataSourceLocatorSet locators{
    HdLegacyDisplayStyleSchema::GetDefaultLocator(),
    HdPrimvarsSchema::GetDefaultLocator()
    };
    _needsReprChanged = needsReprChanged;
    _DirtyAllPrims(locators);
}

void
ReprSelectorSceneIndex::_DirtyAllPrims(
    const HdDataSourceLocatorSet locators)
{
    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath &path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        entries.push_back({path, locators});
    }
    _SendPrimsDirtied(entries);
}

HdSceneIndexPrim ReprSelectorSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if ( (prim.dataSource && !_isExcluded(primPath)) && 
         (prim.primType == HdPrimTypeTokens->mesh) && _needsReprChanged ) {

        //Edit the dataSource as an overlay will not replace any existing attribute value.
        // So we need to edit the _primVarsTokens->overrideWireframeColor attribute as they may already exist in the prim
        auto edited = HdContainerDataSourceEditor(prim.dataSource);

        //Edit the override wireframe color
        edited.Set(HdPrimvarsSchema::GetDefaultLocator().Append(_primVarsTokens->overrideWireframeColor),
                        Fvp::PrimvarDataSource::New(
                            HdRetainedTypedSampledDataSource<VtVec4fArray>::New(
                                                VtVec4fArray{_wireframeColorInterface->getWireframeColor(primPath)}),
                                                HdPrimvarSchemaTokens->constant,
                                                HdPrimvarSchemaTokens->color));
        //Edit the cull style
        edited.Set(HdLegacyDisplayStyleSchema::GetCullStyleLocator(),
                        HdRetainedTypedSampledDataSource<TfToken>::New(HdCullStyleTokens->nothing));//No culling
        prim.dataSource = HdOverlayContainerDataSource::New({ edited.Finish(), _wireframeTypeDataSource});
    }
    return prim;
}

}//end of namespace FVP_NS_DEF
