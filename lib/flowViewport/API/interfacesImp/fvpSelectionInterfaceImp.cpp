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

//Local headers
#include "fvpSelectionInterfaceImp.h"

//STL 
#include <mutex>

namespace{
    static std::mutex _viewportSelectClient_mutex;
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

static SelectionInterfaceImp theInterface;

SelectionClientSet _viewportSelectionClients;

FlowSelectionInterface& FlowSelectionInterface::Get() 
{ 
    return theInterface;
}

SelectionInterfaceImp& SelectionInterfaceImp::Get()
{
    return theInterface;
}

void SelectionInterfaceImp::RegisterSelectionClient(SelectionClient& client)
{
    std::lock_guard<std::mutex> lock(_viewportSelectClient_mutex);

    auto foundResult = _viewportSelectionClients.find(&client);
    if (foundResult == _viewportSelectionClients.cend()){
        _viewportSelectionClients.insert(&client);
    }
}

void SelectionInterfaceImp::UnregisterSelectionClient(SelectionClient& client)
{
    std::lock_guard<std::mutex> lock(_viewportSelectClient_mutex);

    auto foundResult = _viewportSelectionClients.find(&client);
    if (foundResult != _viewportSelectionClients.end()){
        _viewportSelectionClients.erase(foundResult);
    }
}

void SelectionInterfaceImp::DummySelectionCallback()
{
    std::lock_guard<std::mutex> lock(_viewportSelectClient_mutex);

    for (auto _selectionClient : _viewportSelectionClients) {
        if (_selectionClient){
            _selectionClient->DummySelectionCallback();
        }
    }
}

} //End of namespace FVP_NS_DEF

