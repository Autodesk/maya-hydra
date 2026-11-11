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

void MaterialCreationInterfaceImp::RegisterMaterialCreationCallback(const MaterialCreationCallback& callback)
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _callback = callback;
}

void MaterialCreationInterfaceImp::UnregisterMaterialCreationCallback()
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    _callback = nullptr;
}

VtValue MaterialCreationInterfaceImp::CreateMaterial(const SdfPath& materialId) const
{
    MaterialCreationCallback callback;
    {
        std::lock_guard<std::mutex> lock(_callbackMutex);
        callback = _callback;
    }
    return callback ? callback(materialId) : VtValue();
}

bool MaterialCreationInterfaceImp::HasMaterialCreationCallback() const
{
    std::lock_guard<std::mutex> lock(_callbackMutex);
    return static_cast<bool>(_callback);
}

} //End of namespace FVP_NS_DEF
