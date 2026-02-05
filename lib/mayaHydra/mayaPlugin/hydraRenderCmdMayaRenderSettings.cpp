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

#include "batchRenderer.h"
#include "pluginDebugCodes.h"

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <ufeExtensions/Global.h>

#include <maya/MAnimControl.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MFileIO.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MGlobal.h>
#include <maya/MRenderUtil.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>
#include <maya/MStringArray.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/scoped.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>

#include <filesystem>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool IsMayaCameraRenderable(const MDagPath& cameraPath)
{
    // Camera attributes are on shape node beneath transform.
    MDagPath shapePath = cameraPath;
    if (shapePath.extendToShape() != MS::kSuccess) {
        return false;
    }

    MStatus status;
    MFnDagNode cameraFn(shapePath, &status);
    if (status != MS::kSuccess) {
        return false;
    }

    return cameraFn.findPlug("renderable", /* wantNetworkedPlug = */ true).asBool();
}

bool GetMayaCameraDagPath(const MString& cameraName, MDagPath& cameraPath)
{
    MSelectionList sn;
    sn.add(cameraName);
    if (sn.getDagPath(0, cameraPath) != MS::kSuccess) {
        return false;
    }

    return IsMayaCameraRenderable(cameraPath);
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

bool HydraRenderCmd::hydraRenderFromMayaRenderSettings()
{ 
    if (!_batchRenderer) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD, "_batchRenderer is a nullptr.\n");
        return false;
    }
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "hydraRenderFromMayaRenderSettings() start.\n");
    MStatus status;

    // Get maya render settings
    MCommonRenderSettingsData mayaRenderSettings;
    MRenderUtil::getCommonRenderSettings(mayaRenderSettings);
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Maya render settings: %u x %u, animated=%d, frameStart=%.3f, frameEnd=%.3f, frameBy=%.3f\n",
        mayaRenderSettings.width,
        mayaRenderSettings.height,
        mayaRenderSettings.isAnimated(),
        MTime(mayaRenderSettings.frameStart).as(MTime::uiUnit()),
        MTime(mayaRenderSettings.frameEnd).as(MTime::uiUnit()),
        MTime(mayaRenderSettings.frameBy).as(MTime::uiUnit()));

    // Parse the scene name
    std::filesystem::path scenePath { MFileIO::currentFile().asChar() };
    MString               sceneName { scenePath.stem().c_str() };
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Scene name: %s\n",
        sceneName.asChar());

    BatchRenderer::InputParams inputParams;
    inputParams.width = mayaRenderSettings.width;
    inputParams.height = mayaRenderSettings.height;
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Initial render settings resolution: %u x %u\n",
        inputParams.width,
        inputParams.height);

    // Render color buffer by default
    inputParams.renderVarsInfo.renderVars = TfTokenVector { HdAovTokens->color };
 
    // Not considering render layers at all: both renderSetup and
    // legacy render layers are treated as legacy functionality.  Use
    // default render layer for all render products.
    const auto renderLayer = MFnRenderLayer::defaultRenderLayer(&status);
    if (status != MS::kSuccess) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD, "Failed to get default render layer.\n");
        return false;
    }

     // Get all renderable cameras in the scene.  As of 21-Mar-2025 no
    // C++ way to retrieve UFE cameras, so go through Maya command.
    // For Maya cameras only, use
    // MItDag dagIterCameras(MItDag::kDepthFirst, MFn::kCamera);
    // to traverse the whole Maya scene.
    // For now (21-Mar-2025), consider only Maya cameras.
    MStringArray cameras;
    // status = MGlobal::executeCommand("listCameras -ufe", cameras);
    status = MGlobal::executeCommand("listCameras", cameras);
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Renderable camera count (listCameras): %u\n",
        cameras.length());

    // Loop over all render times.
    auto timeStart  = mayaRenderSettings.frameStart;
    auto timeEnd    = mayaRenderSettings.frameEnd;
    auto timeIncr   = mayaRenderSettings.frameBy;

    // If the file naming scheme does not correspond to an animation,
    // use the current time.
    if (!mayaRenderSettings.isAnimated()) {
        timeStart = MAnimControl::currentTime();
        timeIncr = 1.0f;
        timeEnd = timeStart;
    }
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Render time range: start=%.3f end=%.3f by=%.3f\n",
        timeStart.as(MTime::uiUnit()),
        timeEnd.as(MTime::uiUnit()),
        timeIncr);

    for (MTime time = timeStart; time <= timeEnd; time += timeIncr) {

        const double frameNb = time.as(MTime::uiUnit());
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Rendering frame %.3f\n",
            frameNb);
        if (MAnimControl::currentTime() != time) {
            MAnimControl::setCurrentTime(time);
        }

        // Loop over all cameras.  FIXME  Probably ways to compute
        // non-animated camera data once, for animated renders.
        auto cameraIt = cameras.cbegin();
        for (; cameraIt != cameras.cend(); ++cameraIt) {
            auto camera = *cameraIt;

            MDagPath cameraPath;
            if (! GetMayaCameraDagPath(camera, cameraPath)) {
                // Non-renderable camera or error.
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Skipping non-renderable camera: %s\n",
                    camera.asChar());
                continue;
            }
            
            inputParams.ufeCameraPath
                = Ufe::Path(UfeExtensions::dagPathToUfePathSegment(cameraPath));
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                "Using camera: %s\n",
                camera.asChar());
            // View/projection matrices are derived from the UFE camera path
            // inside BatchRenderer::RenderFromMayaRenderSettings().

            // Set the output filename.
            const auto imageName = mayaRenderSettings.getImageName(
                MCommonRenderSettingsData::kFullPathImage,
                frameNb,
                sceneName,
                camera,
                "", // Use render settings file format
                renderLayer,
                /* createDirectory = */ true,
                &status);
            if (status != MS::kSuccess) {
                return false;
            }

            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_BATCHRENDER_CMD, "Render image name is %s.\n", imageName.asChar());

            auto     resetFileName = []() { Fvp::ImageBufferWriter::SetFileName(""); };
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

} // namespace MAYAHYDRA_NS_DEF
