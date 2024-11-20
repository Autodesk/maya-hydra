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
#include "pluginBuildInfoCommand.h"

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

#if defined(MAYAHYDRA_VERSION)
#define STRINGIFY(x)   #x
#define TOSTRING(x)    STRINGIFY(x)
#define PLUGIN_VERSION TOSTRING(MAYAHYDRA_VERSION)
#else
#pragma message("MAYAHYDRA_VERSION is not defined")
#define PLUGIN_VERSION "Maya-Hydra experimental"
#endif

namespace {
    const std::string kMayaHydraPluginName = "mayaHydra";
    const std::string kMayaUsdPluginName = "mayaUsdPlugin";

    // Don't use smart pointers in the static vector: when Maya is doing its
    // default "quick exit" that does not uninitialize plugins, the atexit
    // destruction of the overrides in the vector will crash on destruction,
    // because Hydra has already destroyed structures these rely on.  Simply leak
    // the render overrides in this case.
    std::vector<PXR_NS::MtohRenderOverride*> _renderOverrides;

    std::vector<MCallbackId> _pluginLoadingCallbackIds;

    void setEnv(const std::string& name, const std::string& value)
    {
    #if defined(_WIN32)
    _putenv_s(name.c_str(), value.c_str());
    #else
    setenv(name.c_str(), value.c_str(), 1);
    #endif
    }
}

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

void afterPluginLoadCallback( const MStringArray& strs, void* clientData )
{
    for (const auto& str : strs) {
        // If MayaUSD is being loaded, set up our GeomSubsets picking mode UI.
        // This will re-create the "Select" menu callback if it has been previously torn down.
        if (str.asChar() == kMayaUsdPluginName) {
            MGlobal::executeCommand("if (`exists mayaHydra_GeomSubsetsPickMode_SetupUI`) { mayaHydra_GeomSubsetsPickMode_SetupUI; }");
            break;
        }
    }
}

void beforePluginUnloadCallback( const MStringArray& strs, void* clientData )
{
    for (const auto& str : strs) {
        // If MayaUSD is being unloaded, tear down our GeomSubsets picking mode UI.
        // This resets the variables used to keep track of the UI elements' existence,
        // and allows us to recreate them if MayaUSD is reloaded.
        // We also do the same if mayaHydra is about to be unloaded : we can't rely on
        // the deletion procedure registered through registerUI, as it seems the global 
        // variables tracking our UI elements have been reset at that point for some reason.
        auto strChar = str.asChar();
        if (strChar == kMayaUsdPluginName || strChar == kMayaHydraPluginName) {
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

    // For now this is required for the HdSt backend to use lights.
    setEnv("USDIMAGING_ENABLE_SCENE_LIGHTS", "1");

    // Performance optimization: disable RENDER_SELECTED_EDGE_FROM_FACE feature that could trigger unnecessary running of gometry shader.
    setEnv("HDST_RENDER_SELECTED_EDGE_FROM_FACE", "0");

    MFnPlugin plugin(obj, "Autodesk", PLUGIN_VERSION, "Any");

    if (!plugin.registerCommand(
            MtohViewCmd::name, MtohViewCmd::creator, MtohViewCmd::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydra command!");
        return ret;
    }

    if (!plugin.registerCommand(
        MayaHydraPluginInfoCommand::commandName, MayaHydraPluginInfoCommand::creator, MayaHydraPluginInfoCommand::createSyntax)) {
    ret = MS::kFailure;
    ret.perror("Error registering MayaHydraPluginInfo command!");
    return ret;
    }

    if (auto* renderer = MHWRender::MRenderer::theRenderer()) {
        for (const auto& desc : MayaHydra::MtohGetRendererDescriptions()) {
            auto    mtohRenderer = std::make_unique<PXR_NS::MtohRenderOverride>(desc);
            MStatus status = renderer->registerOverride(mtohRenderer.get());
            if (status == MS::kSuccess) {
                _renderOverrides.push_back(mtohRenderer.release());
            }
        }
    }

    if (!plugin.registerUIStrings(nullptr, "mayaHydra_registerUIStrings")) {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydra UI string resources.");
        return ret;
    }

    if (!plugin.registerUI(
        "mayaHydra_registerUI_load",
        "mayaHydra_registerUI_unload",
        "mayaHydra_registerUI_batch_load",
        "mayaHydra_registerUI_batch_unload"))
    {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydra UI procedures.");
        return ret;
    }

    auto registerPluginLoadingCallback = [&](MSceneMessage::Message pluginLoadingMessage, MMessage::MStringArrayFunction callback) {
        MStatus callbackStatus;
        MCallbackId callbackId = MSceneMessage::addStringArrayCallback(
            pluginLoadingMessage, 
            callback, 
            nullptr, 
            &callbackStatus);
        if (callbackStatus) {
            _pluginLoadingCallbackIds.push_back(callbackId);
        } else {
            ret = MS::kFailure;
            ret.perror("Error registering mayaHydra plugin loading callback.");
        }
    };
    
    std::vector<std::pair<MSceneMessage::Message, MMessage::MStringArrayFunction>> pluginLoadingCallbacks = {
        {MSceneMessage::Message::kAfterPluginLoad, afterPluginLoadCallback},
        {MSceneMessage::Message::kBeforePluginUnload, beforePluginUnloadCallback}
    };
    for (const auto& pluginLoadingCallback : pluginLoadingCallbacks) {
        registerPluginLoadingCallback(pluginLoadingCallback.first, pluginLoadingCallback.second);
        if (!ret) {
            return ret;
        }
    }

    initialize();

    return ret;
}

PLUGIN_EXPORT MStatus uninitializePlugin(MObject obj)
{
    finalize();

    for (const auto& callbackId : _pluginLoadingCallbackIds) {
        MSceneMessage::removeCallback(callbackId);
    }

    MFnPlugin plugin(obj, "Autodesk", PLUGIN_VERSION, "Any");
    MStatus   ret = MS::kSuccess;
    if (auto* renderer = MHWRender::MRenderer::theRenderer()) {
        for (unsigned int i = 0; i < _renderOverrides.size(); i++) {
            renderer->deregisterOverride(_renderOverrides[i]);
            delete _renderOverrides[i];
        }
    }

    _renderOverrides.clear();

    // Clear any registered callbacks
    MGlobal::executeCommand("callbacks -cc -owner mayaHydra;");

    if (!plugin.deregisterCommand(MtohViewCmd::name)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering mayaHydra command!");
    }

    if (!plugin.deregisterCommand(MayaHydraPluginInfoCommand::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering MayaHydraPluginInfo command!");
    }

    return ret;
}
