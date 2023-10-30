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

#include "colorNotFoundException.h"

namespace MAYAHYDRA_NS_DEF {

ColorNotFoundException::ColorNotFoundException(
    const std::string& message,
    const std::string& colorName)
    : std::invalid_argument(message)
    , _colorName(colorName)
{
}

const std::string& ColorNotFoundException::colorName() const { return _colorName; }

RGBAColorNotFoundException::RGBAColorNotFoundException(const std::string& colorName)
    : ColorNotFoundException(colorName + " is not a valid RGBA color.", colorName)
{
}

IndexedColorNotFoundException::IndexedColorNotFoundException(
    const std::string& colorName,
    const std::string& tableName)
    : ColorNotFoundException(
        colorName + " is not a valid color within the " + tableName + " table.",
        colorName)
    , _tableName(tableName)
{
}

const std::string& IndexedColorNotFoundException::tableName() const { return _tableName; }

} // namespace MAYAHYDRA_NS_DEF
