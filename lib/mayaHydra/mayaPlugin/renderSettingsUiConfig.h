//
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
#ifndef MAYAHYDRA_RENDER_SETTINGS_UI_CONFIG_H
#define MAYAHYDRA_RENDER_SETTINGS_UI_CONFIG_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

#include <string>
#include <vector>

namespace MAYAHYDRA_NS_DEF {

/// A setting placed in a render settings UI category.
struct RenderSettingsUiSetting
{
    PXR_NS::TfToken key;   ///< Render setting id, or a MayaHydra per-renderer attribute name.
    std::string     label; ///< Empty: use the render delegate's display name.
};

/// A collapsible group of settings in the option box and the Attribute Editor.
struct RenderSettingsUiCategory
{
    std::string                          label;
    bool                                 collapsed = false;
    std::vector<RenderSettingsUiSetting> settings;
};

/// How a renderer's settings are grouped, ordered and labeled. Read from JSON, see
/// doc/renderSettingsUi.md.
struct RenderSettingsUiLayout
{
    std::vector<RenderSettingsUiCategory> categories;
    /// Receives the settings that no category lists. Its settings list is unused.
    RenderSettingsUiCategory other { "Other", false, {} };
    /// Keys that are never shown.
    PXR_NS::TfToken::Set hidden;
};

/// Returns the layout for the render delegate plugin \p renderer, or nullptr if no layout file
/// declares it (the settings are then shown as a flat list).
///
/// Layout files are the *.json files of each folder in MAYAHYDRA_RENDER_SETTINGS_UI_PATH, in
/// order, then of the default folder <plugin load path>/../mayaHydra/renderSettingsUi. The first
/// file that declares a renderer wins. The files are read on the first call; unloading the
/// plugin drops them, so reloading it reads them again.
const RenderSettingsUiLayout* MtohGetRenderSettingsUiLayout(const PXR_NS::TfToken& renderer);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_SETTINGS_UI_CONFIG_H
