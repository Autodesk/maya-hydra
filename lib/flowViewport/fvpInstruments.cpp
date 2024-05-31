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

#include "fvpInstruments.h"

#include <pxr/base/vt/dictionary.h>

namespace {
PXR_NS::VtDictionary instruments;
}

namespace FVP_NS_DEF {

/* static */
Instruments& Instruments::instance()
{
    static Instruments i;
    return i;
}

PXR_NS::VtValue Instruments::get(const std::string& key) const
{
    auto found = instruments.find(key);
    return (found != instruments.end()) ? found->second : PXR_NS::VtValue();
}

void Instruments::set(const std::string& key, const PXR_NS::VtValue& v)
{
    instruments[key] = v;
}

} // namespace FVP_NS_DEF
