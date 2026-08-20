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

    // Only returns true when -testKeepAlive was passed, so Maya retains this
    // command (and its BatchRenderer) instead of deleting it right after
    // doIt() returns. Lets tests inspect the terminal scene index while the
    // batch renderer is still alive. Not used by real batch renders.
    bool    isUndoable() const override { return _testKeepAlive; }
    MStatus undoIt() override { return MS::kSuccess; }

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
    bool                            _testKeepAlive{false};
};

}

#endif // MAYAHYDRA_HYDRA_RENDER_CMD_H
