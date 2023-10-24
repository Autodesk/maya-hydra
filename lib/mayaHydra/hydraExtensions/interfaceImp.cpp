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

#include "interfaceImp.h"

PXR_NAMESPACE_OPEN_SCOPE

MayaHydraLibInterface& GetMayaHydraLibInterface()
{
    static MayaHydraLibInterfaceImp libInterface;
    return libInterface;
}

void MayaHydraLibInterfaceImp::RegisterTerminalSceneIndex(HdSceneIndexBasePtr sceneIndex)
{
    auto foundSceneIndex = std::find(_sceneIndices.begin(), _sceneIndices.end(), sceneIndex);
    if (foundSceneIndex == _sceneIndices.end()) {
        _sceneIndices.push_back(sceneIndex);
    }
}

void MayaHydraLibInterfaceImp::UnregisterTerminalSceneIndex(HdSceneIndexBasePtr sceneIndex)
{
    auto foundSceneIndex = std::find(_sceneIndices.begin(), _sceneIndices.end(), sceneIndex);
    if (foundSceneIndex != _sceneIndices.end()) {
        _sceneIndices.erase(foundSceneIndex);
    }
}

void MayaHydraLibInterfaceImp::ClearTerminalSceneIndices() { _sceneIndices.clear(); }

const SceneIndicesVector& MayaHydraLibInterfaceImp::GetTerminalSceneIndices() const
{
    return _sceneIndices;
}

PXR_NAMESPACE_CLOSE_SCOPE
