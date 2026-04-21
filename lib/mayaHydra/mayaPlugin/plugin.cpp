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
#include "getFramePassesCountCommand.h"
#include "testingCommand.h"
#include "renderRegionCommand.h"
#include "setVisibleFramePassesCommand.h"
#include "batchRendering/hydraRenderCmd.h"
#include "envSettings.h"

#include <mayaHydraLib/adapters/adapter.h>

#include <flowViewport/global.h>
#include <filesystem>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/envSetting.h>

#include <mayaUsdAPI/utils.h>

#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MSceneMessage.h>
#include <maya/MCommandResult.h>
//======================================================================
// Example code to create a USD render settings stage, for translation
// to Hydra v2 render settings.
//======================================================================
#include <maya/MDagModifier.h>

#include <ufe/sceneSegmentHandler.h>
#include <ufe/runTimeMgr.h>
//======================================================================
// End example code.
//======================================================================

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

class SceneModifiedGuard
{
public:

    // Read modified state before
    SceneModifiedGuard() : _wasModified(sceneModified())
    {}

    ~SceneModifiedGuard()
    {
        // If modified flag became true, clear it.
        if (sceneModified() && !_wasModified) {
            MGlobal::executeCommand("file -modified 0");
        }
    }

private:

    bool _wasModified{false};

    // Scene modified query.
    static bool sceneModified()
    {
        MCommandResult result;
        auto status = MGlobal::executeCommand("file -query -modified", result);
        if (status != MStatus::kSuccess) {
            throw std::runtime_error("File modified query error in mayaHydra plugin load.");
        }
        int typedResult{0};
        status = result.getResult(typedResult);
        if (status != MStatus::kSuccess) {
            throw std::runtime_error("Command result access error in mayaHydra plugin load.");
        }
        return (typedResult != 0);
    }
};

//======================================================================
// Example code to create a USD render settings stage, for translation
// to Hydra v2 render settings.
//======================================================================

static MCallbackId g_renderSettingsFileNewCallbackId = 0;
static MCallbackId g_renderSettingsFileOpenCallbackId = 0;

bool createRenderSettings()
{
    MStatus status;
    MDagModifier dagMod;

    // Create a transform node as parent
    MObject transformObj = dagMod.createNode("transform", MObject::kNullObj, &status);
    if (status != MS::kSuccess || transformObj.isNull()) {
        return false;
    }

    // Create the mayaUsdProxyShape node
    MObject proxyShapeObj = dagMod.createNode("mayaUsdProxyShape", transformObj, &status);
    if (status != MS::kSuccess || proxyShapeObj.isNull()) {
        return false;
    }

    // Rename the proxy shape to "renderSettings"
    status = dagMod.renameNode(transformObj, "renderSettings");
    if (status != MS::kSuccess) {
        return false;
    }

    status = dagMod.renameNode(proxyShapeObj, "renderSettingsShape");
    if (status != MS::kSuccess) {
        return false;
    }
    // Execute the DAG modifier operations
    status = dagMod.doIt();
    if (status != MS::kSuccess) {
        return false;
    }

    return true;
}

// Callback function to create USD render settings on file new.
void onFileNewCreateRenderSettings(void* /*clientData*/)
{
    // For TF_WARN macro.
    PXR_NAMESPACE_USING_DIRECTIVE

    if (!createRenderSettings()) {
        TF_WARN("USD render settings creation failed.");
    }
}

// Callback function to check if a stage exists on file open, otherwise create
// one with usd render settings.
void onFileOpenCheckOrCreateRenderSettings(void* /*clientData*/)
{
    // If there is at least one stage in the Maya scene, render settings will
    // be taken from one existing stage.
    const auto mayaSceneSegmentHandler = Ufe::RunTimeMgr::instance().sceneSegmentHandler(MayaUsdAPI::getMayaRunTimeId());
    const auto mayaRootPath = mayaSceneSegmentHandler->rootSceneSegmentRootPath();
    const auto gatewayItems = Ufe::SceneSegmentHandler::findGatewayItems(
        mayaRootPath, MayaUsdAPI::getUsdRunTimeId());

    if (gatewayItems.empty()) {
        onFileNewCreateRenderSettings(nullptr);
    }
}

}

void initialize()
{
    Fvp::InitializationParams fvpInitParams;
    if (MGlobal::mayaState() != MGlobal::kBatch) {
        // MayaColorPreferencesTranslator ctor will throw an exception
        // on construction in batch mode, as 
        // MayaHydra::getRGBAColorPreferenceValue() fails because
        // the Maya displayRGBColor command is unavailable in batch mode.
        fvpInitParams.colorPreferencesNotificationProvider
            = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
        fvpInitParams.colorPreferencesTranslator
            = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    }
    Fvp::initialize(fvpInitParams);
}

void finalize()
{
    Fvp::InitializationParams fvpInitParams;
    if (MGlobal::mayaState() != MGlobal::kBatch) {
        fvpInitParams.colorPreferencesNotificationProvider
            = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
        fvpInitParams.colorPreferencesTranslator
            = MayaHydra::MayaColorPreferencesTranslator::getInstance().shared_from_this();
    }
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
    SceneModifiedGuard guard;

    MString experimental("mayaHydra is experimental.");
    MGlobal::displayWarning(experimental);

    MStatus ret = MS::kSuccess;

    ret = PXR_NS::MayaHydraAdapter::Initialize();
    if (!ret) {
        return ret;
    }

    // For now this is required for the HdSt backend to use lights.
    setEnv("USDIMAGING_ENABLE_SCENE_LIGHTS", "1");

    // Set dome light textures maximum resolution default to 1024.  A proper
    // solution with a Hydra preferences category in the Maya
    // preferences UI is preferable, but at time of writing is not in
    // scope.
    MGlobal::executeCommand("if (!`optionVar -exists HdStormRendererPlugin__domeLightTexturesMaxResolution`) { optionVar -iv HdStormRendererPlugin__domeLightTexturesMaxResolution 1024; }");

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

    if (!plugin.registerCommand(
            MayaHydraTestingCommand::commandName,
            MayaHydraTestingCommand::creator,
            MayaHydraTestingCommand::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydraTesting command!");
        return ret;
    }

    if (!plugin.registerCommand(
            MayaHydraSetVisibleFramePasses::commandName,
            MayaHydraSetVisibleFramePasses::creator,
            MayaHydraSetVisibleFramePasses::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering MayaHydraSetVisibleFramePasses !");
        return ret;
    }

    if (!plugin.registerCommand(
            MayaHydraRenderRegionCommand::commandName,
            MayaHydraRenderRegionCommand::creator,
            MayaHydraRenderRegionCommand::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering mayaHydraRenderRegion command!");
        return ret;
    }

	if (!plugin.registerCommand(
            MayaHydraGetFramePassesCount::commandName,
            MayaHydraGetFramePassesCount::creator,
            MayaHydraGetFramePassesCount::createSyntax)) {
        ret = MS::kFailure;
        ret.perror("Error registering MayaHydraGetFramePassesCount !");
        return ret;
    }

    // *** FIXME ***  Have a single templated function for all 3 commands. 
    if (!plugin.registerCommand(
            HydraRenderCmd::name, HydraRenderCmd::creator, HydraRenderCmd::createSyntax)) {
        ret = MS::kFailure;
        std::ostringstream msg;
        msg << "Error registering " << HydraRenderCmd::name << " command!";
        ret.perror(msg.str().c_str());
        return ret;
    }

    // Set the path where maya hydra is loaded to be used later
    //This must be called before the renderoverride is created
    MtohSetMayaHydraPluginLocation(std::filesystem::path(plugin.loadPath().asChar())); 
  
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

    // Renderer registration must be done after UI registration, as UI
    // registration defines the UI tab in the Maya render settings.  To be
    // re-evaluated as render settings UI requirements are clarified.
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

    // Register Hydra renderers as Maya production renderers.
    for (const auto& desc : MayaHydra::MtohGetRendererDescriptions()) {
        registerRenderer(desc);
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

    if (addRenderSettingsToScene()) {
        // Register file new callback to create renderSettings proxy shape
        g_renderSettingsFileNewCallbackId = MSceneMessage::addCallback(
            MSceneMessage::kAfterNew, onFileNewCreateRenderSettings, nullptr, &ret);
        if (!ret) {
            ret.perror("Unable to register render settings file new callback.");
            return ret;
        }

        // Register file open callback
        g_renderSettingsFileOpenCallbackId = MSceneMessage::addCallback(
            MSceneMessage::kAfterOpen, onFileOpenCheckOrCreateRenderSettings, nullptr, &ret);
        if (!ret) {
            ret.perror("Unable to register render settings file open callback.");
            return ret;
        }
    }

    constexpr const char* melRsUtils = "mayaHydra_renderSettings_utils";
    if (MGlobal::sourceFile(MString(melRsUtils)) != MS::kSuccess) {
        std::ostringstream msg;
        msg << "Error sourcing script " << melRsUtils;
        ret = MS::kFailure;
        ret.perror(msg.str().c_str());
        return ret;
    }

    return ret;
}

PLUGIN_EXPORT MStatus uninitializePlugin(MObject obj)
{
    if (g_renderSettingsFileOpenCallbackId != 0) {
        MMessage::removeCallback(g_renderSettingsFileOpenCallbackId);
        g_renderSettingsFileOpenCallbackId = 0;
    }

    if (g_renderSettingsFileNewCallbackId != 0) {
        MMessage::removeCallback(g_renderSettingsFileNewCallbackId);
        g_renderSettingsFileNewCallbackId = 0;
    }

    finalize();

    for (const auto& callbackId : _pluginLoadingCallbackIds) {
        MSceneMessage::removeCallback(callbackId);
    }

    MFnPlugin plugin(obj, "Autodesk", PLUGIN_VERSION, "Any");
    MStatus   ret = MS::kSuccess;
    if (auto* renderer = MHWRender::MRenderer::theRenderer()) {
        for (unsigned int i = 0; i < _renderOverrides.size(); i++) {
            renderer->deregisterOverride(_renderOverrides[i]);
            // Using delete because we cannot use smart pointers in the static _renderOverrides
            // vector, see declaration of _renderOverrides for explanation
            delete _renderOverrides[i];
        }
    }

    _renderOverrides.clear();

    // Clear any registered callbacks
    MGlobal::executeCommand("callbacks -cc -owner mayaHydra;");

    if (!plugin.deregisterCommand(HydraRenderCmd::name)) {
        ret = MS::kFailure;
        std::ostringstream msg;
        msg << "Error deregistering " << HydraRenderCmd::name << " command!";
        ret.perror(msg.str().c_str());
    }

    if (!plugin.deregisterCommand(MtohViewCmd::name)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering mayaHydra command!");
    }

    if (!plugin.deregisterCommand(MayaHydraPluginInfoCommand::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering MayaHydraPluginInfo command!");
    }

    if (!plugin.deregisterCommand(MayaHydraTestingCommand::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering mayaHydraTesting command!");
    }

    if (!plugin.deregisterCommand(MayaHydraSetVisibleFramePasses::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering MayaHydraSetVisibleFramePasses command!");
    }

    if (!plugin.deregisterCommand(MayaHydraRenderRegionCommand::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering mayaHydraRenderRegion command!");
    }

    if (!plugin.deregisterCommand(MayaHydraGetFramePassesCount::commandName)) {
        ret = MS::kFailure;
        ret.perror("Error deregistering MayaHydraGetFramePassesCount command!");
    }

    return ret;
}
