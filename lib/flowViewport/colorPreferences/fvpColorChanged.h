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
#ifndef FVP_COLOR_CHANGED_H
#define FVP_COLOR_CHANGED_H

#include "flowViewport/api.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>

#include <ufe/notification.h>

namespace FVP_NS_DEF {

/// \class ColorChanged
///
/// \brief \p Notification class representing a color change and containing
/// the information associated with it. Objects of this class are immutable.
///
class FVP_API ColorChanged : public Notification
{
public:
    ColorChanged(
        const PXR_NS::TfToken& token,
        const PXR_NS::GfVec4f& oldColor,
        const PXR_NS::GfVec4f& newColor);
    ~ColorChanged() override = default;

    const PXR_NS::TfToken& token() const;
    const PXR_NS::GfVec4f& oldColor() const;
    const PXR_NS::GfVec4f& newColor() const;

private:
    PXR_NS::TfToken _token;
    PXR_NS::GfVec4f _oldColor;
    PXR_NS::GfVec4f _newColor;
};

} // namespace FVP_NS_DEF

#endif // FVP_COLOR_CHANGED_H
