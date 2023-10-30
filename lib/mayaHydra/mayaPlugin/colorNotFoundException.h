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

#ifndef COLOR_NOT_FOUND_EXCEPTION_H
#define COLOR_NOT_FOUND_EXCEPTION_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>

#include <stdexcept>

namespace MAYAHYDRA_NS_DEF {

class ColorNotFoundException : public std::invalid_argument
{
public:
    ColorNotFoundException(const std::string& message, const std::string& colorName);

    const std::string& colorName() const;

private:
    std::string _colorName;
};

class RGBAColorNotFoundException : public ColorNotFoundException
{
public:
    RGBAColorNotFoundException(const std::string& colorName);
};

class IndexedColorNotFoundException : public ColorNotFoundException
{
public:
    IndexedColorNotFoundException(const std::string& colorName, const std::string& tableName);

    const std::string& tableName() const;

private:
    std::string _tableName;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // COLOR_NOT_FOUND_EXCEPTION_H
