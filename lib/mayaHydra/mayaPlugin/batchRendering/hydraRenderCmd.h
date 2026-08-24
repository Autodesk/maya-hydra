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
    bool hydraRenderFromMayaRenderSettings();
    bool hydraRenderFromHydraV1RenderSettings();
    bool hydraRenderFromHydraV2RenderSettings();

    std::unique_ptr<BatchRenderer>  _batchRenderer;
    std::unique_ptr<GLRenderWindow> _renderWindow;
    bool                            _gpuEnabled{false};
};

}

#endif // MAYAHYDRA_HYDRA_RENDER_CMD_H
