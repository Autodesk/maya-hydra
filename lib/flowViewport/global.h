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
#ifndef FVP_GLOBAL_H
#define FVP_GLOBAL_H

#include <flowViewport/api.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTranslator.h>

#include <memory>

namespace FVP_NS_DEF {

struct FVP_API InitializationParams
{
    std::shared_ptr<Subject>                    colorPreferencesNotificationProvider;
    std::shared_ptr<ColorPreferencesTranslator> colorPreferencesTranslator;
};

void FVP_API initialize(const InitializationParams& params);
void FVP_API finalize(const InitializationParams& params);

} // namespace FVP_NS_DEF

#endif // FVP_GLOBAL_H
