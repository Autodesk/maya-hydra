//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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
#include "hydraRenderCmd.h"

#include "pluginUtils.h"
#include "renderSettingsUtils.h"
#include "pluginDebugCodes.h"
#include "batchRenderer.h"

#include <mayaHydraLib/mayaUtils.h>

#include <ufeExtensions/Global.h>

#include <mayaUsdAPI/proxyStage.h>

#include <maya/MArgDatabase.h>
#include <maya/MAnimControl.h>
#include <maya/MDagPath.h>
#include <maya/MSyntax.h>
#include <maya/MTime.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/glf/diagnostic.h> // For GlfRegisterDefaultDebugOutputMessageCallback()
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/garch/glDebugWindow.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/usd/sdf/layer.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

// hydraRender [-renderer string] [-camera string] [-currentFrame] [-frame float] [-height uint] [-layer name] [-width uint] [-gpu {0|1}]

namespace {

constexpr auto _width = "-w";
constexpr auto _widthLong = "-width";

constexpr auto _height = "-h";
constexpr auto _heightLong = "-height";

constexpr auto _cameraFlagShort = "-cam";
constexpr auto _cameraFlagLong = "-camera";

constexpr auto _renderer = "-r";
constexpr auto _rendererLong = "-renderer";

constexpr auto _currentFrame = "-cf";
constexpr auto _currentFrameLong = "-currentFrame";

// _frame conflicts with _frame in pytypedefs.h, figure this out.
constexpr auto _frameShort = "-f";
constexpr auto _frameLong = "-frame";

constexpr auto _layer = "-l";
constexpr auto _layerLong = "-layer";

constexpr auto _gpuEnabledFlag = "-gpu";
constexpr auto _gpuEnabledFlagLong = "-gpuEnabled";

using namespace MayaHydra;

constexpr auto _helpText = R"HELP(For details on args usage please see 
https://github.com/Autodesk/maya-hydra/blob/dev/doc/mayaHydraCommands.md
)HELP";

// Cheap existence check against the registered Hydra renderer plugins:
// GetPluginDesc() only consults plugin metadata, it does not instantiate the
// plugin/render delegate (unlike GetRendererPlugin()/CreateRenderDelegate(),
// which BatchRenderer::_InitHydraResources() still uses to catch
// instantiation failures, e.g. no GPU context).
//
// Note: this deliberately does NOT go through Maya's `renderer -query
// -namesOfAvailableRenderers`/`-capability "isHydra"` commands. Those only
// reflect viewport renderer-override registration, which only happens if
// MHWRender::MRenderer::theRenderer() is non-null (see plugin.cpp); in
// headless/batch invocations (e.g. mayapy with no GPU/display context, as
// on Linux CI) that is null, so a genuinely registered Hydra render
// delegate like HdArnoldRendererPlugin would be wrongly reported as
// unavailable.
bool validRenderer(const TfToken& rendererName)
{
    if (rendererName.IsEmpty()) {
        return false;
    }

    HfPluginDesc pluginDesc;
    return HdRendererPluginRegistry::GetInstance().GetPluginDesc(rendererName, &pluginDesc);
}

// The presence of session layer dynamic attributes on the
// UsdDefaultRenderDescription node indicates use of a -sessionLayer or
// -sessionLayerStage argument to the Render executable.
std::string getSessionLayerString(std::string_view s)
{
    MPlug plug;
    if (!GetPlug(rdNodeName(), s.data(), plug)) {
        return {};
    }

    return std::string(plug.asString().asChar());
}

std::string getSessionLayer()
{
    return getSessionLayerString("sessionLayer");
}

Ufe::Path getSessionLayerStage()
{
    return Ufe::PathString::path(getSessionLayerString("sessionLayerStage"));
}

bool applySessionLayer(
    const std::string& sessionLayer,
    const Ufe::Path&   sessionLayerStagePath
)
{
    auto layer = SdfLayer::FindOrOpen(sessionLayer);
    if (!layer) {
        TF_WARN("applySessionLayer: could not open session layer file '%s'.",
                sessionLayer.c_str());
        return false;
    }

    MObject nodeObj = (sessionLayerStagePath == UsdDefaultRenderDescriptionNodePath())
        // Stage from UsdDefaultRenderDescriptionNode.
        ? GetDependNodeFromNodeName(rdNodeName())
        : [&sessionLayerStagePath]() {
            // Stage from MayaUsdProxyShape node.
            MDagPath dagPath = UfeExtensions::ufeToDagPath(sessionLayerStagePath);
            if (!dagPath.isValid()) {
                return MObject();
            }

            // The path may name the proxy shape's parent transform.
            dagPath.extendToShape();

            MObject nodeObj = dagPath.node();
            return (kMayaUsdProxyShapeId == MFnDependencyNode(nodeObj).typeId()) ? nodeObj : MObject();
        }();

    if (nodeObj.isNull()) {
        TF_WARN("'%s' is not a valid path to a USD stage.",
                Ufe::PathString::string(sessionLayerStagePath).c_str());
        return false;
    }

    const auto stage = MayaUsdAPI::ProxyStage(nodeObj).getUsdStage();

    if (!stage) {
        TF_WARN("applySessionLayer: could not open the stage at '%s'.",
                Ufe::PathString::string(sessionLayerStagePath).c_str());
        return false;
    }

    // Copy the contents of the argument session layer into the stage's
    // session layer.
    auto stageSessionLayer = stage->GetSessionLayer();
    stageSessionLayer->TransferContent(layer);

    return true;
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

//======================================================================
// CLASS GLRenderWindow
//======================================================================

class GLRenderWindow : public GarchGLDebugWindow
{
public:
    typedef GLRenderWindow This;

public:
    GLRenderWindow();
    virtual ~GLRenderWindow() = default;

    // GarchGLDebugWindow overrides
    virtual void OnInitializeGL();
};

GLRenderWindow::GLRenderWindow()
    : GarchGLDebugWindow("Maya Hydra Render", 100, 100)
{}

/* virtual */
void
GLRenderWindow::OnInitializeGL()
{
    GarchGLApiLoad();
    GlfRegisterDefaultDebugOutputMessageCallback();
}
const MString HydraRenderCmd::name("hydraRender");

//======================================================================
// CLASS HydraRenderCmd 
//======================================================================

void HydraRenderCmd::displayError(std::string_view error)
{
    MPxCommand::displayError(
        name + ": " + MString(error.data(), static_cast<int>(error.size())),
        /* showLineNumber = */ true);
}

MSyntax HydraRenderCmd::createSyntax()
{
    MSyntax syntax;

    syntax.addFlag(_width, _widthLong, MSyntax::kUnsigned);
    syntax.addFlag(_height, _heightLong, MSyntax::kUnsigned);
    syntax.addFlag(_cameraFlagShort, _cameraFlagLong, MSyntax::kString);
    syntax.addFlag(_renderer, _rendererLong, MSyntax::kString);
    syntax.addFlag(_currentFrame, _currentFrameLong);
    syntax.addFlag(_gpuEnabledFlag, _gpuEnabledFlagLong, MSyntax::kBoolean);
    syntax.addFlag(_frameShort, _frameLong, MSyntax::kDouble);
    syntax.addFlag(_layer, _layerLong, MSyntax::kString);

    return syntax;
}

HydraRenderCmd::HydraRenderCmd() 
{}

HydraRenderCmd::~HydraRenderCmd()
{
    if (_batchRenderer && BatchRenderer::TestModeEnabled()) {
        BatchRenderer::RetainForTest(std::move(_batchRenderer));
    }
}

bool HydraRenderCmd::parseDatabase(const MArgDatabase& db)
{
    return true;
}

bool HydraRenderCmd::initialize()
{
    if (!_batchRenderer) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD, "_batchRenderer is a nullptr.\n");
        return false;
    }

    return _batchRenderer->Initialize();
}

bool HydraRenderCmd::render()
{
    // Must execute the render operations currently set up by
    // MtohRenderOverride::setup().

    if (!hydraPreRender()) {
        return false;
    }

    if (!hydraRender()) {
        return false;
    }

    return true;
}

bool HydraRenderCmd::hydraPreRender()
{
    if (!_gpuEnabled) {
        // Nothing to do, early out.
        return true;
    }

    // If we need an OpenGL context, create one now.
    _renderWindow = std::make_unique<GLRenderWindow>();
    _renderWindow->Init();

    return true;
}

bool HydraRenderCmd::hydraRender()
{
    if (!_batchRenderer) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD, "_batchRenderer is a nullptr.\n");
        return false;
    }

    // Dispatch to the render path that matches the active render-settings type.
    const auto renderSettingsType
        = ReadRenderSettingsTypeFromRenderDelegate(_batchRenderer->GetRendererName());

    if (renderSettingsType == RenderSettingsType::HydraV1) {
        return hydraRenderFromHydraV1RenderSettings();
    }
    if (renderSettingsType == RenderSettingsType::HydraV2) {
        return hydraRenderFromHydraV2RenderSettings();
    }

    displayError(std::string("Batch rendering requires USD render settings (with at least one "
                             "render product) or a render-delegate-owned render pass. No usable "
                             "USD render settings were found, and render delegate '")
                 + _batchRenderer->GetRendererName().GetText()
                 + "' does not drive the render pass.");
    return false;
}

MStatus HydraRenderCmd::doIt(const MArgList& args)
{
    MStatus status;

    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    if (!parseDatabase(db)) {
      return MS::kFailure;
    }

    TfToken rendererName;
    if (db.isFlagSet(_renderer)) {
        MString rn;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_renderer, 0, rn));

        rendererName = TfToken(rn.asChar());
        if (rendererName.IsEmpty()) {
            displayError("the -renderer/-r flag was set to an empty renderer name.");
            return MS::kFailure;
        }
    }
    else {
        // Get renderer from the scene.
        rendererName = GetCurrentRenderer();
        if (rendererName.IsEmpty()) {
            displayError("no renderer specified. Pass -renderer/-r, or author the "
                         "currentRenderer attribute on the USD render-description node.");
            return MS::kFailure;
        }
    }

    // Validate the renderer
    if (!validRenderer(rendererName)) {
        displayError(
            std::string("\"") + rendererName.GetText()
            + "\" is not a registered Hydra renderer. Pass a valid -renderer/-r, or "
              "author the currentRenderer attribute on UsdDefaultRenderDescription.");
        return MS::kFailure;
    }

    if (db.isFlagSet(_gpuEnabledFlag)) {
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_gpuEnabledFlag, 0, _gpuEnabled));
    }

    // Check if we were asked to add a session layer onto a stage.  If
    // no explicit sessionLayerStage argument was given to the Render
    // executable, use the UsdDefaultRenderDescription stage.
    const auto sessionLayer = getSessionLayer();
    if (!sessionLayer.empty()) {
        auto sessionLayerStagePath = getSessionLayerStage();
        if (sessionLayerStagePath.empty()) {
            sessionLayerStagePath = UsdDefaultRenderDescriptionNodePath();
        }
        if (!applySessionLayer(sessionLayer, sessionLayerStagePath)) {
            displayError(
                std::string("failed to apply session layer \"") + sessionLayer + "\".");
            return MS::kFailure;
        }
    }

    // Create the batch renderer.  The second and third arguments of
    // the renderer description are the unused override name and
    // display name, respectively.
    _batchRenderer = std::make_unique<BatchRenderer>(
        MtohRendererDescription(rendererName, {}, {}));

    if (db.isFlagSet(_currentFrame)) {
        const auto currentTime = MAnimControl::currentTime();
        _batchRenderer->SetRenderTimes(
            RenderTimes({ { currentTime, currentTime } }, 1.0f));
    }

    if (db.isFlagSet(_frameShort)) {
        double frameValue = 0.0;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_frameShort, 0, frameValue));
        const MTime frameTime(frameValue, MTime::uiUnit());
        _batchRenderer->SetRenderTimes(
            RenderTimes({ { frameTime, frameTime } }, 1.0f));
    }

    // Initialize Hydra renderer.
    if (initialize()) {
        if (render()) {
            return MS::kSuccess;
        }
    }

    return MS::kFailure;
}

}
