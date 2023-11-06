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

#include "fvpColorChanged.h"

namespace FVP_NS_DEF {

ColorChanged::ColorChanged(
    const PXR_NS::TfToken& token,
    const PXR_NS::GfVec4f& oldColor,
    const PXR_NS::GfVec4f& newColor)
    : _token(token)
    , _oldColor(oldColor)
    , _newColor(newColor)
{
}

const PXR_NS::TfToken& ColorChanged::token() const { return _token; }

const PXR_NS::GfVec4f& ColorChanged::oldColor() const { return _oldColor; }

const PXR_NS::GfVec4f& ColorChanged::newColor() const { return _newColor; }

} // namespace FVP_NS_DEF
