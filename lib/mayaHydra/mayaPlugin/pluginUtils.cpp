//
// Copyright 2019 Luma Pictures
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
#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

    // Declared as a char array (not const char*) so its length is a
    // compile-time constant via sizeof(), removing the need for std::strlen()
    // and avoiding any risk of an over-read on a non-\0-terminated buffer
    // (CWE-126 / CCPP-STRG-30).
    constexpr char TECHNOLOGY_PREVIEW_PREFIX[] = "(Technology Preview) ";
    constexpr std::size_t TECHNOLOGY_PREVIEW_PREFIX_LEN
        = sizeof(TECHNOLOGY_PREVIEW_PREFIX) - 1; // -1 to drop the trailing '\0'
    constexpr auto CMD_RENDER_SUFFIX   = "CmdRender";
    constexpr auto BATCH_RENDER_SUFFIX = "BatchRender";
    constexpr auto RENDER_SUFFIX       = "Render";
    constexpr auto OPTIONS_SUFFIX      = "BatchRenderOptionsString";

    std::filesystem::path _mayaHydraPluginLocation;

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

            store.first.emplace_back(
                renderer,
                TfToken(TfStringPrintf(
                    "%s%s", MayaHydra::MTOH_RENDER_OVERRIDE_PREFIX, renderer.GetText())),
                TfToken(TfStringPrintf(
                    "%sHydra %s",
                    TECHNOLOGY_PREVIEW_PREFIX,
                    PXR_NS::UsdImagingGLEngine::GetRendererDisplayName(pluginDesc.id).c_str())));
            MtohRenderGlobals::BuildOptionsMenu(store.first.back(), rendererSettingDescriptors);
        }

        // Make sure the static's size doesn't have any extra overhead
        store.first.shrink_to_fit();
        assert(store.first.size() == store.second.size());
        return store;
    }();
    return ret;
}

std::string_view prettyDisplayName(const std::string& srcDisplayName)
{
    auto dn = std::string_view(srcDisplayName);
    // Awkwardly rework what MtohInitializeRenderPlugins() has created.
    // Use the compile-time length of the prefix instead of std::strlen()
    // so we can never over-read past a non-\0-terminated buffer.
    dn.remove_prefix(TECHNOLOGY_PREVIEW_PREFIX_LEN);
    return dn;
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

void MtohSetMayaHydraPluginLocation(const std::filesystem::path& mayaHydraLocation)
{
    _mayaHydraPluginLocation = mayaHydraLocation;
}

std::filesystem::path MtohGetMayaHydraPluginLocation() 
{ 
    return _mayaHydraPluginLocation;
}

bool registerRenderer(const MtohRendererDescription& desc)
{
    // Renderer registration in Maya is done through the renderer MEL
    // command, with MEL callbacks that implement the various
    // rendering scenarios.  Each Hydra renderer will have its own MEL
    // callbacks, to account for potential differences in the
    // hydraRender MEL command arguments (e.g. Storm requires an
    // OpenGL context).

    // We therefore first call the renderer command with the proper
    // renderer-specific callback names.  We do not add tabs to the
    // render settings UI ("renderer -addGlobalsTab"), nor do we add a
    // Maya renderer settings global node ("renderer
    // -addGlobalsNode"), as these two capabilities are unused by Maya
    // Hydra renderers and will be replaced.

    std::ostringstream cmdStr;
    constexpr auto dq = '"';
    const auto& rn = desc.rendererName;
    cmdStr << "renderer"
           << " -rendererUIName " 
           << dq << prettyDisplayName(desc.displayName.GetString()) << dq
           // Called during command line renders with the Render executable.
           << " -commandRenderProcedure " << rn << CMD_RENDER_SUFFIX
           // Called during batch rendering launched interactively from Maya.
           << " -batchRenderProcedure " << rn << BATCH_RENDER_SUFFIX
           // Called for render current frame launched interactively from Maya.
           << " -renderProcedure " << rn << RENDER_SUFFIX
           // Returns the Hydra-specific command line arguments to the Render
           // executable on batch render from interactive mode.
           << " -batchRenderOptionsStringProcedure " << rn << OPTIONS_SUFFIX
           << " -cancelBatchRenderProcedure batchRender"
           << " -showBatchRenderProcedure " << dq << "batchRender -showImage true" << dq
           << " -renderSequenceProcedure mayaRenderSequence"
           << " -supportColorManagement true"
           // To avoid breaking the render settings UI we add the Common tab,
           // even though we don't use it.
           << " -addGlobalsTab Common createMayaSoftwareCommonGlobalsTab updateMayaSoftwareCommonGlobalsTab"
           << " " << desc.rendererName;

    if (MGlobal::executeCommand(cmdStr.str().c_str()) != MS::kSuccess) {
        return false;
    }

    // Next, define the callback procedures themselves.

    //
    // HYDRA-2025: we need a mechanism so that a renderer can describe its
    // requirements to MayaHydra.  Could be a MayaHydra-specific JSON file.
    // For now (27-Jan-2026) hard-code Storm requirement for an OpenGL context.
    //
    const bool needsGpu = (rn.GetString().find("Storm") != std::string::npos);

    std::ostringstream cbStr;
    cbStr << "global proc " << rn << CMD_RENDER_SUFFIX << "(string $option)\n"
          << "{\n" 
          << "hydraRender -renderer " << rn 
          << (needsGpu ? " -gpu 1" : "") << ";\n"
          << "}\n\n"
          << "global proc " << rn << BATCH_RENDER_SUFFIX << "(string $option)\n"
          << "{\n"
          << "hydraRender -renderer " << rn
          << (needsGpu ? " -gpu 1" : "") << ";\n"
          << "}\n\n"
          << "global proc " << rn << RENDER_SUFFIX 
          << "(int $width, int $height, int $doShadows, int $doGlowPass, string $camera, string $option)\n"
          << "{\n"
          // Interactive Maya provides OpenGL context if renderer requires it.
          << "hydraRender -renderer " << rn << ";\n"
          << "}\n\n"
          << "global proc string " << rn << OPTIONS_SUFFIX << "()\n"
          << "{\n"
          << "return " << dq << " -r " << rn << " " << dq << ";\n"
          << "}\n";

    if (MGlobal::executeCommand(cbStr.str().c_str()) != MS::kSuccess) {
        return false;
    }

    return true;
}

} // namespace MAYAHYDRA_NS_DEF
