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
#ifndef FLOW_VIEWPORT_API_SAMPLES_SELECTIONCLIENTEXAMPLE_H
#define FLOW_VIEWPORT_API_SAMPLES_SELECTIONCLIENTEXAMPLE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpSelectionClient.h"
#include "flowViewport/API/fvpSelectionInterface.h"

namespace FVP_NS_DEF {

///Implementation of an SelectionClient which is the way to communicate with our Hydra viewport plugin to deal with selection
class FVP_API SelectionClientExample : public SelectionClient
{
public:
    SelectionClientExample() = default;
    ~SelectionClientExample() override;

    ///Set the hydra interface
    void SetHydraInterface(SelectionInterface* si)   {_hydraViewportSelectionInterface   = si;}

    ///From FlowViewport::SelectionClient
    ///This is a dummy callback function to be replaced
    void DummySelectionCallback()override;

protected:
    ///Store the Hydra interface pointer
    SelectionInterface*  _hydraViewportSelectionInterface    = nullptr;
};

} //end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_API_SAMPLES_SELECTIONCLIENTEXAMPLE_H
