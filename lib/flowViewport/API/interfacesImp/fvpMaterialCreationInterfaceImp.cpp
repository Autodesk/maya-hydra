//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//Local headers
#include "fvpMaterialCreationInterfaceImp.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

MaterialCreationInterface& MaterialCreationInterface::Get()
{
    return MaterialCreationInterfaceImp::Get();
}

MaterialCreationInterfaceImp& MaterialCreationInterfaceImp::Get()
{
    static MaterialCreationInterfaceImp theInterface;
    return theInterface;
}

void MaterialCreationInterfaceImp::RegisterMaterialCreationCallback(
    const std::string&              name,
    const MaterialCreationCallback& callback)
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _callbacks[name] = callback;
}

void MaterialCreationInterfaceImp::UnregisterMaterialCreationCallback(const std::string& name)
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _callbacks.erase(name);
}

VtValue MaterialCreationInterfaceImp::CreateMaterial(const std::string& name, const SdfPath& materialId) const
{
    MaterialCreationCallback callback;
    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        auto it = _callbacks.find(name);
        if (it != _callbacks.end()) {
            callback = it->second;
        }
    }
    return callback ? callback(materialId) : VtValue();
}

bool MaterialCreationInterfaceImp::HasMaterialCreationCallback(const std::string& name) const
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    auto it = _callbacks.find(name);
    return it != _callbacks.end() && static_cast<bool>(it->second);
}

} //End of namespace FVP_NS_DEF
