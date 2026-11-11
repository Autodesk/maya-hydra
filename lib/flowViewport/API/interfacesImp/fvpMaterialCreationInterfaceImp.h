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

#ifndef FLOW_VIEWPORT_API_INTERFACESIMP_MATERIAL_CREATION_INTERFACE_IMP_H
#define FLOW_VIEWPORT_API_INTERFACESIMP_MATERIAL_CREATION_INTERFACE_IMP_H

//Local headers
#include <flowViewport/api.h>
#include <flowViewport/API/fvpMaterialCreationInterface.h>

#include <mutex>

namespace FVP_NS_DEF {

///Is a singleton, use MaterialCreationInterfaceImp& MaterialCreationInterfaceImp::Get() to get an instance of that interface
class MaterialCreationInterfaceImp : public MaterialCreationInterface
{
public:
    virtual ~MaterialCreationInterfaceImp() = default;

    ///Interface accessor
    static FVP_API MaterialCreationInterfaceImp& Get();

    //From MaterialCreationInterface
    void RegisterMaterialCreationCallback(const MaterialCreationCallback& callback) override;
    void UnregisterMaterialCreationCallback() override;
    PXR_NS::VtValue CreateMaterial(const PXR_NS::SdfPath& materialId) const override;

    FVP_API bool HasMaterialCreationCallback() const;

private:
    MaterialCreationInterfaceImp() = default;

    mutable std::mutex       _callbackMutex;
    MaterialCreationCallback _callback;
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_INTERFACESIMP_MATERIAL_CREATION_INTERFACE_IMP_H
