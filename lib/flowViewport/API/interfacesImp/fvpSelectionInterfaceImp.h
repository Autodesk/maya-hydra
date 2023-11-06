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

#ifndef FLOW_VIEWPORT_API_INTERFACESIMP_SELECTIONINTERFACEIMP_H
#define FLOW_VIEWPORT_API_INTERFACESIMP_SELECTIONINTERFACEIMP_H

//Local headers
#include <flowViewport/api.h>
#include <flowViewport/API/fvpFlowSelectionInterface.h>
#include <flowViewport/API/fvpSelectionClient.h>

namespace FVP_NS_DEF {

///Is a singleton, use SelectionInterfaceImp& selInterfaceImp = SelectionInterfaceImp::Get() to get an instance of that interface
class SelectionInterfaceImp :  public FVP_NS_DEF::FlowSelectionInterface
{
public:
    SelectionInterfaceImp()             = default;
    virtual ~SelectionInterfaceImp()    = default;

    ///Interface accessor
    static SelectionInterfaceImp& Get();

    //From FVP_NS_DEF::SelectionInterface
    void RegisterSelectionClient(SelectionClient& client)override;
    void UnregisterSelectionClient(SelectionClient& client)override;
    
    //To be called by maya-hydra
    void DummySelectionCallback();
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_INTERFACESIMP_SELECTIONINTERFACEIMP_H