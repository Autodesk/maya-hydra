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
#ifndef FVP_COLOR_PREFERENCES_TRANSLATOR_H
#define FVP_COLOR_PREFERENCES_TRANSLATOR_H

#include "flowViewport/api.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/base/tf/token.h>

#include <ufe/subject.h>

namespace FVP_NS_DEF {

/// \class ColorPreferencesTranslator
///
/// A \p ColorPreferencesTranslator acts as a translation layer between
/// the host and the Flow Viewport interface. It is an interface which
/// the host implements. It only needs to implement the virtual method
/// \p getColor() to return a requested color preference.
///
class FVP_API ColorPreferencesTranslator
{
public:
    /// @brief Retrieve the color value for a given color preference.
    ///
    /// @param[in] preference The color preference we want to know the color of.
    /// @param[out] outColor Output parameter to store the color resulting from
    /// the query.
    ///
    /// @return True if the color was found and \p outColor was populated,
    /// false otherwise.
    virtual bool getColor(const PXR_NS::TfToken& preference, PXR_NS::GfVec4f& outColor) const = 0;

    virtual ~ColorPreferencesTranslator() = default;
};

} // namespace FVP_NS_DEF

#endif // FVP_COLOR_PREFERENCES_TRANSLATOR_H
