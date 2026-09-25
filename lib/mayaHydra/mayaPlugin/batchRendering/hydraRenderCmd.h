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
#ifndef MAYAHYDRA_HYDRA_RENDER_CMD_H
#define MAYAHYDRA_HYDRA_RENDER_CMD_H

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MPxCommand.h>

#include <pxr/base/tf/token.h>

#include <memory>

namespace MAYAHYDRA_NS_DEF {

class BatchRenderer;
class GLRenderWindow;

class HydraRenderCmd : public MPxCommand
{
public:
    static void*   creator() { return new HydraRenderCmd(); }
    static MSyntax createSyntax();

    static const MString name;

    ~HydraRenderCmd();

    MStatus doIt(const MArgList& args) override;

private:

    HydraRenderCmd();

    bool parseDatabase(const MArgDatabase& db);
    bool initialize();
    bool render();
    bool hydraRender();
    bool hydraPreRender();
    bool hydraRenderFromHydraV1RenderSettings();
    bool hydraRenderFromHydraV2RenderSettings();

    /*! \brief Resolve and validate the renderer to use for this batch render.
     *
     *  Resolution order: the -renderer/-r flag if set, otherwise the
     *  currentRenderer attribute on the USD render-description node.
     *  There is no hardcoded default renderer, so a no-flag invocation
     *  requires that attribute to be authored.
     *
     *  The resolved name is checked against the registered Hydra renderer
     *  plugins; an unregistered name is a hard error here, before
     *  BatchRenderer is constructed. This does not guarantee the plugin can
     *  actually be instantiated (e.g. missing GPU context): that is still
     *  reported separately by BatchRenderer::_InitHydraResources().
     */
    PXR_NS::TfToken GetRenderer();

    std::unique_ptr<BatchRenderer>  _batchRenderer;
    std::unique_ptr<GLRenderWindow> _renderWindow;
    bool                            _gpuEnabled{false};
    bool                            _rendererFlagSet{false};
    PXR_NS::TfToken                 _rendererFromFlag;
};

}

#endif // MAYAHYDRA_HYDRA_RENDER_CMD_H
