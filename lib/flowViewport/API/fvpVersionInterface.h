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

#ifndef FLOW_VIEWPORT_API_VERSION_INTERFACE_H
#define FLOW_VIEWPORT_API_VERSION_INTERFACE_H

#include "flowViewport/api.h"

namespace FVP_NS_DEF
{
    /** 
    * VersionInterface is used to get the version of the Flow Viewport API.
    * 
    * To get an instance of the VersionInterface class, please use :
    * Fvp::VersionInterface& versionInterface = Fvp::VersionInterface::Get();
    */
    class VersionInterface
    {
     public:
         static FVP_API VersionInterface& Get();

        /**
        *  @brief      Get the version of the flow viewport API.
        *
        *              Get the version of the flow viewport API so you can handle different versions in your plugin if it changes.
        * 
        *  @param[out] majorVersion is the major version number.
        *  @param[out] minorVersion is the minor version number.
        *  @param[out] patchLevel is the patch level number.
        */
        virtual void GetVersion(int& majorVersion, int& minorVersion, int& patchLevel) = 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_VERSION_INTERFACE_H
