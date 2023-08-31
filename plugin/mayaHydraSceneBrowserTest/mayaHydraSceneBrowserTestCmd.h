
#ifndef MAYA_HYDRA_SCENE_BROWSER_TEST_CMD_H
#define MAYA_HYDRA_SCENE_BROWSER_TEST_CMD_H

#include <maya/MPxCommand.h>

class mayaHydraSceneBrowserTestCmd : public MPxCommand
{
public:
    static void* creator() { return new mayaHydraSceneBrowserTestCmd(); }

    static const MString name;

    MStatus doIt(const MArgList& args) override;
};

#endif // MAYA_HYDRA_SCENE_BROWSER_TEST_CMD_H
