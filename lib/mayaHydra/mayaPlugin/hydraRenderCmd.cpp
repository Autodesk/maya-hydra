//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#include <ufeExtensions/Global.h>

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <maya/MArgDatabase.h>
#include <maya/MGlobal.h>
#include <maya/MSyntax.h>
#include <maya/MFnCamera.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MRenderUtil.h>
#include <maya/MFnRenderLayer.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/scoped.h>

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

// hydraRender [-renderer string] [-camera string] [-currentFrame] [-frame float] [-height uint] [-layer name] [-width uint] 

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

// TODO_BATCH_RENDER  Add documentation for this command.
constexpr auto _helpText = R"HELP(For details on args usage please see 
https://github.com/Autodesk/maya-hydra/blob/dev/doc/mayaHydraCommands.md
)HELP";

} // namespace

namespace MAYAHYDRA_NS_DEF {

const MString HydraRenderCmd::name("hydraRender");

MSyntax HydraRenderCmd::createSyntax()
{
    MSyntax syntax;

    syntax.addFlag(_width, _widthLong, MSyntax::kUnsigned);
    syntax.addFlag(_height, _heightLong, MSyntax::kUnsigned);
    syntax.addFlag(_camera, _cameraLong, MSyntax::kString);
    syntax.addFlag(_renderer, _rendererLong, MSyntax::kString);
    syntax.addFlag(_currentFrame, _currentFrameLong);
    syntax.addFlag(_frameShort, _frameLong, MSyntax::kDouble);
    syntax.addFlag(_layer, _layerLong, MSyntax::kString);

    return syntax;
}

HydraRenderCmd::HydraRenderCmd() 
    : _batchRenderer(MtohRendererDescription(
          TfToken("HdStormRendererPlugin"),
          TfToken("mayaHydraRenderOverride_HdStormRendererPlugin"),
          TfToken("(Technology Preview) Hydra GL)")))
{}

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

    // hydraPreRender();

    if (!hydraRender()) {
        return false;
    }

    // Do we need this one?  Or just write to disk using the new image write
    // functionality?
    // hydraPresentTarget();

    return true;
}

bool HydraRenderCmd::hydraRender()
{
    MStatus status;

    // Get render settings from Maya.
    MCommonRenderSettingsData renderSettings;
    MRenderUtil::getCommonRenderSettings(renderSettings);

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
    
    // Loop over all cameras.
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
            std::cout << "PPT: camera " << camera.asChar() 
                      << " not renderable, skipped." << std::endl;
            continue;
        }

        inputParams.cameraPath = cameraPath;
        inputParams.ufeCameraPath = Ufe::Path(
            UfeExtensions::dagPathToUfePathSegment(cameraPath));
        inputParams.viewMatrix = GetGfMatrixFromMaya(
            cameraPath.inclusiveMatrixInverse());
        inputParams.projectionMatrix = GetGfMatrixFromMaya(
            MFnCamera(cameraPath).projectionMatrix());
    
        // Unclear how to translate Maya data.
        // MHWRender::MDataServerOperation::MViewportScene carries MRenderItem's
        // created by OGS, and has some level of change notification.  Should
        // we still somehow rely on OGS?  Seems like the best approach.
        // Another possibility would be to use in-memory conversion to USD
        // from Maya USD's duplicate as USD.  For the moment pass in an
        // empty scene and don't translate Maya data.
        MHWRender::MDataServerOperation::MViewportScene scene;
    
        // Set the output filename.
        const auto imageName = renderSettings.getImageName(
            MCommonRenderSettingsData::kFullPathImage,
            0.0,
            "myScene",
            camera,
            "",                     // Use render settings file format
            renderLayer,
            /* createDirectory = */ true,
            &status);
        if (status != MS::kSuccess) {
            return false;
        }
    
        std::cout << "PPT: render settings file name is " << imageName.asChar()
                  << std::endl;
    
        auto resetFileName = []() { Fvp::ImageBufferWriter::SetFileName(""); };
        TfScoped guard(resetFileName);
        Fvp::ImageBufferWriter::SetFileName(imageName.asChar());
    
        if (_batchRenderer.Render(inputParams, scene) != MS::kSuccess) {
            return false;
        }
    }
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

    // Initialize Hydra renderer.
    if (initialize()) {
        if (render()) {
            return MS::kSuccess;
        }
    }

    return MS::kFailure;

#if 0
    TfToken renderDelegateName;
    if (db.isFlagSet(_rendererId)) {
        MString id;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_rendererId, 0, id));

        // Passing 'mayaHydra' as the renderer adresses all renderers
        if (id != "mayaHydra") {
            renderDelegateName = TfToken(id.asChar());
        }
    }

    if (db.isFlagSet(_hdGPUMem)) {
        appendToResult(MtohRenderOverride::GetUsedGPUMemory());
    } if (db.isFlagSet(_currentProcessMemory)) {
        appendToResult(getProcessMemory());
    } else if (db.isFlagSet(_listRenderers)) {
        for (auto& plugin : MtohGetRendererDescriptions())
            appendToResult(plugin.rendererName.GetText());

        // Want to return an empty list, not None
        if (!isCurrentResultArray()) {
            setResult(MStringArray());
        }
    } else if (db.isFlagSet(_listActiveRenderers)) {
        for (const auto& renderer : MtohRenderOverride::AllActiveRendererNames()) {
            appendToResult(renderer);
        }
        // Want to return an empty list, not None
        if (!isCurrentResultArray()) {
            setResult(MStringArray());
        }
    } else if (db.isFlagSet(_getRendererDisplayName)) {
        if (renderDelegateName.IsEmpty()) {
            MGlobal::displayError(
                MString("Must supply '") + _rendererIdLong + "' flag when using '"
                + _getRendererDisplayNameLong + "' flag");
            return MS::kInvalidParameter;
        }

        const auto dn = MtohGetRendererPluginDisplayName(renderDelegateName);
        setResult(MString(dn.c_str()));
    } else if (db.isFlagSet(_help)) {
        MString helpText = _helpText;
        MGlobal::displayInfo(helpText);
    } else if (db.isFlagSet(_createRenderGlobals)) {
        bool userDefaults = db.isFlagSet(_userDefaultsId);
        MtohRenderGlobals::CreateAttributes({ renderDelegateName, true, userDefaults });
    } else if (db.isFlagSet(_updateRenderGlobals)) {
        MString    attrFlag;
        const bool storeUserSettings = true;
        if (db.getFlagArgument(_updateRenderGlobals, 0, attrFlag) == MS::kSuccess) {
            bool          userDefaults = db.isFlagSet(_userDefaultsId);
            const TfToken attrName(attrFlag.asChar());
            auto&         inst = MtohRenderGlobals::GlobalChanged(
                { attrName, false, userDefaults }, storeUserSettings);
            MtohRenderOverride::UpdateRenderGlobals(inst, attrName);
            return MS::kSuccess;
        }
        MtohRenderOverride::UpdateRenderGlobals(
            MtohRenderGlobals::GetInstance(storeUserSettings), renderDelegateName);
    } else if (db.isFlagSet(_listRenderIndex)) {
        if (renderDelegateName.IsEmpty()) {
            MGlobal::displayError(
                MString("Must supply '") + _rendererIdLong + "' flag when using '"
                + _listRenderIndexLong + "' flag");
            return MS::kInvalidParameter;
        }

        auto rprimPaths
            = MtohRenderOverride::RendererRprims(renderDelegateName, db.isFlagSet(_visibleOnly));
        for (auto& rprimPath : rprimPaths) {
            appendToResult(rprimPath.GetText());
        }
        // Want to return an empty list, not None
        if (!isCurrentResultArray()) {
            setResult(MStringArray());
        }
    } else if (db.isFlagSet(_sceneDelegateId)) {
        if (renderDelegateName.IsEmpty()) {
            MGlobal::displayError(
                MString("Must supply '") + _rendererIdLong + "' flag when using '"
                + _sceneDelegateIdLong + "' flag");
            return MS::kInvalidParameter;
        }

        MString sceneDelegateName;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(_sceneDelegateId, 0, sceneDelegateName));

        SdfPath delegateId = MtohRenderOverride::RendererSceneDelegateId(
            renderDelegateName, TfToken(sceneDelegateName.asChar()));
        setResult(MString(delegateId.GetText()));
    } else if (db.isFlagSet(_majorVersion)) {
        setResult(MAYAHYDRA_MAJOR_VERSION);
    } else if (db.isFlagSet(_minorVersion)) {
        setResult(MAYAHYDRA_MINOR_VERSION);
    } else if (db.isFlagSet(_patchVersion)) {
        setResult(MAYAHYDRA_PATCH_LEVEL);
    } else if (db.isFlagSet(_buildNumber)) {
        setResult(MhBuildInfo::buildNumber());
    } else if (db.isFlagSet(_gitCommit)) {
        setResult(MhBuildInfo::gitCommit());
    } else if (db.isFlagSet(_gitBranch)) {
        setResult(MhBuildInfo::gitBranch());
    } else if (db.isFlagSet(_buildDate)) {
        setResult(MhBuildInfo::buildDate());
    } else if (db.isFlagSet(_usdVersion)) {
        setResult(PXR_VERSION);
    }
#endif
    return MS::kSuccess;
}

}
