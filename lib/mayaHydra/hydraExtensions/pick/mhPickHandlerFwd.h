//
// Copyright 2024 Autodesk
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
#ifndef MH_PICK_HANDLER_FWD_H
#define MH_PICK_HANDLER_FWD_H

#include <mayaHydraLib/api.h>

#include <memory>

namespace MAYAHYDRA_NS_DEF {

class PickHandler;

using PickHandlerPtr      = std::shared_ptr<PickHandler>;
using PickHandlerConstPtr = std::shared_ptr<const PickHandler>;

}

#endif