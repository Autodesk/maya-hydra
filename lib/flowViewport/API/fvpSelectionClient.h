//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#ifndef FLOW_VIEWPORT_API_SELECTIONCLIENT_H
#define FLOW_VIEWPORT_API_SELECTIONCLIENT_H

#include "flowViewport/api.h"

#include <pxr/usd/sdf/path.h>
#include <vector>

namespace FVP_NS_DEF 
{
    /**
    * SelectionClient is the definition of selection callbacks for an Hydra viewport.
    * Subclass this class to register an instance of SelectionClient with Fvp::FlowSelectionInterface::RegisterSelectionClient
    */
    class FVP_API SelectionClient
    {
    public:
        
        /**
        *  @brief      Is a dummy selection callback as a placeholder
        *
        */
        virtual void DummySelectionCallback() = 0;

        /// Destructor
        virtual ~SelectionClient() = default;
    };

    ///Array of SelectionClient
    using SelectionClientSet = std::set<SelectionClient*>;

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_API_SELECTIONCLIENT_H
