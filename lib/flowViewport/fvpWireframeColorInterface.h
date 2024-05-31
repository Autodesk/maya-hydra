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
#ifndef FLOW_VIEWPORT_WIREFRAME_COLOR_INTERFACE_H
#define FLOW_VIEWPORT_WIREFRAME_COLOR_INTERFACE_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/usd/sdf/path.h>
#include <pxr/base/gf/vec4f.h>

namespace FVP_NS_DEF {


/// \class WireframeColorInterface
/// An interface to get the wireframe color from a prim
class WireframeColorInterface
{
public:
    //Get the wireframe color of a primitive for selection highlighting
    virtual PXR_NS::GfVec4f getWireframeColor(const PXR_NS::SdfPath& primPath) const = 0;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_WIREFRAME_COLOR_INTERFACE_H
