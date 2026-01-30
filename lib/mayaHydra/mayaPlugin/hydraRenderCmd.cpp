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
#include <pxr/base/tf/scoped.h>
#include <pxr/imaging/glf/diagnostic.h> // For GlfRegisterDefaultDebugOutputMessageCallback()
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/garch/glDebugWindow.h>

#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

// hydraRender [-renderer string] [-camera string] [-currentFrame] [-frame float] [-height uint] [-layer name] [-width uint] [-gpu {0|1}]

namespace {

constexpr auto _width = "-w";
constexpr auto _widthLong = "-width";

constexpr auto _height = "-h";
constexpr auto _heightLong = "-height";

constexpr auto _camera = "-cam";
constexpr auto _cameraLong = "-camera";

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

// TODO_BATCH_RENDER  Add documentation for this command.
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
    : GarchGLDebugWindow("Maya Hydra Render", 100, 100) // Width, height relevance?
{}

/* virtual */
void
GLRenderWindow::OnInitializeGL()
{
    GarchGLApiLoad();
    GlfRegisterDefaultDebugOutputMessageCallback();

    std::cout << glGetString(GL_VENDOR) << "\n";
    std::cout << glGetString(GL_RENDERER) << "\n";
    std::cout << glGetString(GL_VERSION) << "\n";
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
    syntax.addFlag(_camera, _cameraLong, MSyntax::kString);
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
    return true;
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

    // Do we need this one?  Or just write to disk using the new image write
    // functionality?
    // hydraPresentTarget();

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
    MStatus status;

    // Get render settings from Maya.
    MCommonRenderSettingsData renderSettings;
    MRenderUtil::getCommonRenderSettings(renderSettings);

    // Unexpectedly renderSettings.name is empty, so parse the scene name
    // ourselves.
    std::filesystem::path scenePath{MFileIO::currentFile().asChar()};
    MString sceneName{scenePath.stem().c_str()};

    BatchRenderer::InputParams inputParams;
    inputParams.width = renderSettings.width;
    inputParams.height = renderSettings.height;

    // Get all renderable cameras in the scene.  As of 21-Mar-2025 no
    // C++ way to retrieve UFE cameras, so go through Maya command.
    // For Maya cameras only, use
    // MItDag dagIterCameras(MItDag::kDepthFirst, MFn::kCamera);
    // to traverse the whole Maya scene.
    // For now (21-Mar-2025), consider only Maya cameras.
    MStringArray cameras;
    // status = MGlobal::executeCommand("listCameras -ufe", cameras);
    status = MGlobal::executeCommand("listCameras", cameras);

    // Not considering render layers at all: both renderSetup and
    // legacy render layers are treated as legacy functionality.  Use
    // default render layer for all render products.
    const auto renderLayer = MFnRenderLayer::defaultRenderLayer(&status);
    if (status != MS::kSuccess) {
        return false;
    }
    
    // Loop over all render times.
    auto timeStart = renderSettings.frameStart;
    auto timeEnd   = renderSettings.frameEnd;
    auto timeIncr  = renderSettings.frameBy;

    // If the file naming scheme does not correspond to an animation,
    // use the current time.
    if (!renderSettings.isAnimated()) {
        timeStart = MAnimControl::currentTime();
        timeIncr = 1.0f;
        timeEnd = timeStart;
    }

    for (MTime time = timeStart; time <= timeEnd; time += timeIncr) {

        const double frameNb = time.as(MTime::uiUnit());
        if (MAnimControl::currentTime() != time) {
            MAnimControl::setCurrentTime(time);
        }

        // Loop over all cameras.  FIXME  Probably ways to compute
        // non-animated camera data once, for animated renders.
        auto cameraIt = cameras.cbegin();
        for (; cameraIt != cameras.cend(); ++cameraIt) {
            auto camera = *cameraIt;
    
            MSelectionList sn;
            sn.add(camera);
            MDagPath cameraPath;
            if (sn.getDagPath(0, cameraPath) != MS::kSuccess) {
                // Non-Maya UFE path.
                continue;
            }
    
            // Camera attributes are on shape node beneath transform
            if (cameraPath.extendToShape() != MS::kSuccess) {
                return false;
            }
    
            // Is the camera renderable?
            MFnDagNode cameraFn(cameraPath, &status);
            // MFnDependencyNode cameraFn(cameraPath.node(), &status);
            if (status != MS::kSuccess) {
                return false;
            }
    
            if (!cameraFn.findPlug(
                    "renderable", /* wantNetworkedPlug = */ true).asBool()) {
                continue;
            }
    
            inputParams.ufeCameraPath = Ufe::Path(
                UfeExtensions::dagPathToUfePathSegment(cameraPath));
            // View/projection matrices are derived from the UFE camera path
            // inside BatchRenderer::RenderFromMayaRenderSettings().
    
            // Set the output filename.
            const auto imageName = renderSettings.getImageName(
                MCommonRenderSettingsData::kFullPathImage,
                frameNb,
                sceneName,
                camera,
                "",                     // Use render settings file format
                renderLayer,
                /* createDirectory = */ true,
                &status);
            if (status != MS::kSuccess) {
                return false;
            }

            TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                         "Render image name is %s.\n", imageName.asChar());

            auto resetFileName = []() { Fvp::ImageBufferWriter::SetFileName(""); };
            TfScoped guard(resetFileName);
            Fvp::ImageBufferWriter::SetFileName(imageName.asChar());
        
            // Unclear how to translate Maya data.
            // MHWRender::MDataServerOperation::MViewportScene carries MRenderItem's
            // created by OGS, and has some level of change notification.
            // That class is used in the viewport renderer to translate Maya data.
            // Should we do the same here, should we still somehow rely on OGS?
            // Seems like the best approach. Another possibility would be to use
            // in-memory conversion to USD from Maya USD's duplicate as USD.
            // For the moment, don't pass in anything to the renderer.
            if (_batchRenderer->RenderFromMayaRenderSettings(inputParams) != MS::kSuccess) {
                return false;
            }
        } // Camera loop
    } // Time loop
    return true;
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
