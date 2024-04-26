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
#ifndef FVP_INSTRUMENTS
#define FVP_INSTRUMENTS

#include <flowViewport/api.h>

#include <pxr/base/vt/value.h>

#include <string>

namespace FVP_NS_DEF {

/// \class Instruments
///
/// A registry to measure Flow Viewport processing.
///
class Instruments
{
public:

    FVP_API
    static Instruments& instance();

    FVP_API
    PXR_NS::VtValue get(const std::string& key) const;
    FVP_API
    void set(const std::string& key, const PXR_NS::VtValue& v);

private:

    Instruments() = default;
};

} // namespace FVP_NS_DEF

#endif // FVP_INSTRUMENTS
