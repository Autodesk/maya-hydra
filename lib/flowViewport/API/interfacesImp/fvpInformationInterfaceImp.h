//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#ifndef FLOW_VIEWPORT_API_INTERFACESIMP_INFORMATION_INTERFACE_IMP_H
#define FLOW_VIEWPORT_API_INTERFACESIMP_INFORMATION_INTERFACE_IMP_H

//Local headers
#include <flowViewport/api.h>
#include <flowViewport/API/fvpInformationInterface.h>
#include <flowViewport/API/fvpInformationClient.h>

namespace FVP_NS_DEF {

///Is a singleton, use InformationInterfaceImp& _getInformationInterfaceImp(void) below to get an instance of that interface
class InformationInterfaceImp :  public InformationInterface
{
public:
    InformationInterfaceImp()             = default;
    virtual ~InformationInterfaceImp()    = default;

    ///Interface accessor
    static FVP_API InformationInterfaceImp& Get();

    //From InformationInterface
    void RegisterInformationClient(const InformationClient& client)override;
    void UnregisterInformationClient(const InformationClient& client)override;
    void GetViewportsInformation(std::set<const ViewportInformation*>& outHydraviewportInformationSet) const override;
   
    void SceneIndexAdded(const InformationInterface::ViewportInformation& _viewportInfo);
    void SceneIndexRemoved(const InformationInterface::ViewportInformation& viewportInfo);
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_INTERFACESIMP_INFORMATION_INTERFACE_IMP_H