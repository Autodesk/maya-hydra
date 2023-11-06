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
#include "fvpVersionInterfaceImp.h"

namespace FVP_NS_DEF {

//Interface version
#define FLOW_VIEWPORT_API_MAJOR_VERSION  0
#define FLOW_VIEWPORT_API_MINOR_VERSION  1
#define FLOW_VIEWPORT_API_PATCH_LEVEL    0

static VersionInterfaceImp theInterface;

VersionInterface& VersionInterface::Get() 
{ 
    return theInterface;
}

VersionInterfaceImp& VersionInterfaceImp::Get()
{
    return theInterface;
}

/////////////////////////////////       SelectionInterfaceImp          ////////////////////
void VersionInterfaceImp::GetVersion(int& majorVersion, int& minorVersion, int&patchLevel)
{
    majorVersion    = FLOW_VIEWPORT_API_MAJOR_VERSION;
    minorVersion    = FLOW_VIEWPORT_API_MINOR_VERSION;
    patchLevel      = FLOW_VIEWPORT_API_PATCH_LEVEL;
}

} //End of namespace FVP_NS_DEF

