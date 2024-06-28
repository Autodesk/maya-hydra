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
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
#include "mayaColorPreferencesTranslator.h"
#include "pluginUtils.h"
#include "renderGlobals.h"
#include "renderOverride.h"
#include "viewCommand.h"

#include <mayaHydraLib/adapters/adapter.h>

#include <flowViewport/global.h>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/envSetting.h>

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MSceneMessage.h>

#include <memory>
#include <vector>

#include <stdio.h>
#include <stdlib.h>

// Maya plugin init/uninit functions

#if MAYA_API_VERSION < 20240000
#error Maya API version 2024+ required
#endif

using namespace MayaHydra;

// Don't use smart pointers in the static vector: when Maya is doing its
// default "quick exit" that does not uninitialize plugins, the atexit
// destruction of the overrides in the vector will crash on destruction,
// because Hydra has already destroyed structures these rely on.  Simply leak
// the render overrides in this case.
static std::vector<PXR_NS::MtohRenderOverride*> gsRenderOverrides;

#if defined(MAYAHYDRA_VERSION)
#define STRINGIFY(x)   #x
#define TOSTRING(x)    STRINGIFY(x)
#define PLUGIN_VERSION TOSTRING(MAYAHYDRA_VERSION)
#else
#pragma message("MAYAHYDRA_VERSION is not defined")
#define PLUGIN_VERSION "Maya-Hydra experimental"
#endif

void initialize()
{
    Fvp::InitializationParams fvpInitParams;
    fvpInitParams.colorPreferencesNotificationProvider
        = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    fvpInitParams.colorPreferencesTranslator
        = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    Fvp::initialize(fvpInitParams);
}

void finalize()
{
    Fvp::InitializationParams fvpInitParams;
    fvpInitParams.colorPreferencesNotificationProvider
        = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    fvpInitParams.colorPreferencesTranslator
        = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    Fvp::finalize(fvpInitParams);
    MayaHydra::MayaColorPreferencesTranslator::deleteInstance();
}

static std::optional<MCallbackId> gsBeforePluginUnloadCallbackId;

void beforePluginUnloadCallback( const MStringArray& strs, void* clientData )
{
    for (const auto& str : strs) {
        if (str == "mayaUsdPlugin") {
            MGlobal::executeCommand("mayaHydra_GeomSubsetsPickMode_TeardownUI");
            break;
        }
    }
}

PLUGIN_EXPORT MStatus initializePlugin(MObject obj)
{
    MString experimental("mayaHydra is experimental.");
    MGlobal::displayWarning(experimental);

    MStatus ret = MS::kSuccess;

    ret = PXR_NS::MayaHydraAdapter::Initialize();
    if (!ret) {
        return ret;
    }

    // For now this is required for the HdSt backed to use lights.
    // putenv requires char* and I'm not willing to use const cast!
    constexpr const char* envVarSet = "USDIMAGING_ENABLE_SCENE_LIGHTS=1";
    const auto            envVarSize = strlen(envVarSet) + 1;
    std::vector<char>     envVarData;
    envVarData.resize(envVarSize);
    snprintf(envVarData.data(), envVarSize, "%s", envVarSet);
    putenv(envVarData.data());

    MFnPlugin plugin(obj, "Autodesk", PLUGIN_VERSION, "Any");

    if (!plugin.registerCommand(
            MtohViewCmd::name, MtohViewCmd::creator, MtohViewCmd::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydra command!");
        return ret;
    }

    if (auto* renderer = MHWRender::MRenderer::theRenderer()) {
        for (const auto& desc : MayaHydra::MtohGetRendererDescriptions()) {
            auto    mtohRenderer = std::make_unique<PXR_NS::MtohRenderOverride>(desc);
            MStatus status = renderer->registerOverride(mtohRenderer.get());
            if (status == MS::kSuccess) {
                gsRenderOverrides.push_back(mtohRenderer.release());
            }
        }
    }

    plugin.registerUI(
        "mayaHydra_registerUI_load",
        "mayaHydra_registerUI_unload",
        "mayaHydra_registerUI_batch_load",
        "mayaHydra_registerUI_batch_unload");

    MStatus beforePluginUnloadCallbackStatus;
    MCallbackId beforePluginUnloadCallbackId = MSceneMessage::addStringArrayCallback(
        MSceneMessage::Message::kBeforePluginUnload, 
        beforePluginUnloadCallback, 
        nullptr, 
        &beforePluginUnloadCallbackStatus);
    if (beforePluginUnloadCallbackStatus) {
        gsBeforePluginUnloadCallbackId = beforePluginUnloadCallbackId;
    } else {
        ret = MS::kFailure;
        ret.perror("Error registering BeforePluginUnload callback.");
        return ret;
    }

    initialize();

    return ret;
}

PLUGIN_EXPORT MStatus uninitializePlugin(MObject obj)
{
    finalize();

    if (gsBeforePluginUnloadCallbackId.has_value()) {
        MSceneMessage::removeCallback(gsBeforePluginUnloadCallbackId.value());
    }

    MFnPlugin plugin(obj, "Autodesk", PLUGIN_VERSION, "Any");
    MStatus   ret = MS::kSuccess;
    if (auto* renderer = MHWRender::MRenderer::theRenderer()) {
        for (unsigned int i = 0; i < gsRenderOverrides.size(); i++) {
            renderer->deregisterOverride(gsRenderOverrides[i]);
            delete gsRenderOverrides[i];
        }
    }

    gsRenderOverrides.clear();

    // Clear any registered callbacks
    MGlobal::executeCommand("callbacks -cc mayaHydra;");

    if (!plugin.deregisterCommand(MtohViewCmd::name)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering mayaHydra command!");
    }

    return ret;
}
