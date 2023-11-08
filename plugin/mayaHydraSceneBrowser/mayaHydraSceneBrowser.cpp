// Copyright 2023 Autodesk
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
#include "mayaHydraSceneBrowser.h"

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaHydraLibInterfaceImp.h>

#include <sceneIndexDebuggerWidget.h>

#include <maya/MFnPlugin.h>
#include <maya/MArgList.h>
#include <maya/MGlobal.h>
#include <maya/MQtUtil.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MAYAHYDRA_NS;

HduiSceneIndexDebuggerWidget* widget = nullptr;
MStatus mayaHydraSceneBrowserCmd::doIt( const MArgList& args )
{
    const SceneIndicesVector& sceneIndices = GetMayaHydraLibInterface().GetTerminalSceneIndices();
    if (sceneIndices.empty()) {
        MGlobal::displayError("There are no registered terminal scene indices. The Hydra Scene Browser will not be shown.");
        return MS::kFailure;
    }
    else {
        if (!widget) {
            widget = new HduiSceneIndexDebuggerWidget(MQtUtil::mainWindow());
        }
        widget->setWindowTitle("Hydra Scene Browser");
        widget->setWindowFlags(Qt::Tool); // Ensure the browser stays in front of the main Maya window
        widget->SetSceneIndex("", sceneIndices.front(), true);
        widget->show();
    }

    return MS::kSuccess;
}

MStatus initializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj, "Autodesk", "1.0", "Any" );
    plugin.registerCommand( "mayaHydraSceneBrowser", mayaHydraSceneBrowserCmd::creator );
    return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj );
    plugin.deregisterCommand( "mayaHydraSceneBrowser" );
    return MS::kSuccess;
}