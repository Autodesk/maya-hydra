//
// Copyright 2019 Luma Pictures
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

// GLEW must be included early (for core USD < 21.02), but we need pxr.h first
// so that PXR_VERSION has the correct value.
// We also disable clang-format for this block, since otherwise v10.0.0 fails
// to recognize that "utils.h" is the related header.
// clang-format off
#include <pxr/pxr.h>
// clang-format on

#include "pluginUtils.h"

#include "renderGlobals.h"
#include "tokens.h"

#include <pxr/imaging/glf/contextCaps.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

#include <maya/MGlobal.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

std::pair<
    const MayaHydra::MtohRendererDescriptionVector&,
    const MayaHydra::MtohRendererSettings&>
MtohInitializeRenderPlugins()
{
    using Storage = std::
        pair<MayaHydra::MtohRendererDescriptionVector, MayaHydra::MtohRendererSettings>;

    static const Storage ret = []() -> Storage {
        HdRendererPluginRegistry& pluginRegistry = HdRendererPluginRegistry::GetInstance();
        HfPluginDescVector        pluginDescs;
        pluginRegistry.GetPluginDescs(&pluginDescs);

        Storage store;
        store.first.reserve(pluginDescs.size());

        MtohRenderGlobals::OptionsPreamble();

        for (const auto& pluginDesc : pluginDescs) {
            const TfToken     renderer = pluginDesc.id;
            HdRendererPlugin* plugin = pluginRegistry.GetRendererPlugin(renderer);
            if (!plugin) {
                continue;
            }

            // XXX: As of 22.02, this needs to be called for Storm
            if (pluginDesc.id == MtohTokens->HdStormRendererPlugin) {
                GlfContextCaps::InitInstance();
            }

            HdRenderDelegate* delegate
                = plugin->IsSupported() ? plugin->CreateRenderDelegate() : nullptr;

            // No 'delete plugin', should plugin be cached as well?
            if (!delegate) {
                continue;
            }

            auto& rendererSettingDescriptors
                = store.second.emplace(renderer, delegate->GetRenderSettingDescriptors())
                      .first->second;

            // We only needed the delegate for the settings, so release
            plugin->DeleteRenderDelegate(delegate);
            // Null it out to make any possible usage later obv, wrong!
            delegate = nullptr;

            std::shared_ptr<pxr::UsdImagingGLEngine> _engine;
            store.first.emplace_back(
                renderer,
                TfToken(TfStringPrintf(
                    "%s%s", MayaHydra::MTOH_RENDER_OVERRIDE_PREFIX, renderer.GetText())),
                TfToken(TfStringPrintf(
                    "(Technology Preview) Hydra %s",
                    _engine->GetRendererDisplayName(pluginDesc.id).c_str())));
            MtohRenderGlobals::BuildOptionsMenu(store.first.back(), rendererSettingDescriptors);
        }

        // Make sure the static's size doesn't have any extra overhead
        store.first.shrink_to_fit();
        assert(store.first.size() == store.second.size());
        return store;
    }();
    return ret;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

std::string MtohGetRendererPluginDisplayName(const TfToken& id)
{
    HfPluginDesc pluginDesc;
    if (!TF_VERIFY(HdRendererPluginRegistry::GetInstance().GetPluginDesc(id, &pluginDesc))) {
        return {};
    }

    return pluginDesc.displayName;
}

const MtohRendererDescriptionVector& MtohGetRendererDescriptions()
{
    return MtohInitializeRenderPlugins().first;
}

const MtohRendererSettings& MtohGetRendererSettings()
{
    return MtohInitializeRenderPlugins().second;
}

} // namespace MAYAHYDRA_NS_DEF
