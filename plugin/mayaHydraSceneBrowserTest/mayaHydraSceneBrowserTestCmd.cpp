
#include "mayaHydraSceneBrowserTestCmd.h"

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/interfaceImp.h>

#include <maya/MArgList.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>

#include <adskHydraSceneBrowserTesting.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MAYAHYDRA_NS;

const MString mayaHydraSceneBrowserTestCmd::name("mayaHydraSceneBrowserTest");

MStatus mayaHydraSceneBrowserTestCmd::doIt(const MArgList& args)
{
    // Retrieve the scene index
    const SceneIndicesVector& sceneIndices = GetMayaHydraLibInterface().GetTerminalSceneIndices();
    if (sceneIndices.empty()) {
        MGlobal::displayError("There are no registered terminal scene indices. The Hydra Scene "
                              "Browser test will not be run.");
        return MS::kFailure;
    }

    // Run comparison test
    bool didTestSucceed
        = AdskHydraSceneBrowserTesting::RunFullSceneIndexComparisonTest(sceneIndices.front());

    // Display result
    MString testResultString = didTestSucceed ? MString("PASS") : MString("FAIL");
    MGlobal::displayInfo("Hydra Scene Browser comparison test result : " + testResultString);

    // Return result
    return didTestSucceed ? MS::kSuccess : MS::kFailure;
}

MStatus initializePlugin(MObject obj)
{
    MFnPlugin plugin(obj, "Autodesk", "1.0", "Any");
    plugin.registerCommand(
        mayaHydraSceneBrowserTestCmd::name, mayaHydraSceneBrowserTestCmd::creator);
    return MS::kSuccess;
}

MStatus uninitializePlugin(MObject obj)
{
    MFnPlugin plugin(obj);
    plugin.deregisterCommand(mayaHydraSceneBrowserTestCmd::name);
    return MS::kSuccess;
}
