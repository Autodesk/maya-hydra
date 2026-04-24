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
#include "renderVarUtils.h"
#include "pluginDebugCodes.h"
#include "batchRenderer.h"

#include <ufeExtensions/Global.h>

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <maya/MArgDatabase.h>
#include <maya/MAnimControl.h>
#include <maya/MGlobal.h>
#include <maya/MSyntax.h>
#include <maya/MFnCamera.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MRenderUtil.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MFileIO.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/scoped.h>
#include <pxr/imaging/glf/diagnostic.h> // For GlfRegisterDefaultDebugOutputMessageCallback()
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/garch/glDebugWindow.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdRender/product.h>
#include <pxr/usd/usdRender/var.h>
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usd/usdGeom/camera.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/vt/value.h>

#include <ufe/pathString.h>
#include <ufe/sceneSegmentHandler.h>
#include <ufe/sceneItemList.h>
#include <ufe/runTimeMgr.h>

#include <mayaUsdAPI/proxyStage.h>
#include <mayaUsdAPI/utils.h>

#include <algorithm>
#include <filesystem>
#include <vector>

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

using namespace MAYAHYDRA_NS_DEF;

constexpr auto _helpText = R"HELP(For details on args usage please see 
https://github.com/Autodesk/maya-hydra/blob/dev/doc/mayaHydraCommands.md
)HELP";
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

HydraRenderCmd::~HydraRenderCmd() = default;

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

    return hydraRenderFromMayaRenderSettings();
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

    // By default the renderer is Hydra Storm.
    TfToken rendererName("HdStormRendererPlugin");
    if (db.isFlagSet(_renderer)) {
        MString rn;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_renderer, 0, rn));

        rendererName = TfToken(rn.asChar());
    }

    if (db.isFlagSet(_gpuEnabledFlag)) {
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_gpuEnabledFlag, 0, _gpuEnabled));
    }

    // Create the batch renderer.  The second and third arguments of
    // the renderer description are the unused override name and
    // display name, respectively.
    _batchRenderer = std::make_unique<BatchRenderer>(
        MtohRendererDescription(rendererName, {}, {}));

    // Initialize Hydra renderer.
    if (initialize()) {
        if (render()) {
            return MS::kSuccess;
        }
    }

    return MS::kFailure;
}

}
