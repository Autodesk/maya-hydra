//
// Copyright 2022 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
// Copyright 2023 Autodesk
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
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"
#include "flowViewport/sceneIndex/fvpMergingSceneIndex.h"

//Hydra headers
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/renderDelegate.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Copy-pasted and adapted from USD 0.23.08 pxr/imaging/hd/renderIndex.cpp.
// PPT, 1-Sep-2023.
HdSceneIndexBaseRefPtr
_GetInputScene(const HdPrefixingSceneIndexRefPtr &prefixingScene)
{
    const auto inputScenes = prefixingScene->GetInputScenes();
    if (inputScenes.size() == 1) {
        return inputScenes[0];
    }
    TF_CODING_ERROR("Expected exactly one scene index from "
                    "HdPrefixingSceneIndex::GetInputScenes");
    return TfNullPtr;
}

}

namespace FVP_NS_DEF {

RenderIndexProxy::RenderIndexProxy(PXR_NS::HdRenderIndex* renderIndex) :
    _renderIndex(renderIndex), _mergingSceneIndex(MergingSceneIndex::New())
{
    TF_AXIOM(_renderIndex);
    TF_AXIOM(_mergingSceneIndex);
    _mergingSceneIndex->SetDisplayName("Flow Viewport Merging Scene Index");
}

void RenderIndexProxy::InsertSceneIndex(
    const PXR_NS::HdSceneIndexBaseRefPtr& inputScene,
    const PXR_NS::SdfPath&                scenePathPrefix,
    bool                                  needsPrefixing /* = true */
)
{
    // Copy-pasted and adapted from USD 0.23.08
    // HdRenderIndex::InsertSceneIndex() code, to preserve prefixing scene
    // index creation capability.  PPT, 31-Aug-2023.
    auto resolvedScene = inputScene;
    if (needsPrefixing && scenePathPrefix != SdfPath::AbsoluteRootPath()) {
        resolvedScene = HdPrefixingSceneIndex::New(inputScene, scenePathPrefix);
    }

    _mergingSceneIndex->AddInputScene(resolvedScene, scenePathPrefix);
}
    
void RenderIndexProxy::RemoveSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene
)
{
    if (nullptr == inputScene || nullptr == _mergingSceneIndex){
        return;
    }

    // Copy-pasted and adapted from USD 0.23.08
    // HdRenderIndex::RemoveSceneIndex() code, to preserve prefixing scene
    // index removal capability.  PPT, 1-Sep-2023.

    const auto resolvedScenes = _mergingSceneIndex->GetInputScenes();

    // Two cases:
    // - Given scene index was added by InsertSceneIndex with
    //   scenePathPrefix = "/". We find it just by going over the
    //   input scenes of _mergingSceneIndex.
    // - Given scene index was added by InsertSceneIndex with
    //   non-trivial scenePathPrefix. We need to find the HdPrefixingSceneIndex
    //   among the input scenes of _mergingSceneIndex that was constructed
    //   from the given scene index.
    for (const auto& resolvedScene : resolvedScenes) {
        if (inputScene == resolvedScene) {
            _mergingSceneIndex->RemoveInputScene(resolvedScene);
            return;
        }
        if (HdPrefixingSceneIndexRefPtr const prefixingScene =
                TfDynamic_cast<HdPrefixingSceneIndexRefPtr>(resolvedScene)) {
            if (inputScene == _GetInputScene(prefixingScene)) {
                _mergingSceneIndex->RemoveInputScene(resolvedScene);
                return;
            }
        }
    }
}

HdSceneIndexBaseRefPtr RenderIndexProxy::GetMergingSceneIndex() const
{
    return _mergingSceneIndex;
}

HdRenderIndex* RenderIndexProxy::GetRenderIndex() const 
{ 
    return _renderIndex;
}

std::string RenderIndexProxy::GetRendererDisplayName() const 
{
    static std::string empty;

    if (! _renderIndex){
        return empty;
    }
    auto rd = _renderIndex->GetRenderDelegate();
    if (! rd){
        return empty;
    }
    
    return rd->GetRendererDisplayName();
}

}//end of namespace FVP_NS_DEF