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

#include "fvpColorPreferences.h"

#include "fvpColorChanged.h"
#include "fvpColorPreferencesTranslator.h"

#include <pxr/base/tf/diagnostic.h>

namespace {
std::shared_ptr<FVP_NS_DEF::ColorPreferences> instance;
} // namespace

namespace FVP_NS_DEF {

ColorPreferences& ColorPreferences::getInstance()
{
    if (!instance) {
        instance = std::shared_ptr<ColorPreferences>(new ColorPreferences());
    }
    return *instance;
}

void ColorPreferences::deleteInstance()
{
    if (instance) {
        instance.reset();
    }
}

void ColorPreferences::operator()(const Notification& notification)
{
    PXR_NAMESPACE_USING_DIRECTIVE
    if (TF_VERIFY(dynamic_cast<const ColorChanged*>(&notification) != nullptr)) {
        notify(notification);
    }
}

bool ColorPreferences::getColor(const PXR_NS::TfToken& preference, PXR_NS::GfVec4f& outColor) const
{
    PXR_NAMESPACE_USING_DIRECTIVE

    if (!TF_VERIFY(_translator != nullptr)) {
        return false;
    }

    return _translator->getColor(preference, outColor);
}

void ColorPreferences::setTranslator(
    const std::shared_ptr<ColorPreferencesTranslator>& newTranslator)
{
    if (static_cast<bool>(newTranslator) != static_cast<bool>(_translator)) {
        // Happy path : we're either doing null ptr -> valid ptr, or valid ptr -> null ptr
        _translator = newTranslator;
        return;
    }
    PXR_NAMESPACE_USING_DIRECTIVE
    if (newTranslator != nullptr && _translator != nullptr) {
        TF_CODING_ERROR("ColorPreferences::setTranslator was called with a non-null translator "
                        "while already having an active one. The second call will be ignored.");
    } else if (newTranslator == nullptr && _translator == nullptr) {
        TF_CODING_WARNING("ColorPreferences::setTranslator was called with a null translator while "
                          "already having none.");
    }
}

} // namespace FVP_NS_DEF
