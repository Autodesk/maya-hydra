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

#include "flowViewport/sceneIndex/fvpPruneTexturesSceneIndex.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/materialSchema.h>

#include <iostream>
namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (UsdPreviewSurface)
    (ND_standard_surface_surfaceshader)
);

namespace {

void
_PruneTexturesFromMatNetwork(
    HdMaterialNetworkInterface *networkInterface)
{
    if (!networkInterface) {
        return;
    }
    const TfTokenVector nodeNames = networkInterface->GetNodeNames();
    for (TfToken const &nodeName : nodeNames) {
        const TfToken nodeType = networkInterface->GetNodeType(nodeName);
        if (nodeType == _tokens->ND_standard_surface_surfaceshader ||
            nodeType == _tokens->UsdPreviewSurface) {                
            // Look for incoming connection(textures) to surface shader params
            TfTokenVector inputConnections = networkInterface->GetNodeInputConnectionNames(nodeName);
            for (TfToken const &connection : inputConnections) {
                // Trivially remove all input connections to match Maya VP2 behavior
                networkInterface->DeleteNodeInputConnection(nodeName, connection);
            }
        }
    }
}

} // Anonymous namespace

// static
PruneTexturesSceneIndexRefPtr
PruneTexturesSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
{    
    return TfCreateRefPtr(
        new PruneTexturesSceneIndex(inputSceneIndex));
}

void
PruneTexturesSceneIndex::MarkTexturesDirty(bool isTextured)
{
    _needsTexturesPruned = isTextured;
    const HdDataSourceLocatorSet locators(
    HdMaterialSchema::GetDefaultLocator()
        .Append(HdMaterialSchemaTokens->material));

    _DirtyAllPrims(locators);
}

PruneTexturesSceneIndex::PruneTexturesSceneIndex(
    HdSceneIndexBaseRefPtr const &inputSceneIndex)
  : HdMaterialFilteringSceneIndexBase(inputSceneIndex)
{   
}

PruneTexturesSceneIndex::FilteringFnc 
PruneTexturesSceneIndex::_GetFilteringFunction() const
{
    return !_needsTexturesPruned ? 
            _PruneTexturesFromMatNetwork :
            [](HdMaterialNetworkInterface*){};
}

void
PruneTexturesSceneIndex::_DirtyAllPrims(
    const HdDataSourceLocatorSet locators)
{
    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath &path : HdSceneIndexPrimView(_GetInputSceneIndex())) {
        entries.push_back({path, locators});
    }
    _SendPrimsDirtied(entries);
}

} //end of namespace FVP_NS_DEF