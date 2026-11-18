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

#include "renderSettingsUiConfig.h"

#include "pluginDebugCodes.h"
#include "pluginUtils.h"

#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/js/json.h>
#include <pxr/base/js/value.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/stringUtils.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <unordered_map>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

namespace fs = std::filesystem;

using Layouts = std::unordered_map<TfToken, MayaHydra::RenderSettingsUiLayout, TfToken::HashFunctor>;

constexpr const char* kPathEnvVar = "MAYAHYDRA_RENDER_SETTINGS_UI_PATH";

bool isValidId(const std::string& id)
{
    return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

// Parse errors are reported through 'error' rather than TF_WARN so the caller can name the file.
// Without 'requireId', a missing id keeps the category's current one.
bool parseCategoryHeader(
    const JsValue&                      value,
    MayaHydra::RenderSettingsUiCategory& category,
    std::string&                        error,
    bool                                requireId)
{
    if (!value.IsObject()) {
        error = "a category must be an object";
        return false;
    }
    const JsObject& obj = value.GetJsObject();
    const auto      id = obj.find("id");
    if (id != obj.end() || requireId) {
        if (id == obj.end() || !id->second.IsString() || !isValidId(id->second.GetString())) {
            error = "a category needs an 'id' string of letters, digits and underscores";
            return false;
        }
        category.id = id->second.GetString();
    }
    const auto label = obj.find("label");
    if (label == obj.end() || !label->second.IsString() || label->second.GetString().empty()) {
        error = "category '" + category.id + "' needs a non-empty string 'label'";
        return false;
    }
    category.label = label->second.GetString();
    const auto collapsed = obj.find("collapsed");
    if (collapsed != obj.end()) {
        if (!collapsed->second.IsBool()) {
            error = "'collapsed' of category '" + category.label + "' must be true or false";
            return false;
        }
        category.collapsed = collapsed->second.GetBool();
    }
    return true;
}

bool parseCategory(
    const JsValue&                      value,
    MayaHydra::RenderSettingsUiCategory& category,
    std::string&                        error)
{
    if (!parseCategoryHeader(value, category, error, true)) {
        return false;
    }
    const JsObject& obj = value.GetJsObject();
    const auto      settings = obj.find("settings");
    if (settings == obj.end()) {
        return true;
    }
    if (!settings->second.IsArray()) {
        error = "'settings' of category '" + category.label + "' must be an array";
        return false;
    }
    for (const JsValue& setting : settings->second.GetJsArray()) {
        if (setting.IsString()) {
            category.settings.push_back({ TfToken(setting.GetString()), {} });
            continue;
        }
        if (setting.IsObject()) {
            const JsObject& s = setting.GetJsObject();
            const auto      key = s.find("key");
            const auto      label = s.find("label");
            if (key != s.end() && key->second.IsString() && !key->second.GetString().empty()
                && (label == s.end() || label->second.IsString())) {
                category.settings.push_back(
                    { TfToken(key->second.GetString()),
                      label == s.end() ? std::string() : label->second.GetString() });
                continue;
            }
        }
        error = "a setting of category '" + category.label
            + "' must be a key string or an object with a string 'key' and an optional string "
              "'label'";
        return false;
    }
    return true;
}

// Returns the renderer id and its layout, or nothing if the file is not a valid layout file.
std::optional<std::pair<TfToken, MayaHydra::RenderSettingsUiLayout>>
parseLayoutFile(const fs::path& file)
{
    const auto warn = [&file](const std::string& reason) {
        TF_WARN(
            "[mayaHydra] Ignoring render settings UI file '%s': %s.",
            file.string().c_str(),
            reason.c_str());
    };

    std::ifstream stream(file);
    if (!stream) {
        warn("cannot be read");
        return std::nullopt;
    }
    JsParseError   parseError;
    const JsValue root = JsParseStream(stream, &parseError);
    if (root.IsNull()) {
        // The parser's reason already ends with a period; warn() adds one.
        warn(TfStringPrintf(
            "JSON error at line %u, column %u: %s",
            parseError.line,
            parseError.column,
            TfStringTrimRight(parseError.reason, ".").c_str()));
        return std::nullopt;
    }
    if (!root.IsObject()) {
        warn("the top level must be an object");
        return std::nullopt;
    }
    const JsObject& obj = root.GetJsObject();

    const auto renderer = obj.find("renderer");
    if (renderer == obj.end() || !renderer->second.IsString()
        || renderer->second.GetString().empty()) {
        warn("needs a non-empty string 'renderer' (the render delegate plugin id)");
        return std::nullopt;
    }

    MayaHydra::RenderSettingsUiLayout layout;
    std::string                       error;
    std::set<std::string>             ids;

    const auto categories = obj.find("categories");
    if (categories != obj.end()) {
        if (!categories->second.IsArray()) {
            warn("'categories' must be an array");
            return std::nullopt;
        }
        for (const JsValue& value : categories->second.GetJsArray()) {
            MayaHydra::RenderSettingsUiCategory category;
            if (!parseCategory(value, category, error)) {
                warn(error);
                return std::nullopt;
            }
            if (!ids.insert(category.id).second) {
                warn("category id '" + category.id + "' is used twice");
                return std::nullopt;
            }
            layout.categories.push_back(std::move(category));
        }
    }

    const auto other = obj.find("other");
    if (other != obj.end() && !parseCategoryHeader(other->second, layout.other, error, false)) {
        warn("'other': " + error);
        return std::nullopt;
    }

    const auto hidden = obj.find("hidden");
    if (hidden != obj.end()) {
        if (!hidden->second.IsArrayOf<std::string>()) {
            warn("'hidden' must be an array of key strings");
            return std::nullopt;
        }
        for (const auto& key : hidden->second.GetArrayOf<std::string>()) {
            layout.hidden.insert(TfToken(key));
        }
    }

    return std::make_pair(TfToken(renderer->second.GetString()), std::move(layout));
}

std::vector<fs::path> searchFolders()
{
    std::vector<fs::path> folders;
    for (const auto& folder : TfStringTokenize(TfGetenv(kPathEnvVar), ARCH_PATH_LIST_SEP)) {
        folders.emplace_back(folder);
    }
    const fs::path pluginLocation = MayaHydra::MtohGetMayaHydraPluginLocation();
    if (!pluginLocation.empty()) {
        folders.push_back(pluginLocation / ".." / "mayaHydra" / "renderSettingsUi");
    }
    return folders;
}

Layouts loadLayouts()
{
    Layouts layouts;
    for (const auto& folder : searchFolders()) {
        std::error_code ec;
        if (!fs::is_directory(folder, ec)) {
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_RENDER_SETTINGS_UI,
                "Render settings UI: folder '%s' not found.\n",
                folder.string().c_str());
            continue;
        }
        std::vector<fs::path> files;
        for (const auto& entry : fs::directory_iterator(folder, ec)) {
            if (entry.is_regular_file(ec) && entry.path().extension() == ".json") {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());

        TfToken::Set declaredHere;
        for (const auto& file : files) {
            auto parsed = parseLayoutFile(file);
            if (!parsed) {
                continue;
            }
            const TfToken& renderer = parsed->first;
            if (!declaredHere.insert(renderer).second) {
                TF_WARN(
                    "[mayaHydra] Render settings UI file '%s' declares renderer '%s' again in the "
                    "same folder: ignored.",
                    file.string().c_str(),
                    renderer.GetText());
                continue;
            }
            // An earlier folder in the search path wins.
            if (layouts.count(renderer)) {
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_RENDER_SETTINGS_UI,
                    "Render settings UI: '%s' for '%s' is overridden by an earlier folder.\n",
                    file.string().c_str(),
                    renderer.GetText());
                continue;
            }
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_RENDER_SETTINGS_UI,
                "Render settings UI: using '%s' for '%s'.\n",
                file.string().c_str(),
                renderer.GetText());
            layouts.emplace(renderer, std::move(parsed->second));
        }
    }
    return layouts;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

const RenderSettingsUiLayout* MtohGetRenderSettingsUiLayout(const TfToken& renderer)
{
    static const Layouts layouts = loadLayouts();
    const auto           it = layouts.find(renderer);
    if (it == layouts.end()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_RENDER_SETTINGS_UI,
            "Render settings UI: no layout for '%s', showing a flat list.\n",
            renderer.GetText());
        return nullptr;
    }
    return &it->second;
}

} // namespace MAYAHYDRA_NS_DEF
