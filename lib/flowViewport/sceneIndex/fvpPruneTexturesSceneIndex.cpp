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
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (UsdPreviewSurface)
    (ND_standard_surface_surfaceshader)
    (ND_open_pbr_surface_surfaceshader)
    // Surface nodes that carry this parameter (set to true) keep their
    // texture connections even when the viewport "Textured" mode is off.
    (_alwaysShowTextures)
);

namespace {

void
_PruneTexturesFromMatNetwork(HdMaterialNetworkInterface *networkInterface)
{
    if (!networkInterface) {
        return;
    }
    const TfTokenVector nodeNames = networkInterface->GetNodeNames();
    for (TfToken const &nodeName : nodeNames) {
        const TfToken nodeType = networkInterface->GetNodeType(nodeName);
        if (nodeType == _tokens->ND_standard_surface_surfaceshader ||
            nodeType == _tokens->ND_open_pbr_surface_surfaceshader ||
            nodeType == _tokens->UsdPreviewSurface) {
            // Materials that must always display their textures (e.g. image
            // planes) set this parameter to opt out of pruning.
            VtValue alwaysShow = networkInterface->GetNodeParameterValue(
                nodeName, _tokens->_alwaysShowTextures);
            if (alwaysShow.IsHolding<bool>() && alwaysShow.UncheckedGet<bool>()) {
                continue;
            }

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
    const HdSceneIndexBaseRefPtr &inputSceneIndex,
    bool pruneTextures)
{    
    return TfCreateRefPtr(
        new PruneTexturesSceneIndex(inputSceneIndex, pruneTextures));
}

void
PruneTexturesSceneIndex::MarkTexturesDirty(bool pruneTextures)
{
    _pruneTextures = pruneTextures;

    // Storm does not re-resolve a mesh's primvar requirements from a material dirty alone, so
    // toggling the viewport's Textured mode needs more than the material locator to take effect.
    //
    // The primvars entry is the long-standing HYDRA-1061 workaround and is sufficient for a
    // UsdPreviewSurface network. It is NOT sufficient for a MaterialX network: the surface shader
    // is code-generated, and the UV primvar it consumes through a geompropvalue node is only
    // re-requested when the rprim re-initialises its repr -- which is what the displayStyle dirty
    // triggers. Without it the restored texture connections sample a UV set the mesh no longer
    // provides, and the surface renders black.
    //
    // That displayStyle dirty used to arrive by accident: ReprSelectorSceneIndex::SetReprType()
    // fired on any display-style change, including the Textured bit, and dirties every prim with
    // {displayStyle, primvars, purpose, visibility, xform}. Once that push became conditional on
    // the repr actually changing, the texture toggle lost its invalidation and testTexturedMode
    // began rendering a black sphere on USD 24.11. Requesting it here makes the toggle
    // self-sufficient rather than dependent on an unrelated scene index firing on the same frame.
    const HdDataSourceLocatorSet locators {
        HdMaterialSchema::GetDefaultLocator().Append(HdMaterialSchemaTokens->material),
        // Workaround for HYDRA-1061, see https://forum.aousd.org/t/primvars-and-material-dirtying-issue-in-storm/1675
        HdPrimvarsSchema::GetDefaultLocator(),
        HdLegacyDisplayStyleSchema::GetDefaultLocator()
    };

    _DirtyAllPrims(locators);
}

PruneTexturesSceneIndex::PruneTexturesSceneIndex(
    HdSceneIndexBaseRefPtr const &inputSceneIndex,
    bool pruneTextures)
  : HdMaterialFilteringSceneIndexBase(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex)
  , _pruneTextures(pruneTextures)
{   
}

PruneTexturesSceneIndex::FilteringFnc 
PruneTexturesSceneIndex::_GetFilteringFunction() const
{
    return _pruneTextures ? 
        _PruneTexturesFromMatNetwork : [](HdMaterialNetworkInterface*){};
}

void
PruneTexturesSceneIndex::_DirtyAllPrims(
    const HdDataSourceLocatorSet locators)
{
    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath &path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        entries.push_back({path, locators});
    }
    _SendPrimsDirtied(entries);
}

} //end of namespace FVP_NS_DEF
