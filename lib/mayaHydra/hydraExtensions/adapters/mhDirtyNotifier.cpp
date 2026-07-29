// Copyright 2026 Autodesk
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
#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

DirtyNotifier::DirtyNotifier(PXR_NS::MayaHydraAdapter* adapter)
    : Fvp::DirtyNotifier(*adapter->GetMayaHydraSceneIndex(), adapter->GetID())
{}

DirtyNotifier& DirtyNotifier::dirtyUVs()
{
    dirtyPrimvar(MayaHydraAdapterTokens->st);
    return *this;
}

DirtyNotifier& DirtyNotifier::dirtyTangents()
{
    dirtyPrimvar(MayaHydraAdapterTokens->tangents);
    return *this;
}

} // namespace MAYAHYDRA_NS_DEF
