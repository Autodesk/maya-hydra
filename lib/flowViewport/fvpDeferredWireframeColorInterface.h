// Copyright 2026 Autodesk
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

#ifndef FLOW_VIEWPORT_DEFERRED_WIREFRAME_COLOR_INTERFACE_H
#define FLOW_VIEWPORT_DEFERRED_WIREFRAME_COLOR_INTERFACE_H

// Local headers
#include "flowViewport/selection/fvpSelectionTypes.h"
#include "fvpWireframeColorInterface.h"

// Hydra headers
#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/sdf/path.h>

#include <memory>

namespace FVP_NS_DEF {

class DeferredWireframeColorInterface : public WireframeColorInterface
{
    std::shared_ptr<WireframeColorInterface> _impl;

public:
    void    SetImplementation(std::shared_ptr<WireframeColorInterface> impl) { _impl = impl; }

    PXR_NS::GfVec4f getWireframeColor(const PXR_NS::SdfPath& p) const override
    {
        return _impl ? _impl->getWireframeColor(p) : PXR_NS::GfVec4f(1, 1, 1, 1);
    }
    PXR_NS::GfVec4f getWireframeColor(const PrimSelection& p) const override
    {
        return _impl ? _impl->getWireframeColor(p) : PXR_NS::GfVec4f(1, 1, 1, 1);
    }
};

} // end of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_DEFERRED_WIREFRAME_COLOR_INTERFACE_H