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

#include "optionalSceneIndexPlugins.h"

#include "pluginDebugCodes.h"

#include <pxr/base/js/value.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>

#include <algorithm>
#include <set>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

// Metadata keys of the scene index plugin type entry in plugInfo.json.
const char* const kOptionalForRendererKey = "optionalForRenderer";
const char* const kImplementsReprsKey     = "implementsReprs";
const char* const kInsertionPhaseKey      = "insertionPhase";

bool implementsOneOf(const JsValue& implementsReprs, const TfTokenVector& authoredReprs)
{
    if (!implementsReprs.IsArray()) {
        return false;
    }
    for (const JsValue& repr : implementsReprs.GetJsArray()) {
        if (repr.IsString()
            && std::find(authoredReprs.begin(), authoredReprs.end(), TfToken(repr.GetString()))
                != authoredReprs.end()) {
            return true;
        }
    }
    return false;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

void RegisterOptionalSceneIndexPlugins(const TfTokenVector& authoredReprs)
{
    // The registry never forgets a registration and registering twice stacks two instances,
    // so remember what was done in this process. This state is lost when the mayaHydra plugin
    // is unloaded, so a reload registers again: the optional plugins must tolerate that by
    // not adding their scene index when it is already in the chain.
    static std::set<std::string> registered;

    // Kill switch, also used to measure the cost of having the optional plugins registered.
    if (TfGetenvBool("MAYAHYDRA_DISABLE_OPTIONAL_SCENE_INDEX_PLUGINS", false)) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_OPTIONAL_SCENE_INDEX_PLUGINS,
            "Optional scene index plugins: disabled by MAYAHYDRA_DISABLE_OPTIONAL_SCENE_INDEX_PLUGINS.\n");
        return;
    }

    const TfType baseType = TfType::FindByName("HdSceneIndexPlugin");
    if (baseType.IsUnknown()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_OPTIONAL_SCENE_INDEX_PLUGINS,
            "Optional scene index plugins: type HdSceneIndexPlugin is unknown.\n");
        return;
    }

    std::set<TfType> pluginTypes;
    baseType.GetAllDerivedTypes(&pluginTypes);
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_OPTIONAL_SCENE_INDEX_PLUGINS,
        "Optional scene index plugins: %zu HdSceneIndexPlugin types known.\n", pluginTypes.size());

    PlugRegistry& plugRegistry = PlugRegistry::GetInstance();
    for (const TfType& type : pluginTypes) {
        const JsValue renderer = plugRegistry.GetDataFromPluginMetaData(type, kOptionalForRendererKey);
        if (!renderer.IsString()) {
            continue; // Not an optional plugin.
        }
        if (!implementsOneOf(
                plugRegistry.GetDataFromPluginMetaData(type, kImplementsReprsKey), authoredReprs)) {
            continue;
        }
        const JsValue phase = plugRegistry.GetDataFromPluginMetaData(type, kInsertionPhaseKey);
        if (!phase.IsInt()) {
            TF_WARN(
                "Optional scene index plugin '%s' has no integer '%s' in its plugInfo.json: not registered.",
                type.GetTypeName().c_str(), kInsertionPhaseKey);
            continue;
        }

        const std::string pluginId = type.GetTypeName();
        if (!registered.insert(renderer.GetString() + "/" + pluginId).second) {
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_OPTIONAL_SCENE_INDEX_PLUGINS,
                "Optional scene index plugin '%s' for '%s': already registered.\n",
                pluginId.c_str(), renderer.GetString().c_str());
            continue;
        }
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_OPTIONAL_SCENE_INDEX_PLUGINS,
            "Optional scene index plugin '%s': registering for renderer '%s' at phase %d.\n",
            pluginId.c_str(), renderer.GetString().c_str(), phase.GetInt());
        // An optional plugin is inert unless its registration says "enabled": USD's default
        // (hybrid) ordering puts every plugin that has a JSON entry in the chain.
        static const HdContainerDataSourceHandle enabledArgs = HdRetainedContainerDataSource::New(
            TfToken("enabled"), HdRetainedTypedSampledDataSource<bool>::New(true));
        HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
            renderer.GetString(),
            TfToken(pluginId),
            enabledArgs,
            phase.GetInt(),
            HdSceneIndexPluginRegistry::InsertionOrderAtEnd);
    }
}

} // namespace MAYAHYDRA_NS_DEF
