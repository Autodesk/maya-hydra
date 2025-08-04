// Copyright 2025 Autodesk
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

#include "flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h"

#include <pxr/imaging/hd/mergingSceneIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

DataProducerMergingSceneIndexProxy::DataProducerMergingSceneIndexProxy() : 
    _mergingSceneIndex(PXR_NS::HdMergingSceneIndex::New())
{
    TF_AXIOM(_mergingSceneIndex);
    _mergingSceneIndex->SetDisplayName("Data Producer Merging Scene Index");
}

void DataProducerMergingSceneIndexProxy::InsertSceneIndex(
    const PXR_NS::HdSceneIndexBaseRefPtr& inputScene,
    const PXR_NS::SdfPath&                activeInputSceneRoot)
{
    // Add the scene root before adding the scene index to enable filtering 
    // based on latest scene root during adding the scene index.
    _sceneRoots[inputScene] = activeInputSceneRoot;

    _mergingSceneIndex->AddInputScene(inputScene, SdfPath::AbsoluteRootPath());
}

void DataProducerMergingSceneIndexProxy::RemoveSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene
)
{
    _mergingSceneIndex->RemoveInputScene(inputScene);

    const auto& it = _sceneRoots.find(inputScene);
    if (it != _sceneRoots.cend()) {
        _sceneRoots.erase(it);
    }
}

HdSceneIndexBaseRefPtr DataProducerMergingSceneIndexProxy::GetMergingSceneIndex() const
{
    return _mergingSceneIndex;
}

std::set<PXR_NS::SdfPath> DataProducerMergingSceneIndexProxy::GetSceneRoots() const
{
    std::set<PXR_NS::SdfPath> sceneRoot;
    for (auto it = _sceneRoots.cbegin(); it != _sceneRoots.cend(); it++) {
        sceneRoot.emplace(it->second);
    }
    return sceneRoot;
}

}//end of namespace FVP_NS_DEF
