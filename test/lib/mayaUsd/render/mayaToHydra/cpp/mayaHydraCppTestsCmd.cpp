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
#include "mayaHydraCppTestsCmd.h"

#include "testUtils.h"

#include <maya/MArgDatabase.h>
#include <maya/MFnPlugin.h>
#include <maya/MGlobal.h>
#include <maya/MString.h>
#include <maya/MSyntax.h>

#include <gtest/gtest.h>

namespace 
{
constexpr auto _filter = "-f";
constexpr auto _filterLong = "-filter";
constexpr auto _inputDir = "-id";
constexpr auto _inputDirLong = "-inputDir";
constexpr auto _outputDir = "-od";
constexpr auto _outputDirLong = "-outputDir";
}

MSyntax mayaHydraCppTestCmd::createSyntax()
{
    MSyntax syntax;
    syntax.addFlag(_filter, _filterLong, MSyntax::kString);
    syntax.addFlag(_inputDir, _inputDirLong, MSyntax::kString);
    syntax.addFlag(_outputDir, _outputDirLong, MSyntax::kString);
    return syntax;
}

std::vector<std::string> constructGoogleTestArgs(const MArgDatabase& database)
{
    std::vector<std::string> args;
    args.emplace_back("mayahydra_tests");

    MString filter = "*";
    if (database.isFlagSet(_filter)) {
        database.getFlagArgument(_filter, 0, filter);
    }
    ::testing::GTEST_FLAG(filter) = filter.asChar();

    MString inputDir;
    if (database.isFlagSet(_inputDir) && database.getFlagArgument(_inputDir, 0, inputDir)) {
        MayaHydra::setInputDir(inputDir.asChar());
    }

    MString outputDir;
    if (database.isFlagSet(_outputDir) && database.getFlagArgument(_outputDir, 0, outputDir)) {
        MayaHydra::setOutputDir(outputDir.asChar());
    }

    return args;
}

MStatus mayaHydraCppTestCmd::doIt( const MArgList& args ) 
{
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    std::vector<std::string> arguments = constructGoogleTestArgs(db);

    char**  argv = new char*[arguments.size()];
    int32_t argc(arguments.size());
    for (int32_t i = 0; i < argc; ++i) {
        argv[i] = (char*)arguments[i].c_str();
    }

    // By default, if no filter flag is given, all tests will run
    ::testing::InitGoogleTest(&argc, argv);
    if (RUN_ALL_TESTS() == 0 && ::testing::UnitTest::GetInstance()->test_to_run_count() > 0) {
        MGlobal::displayInfo("This test passed.");
        return MS::kSuccess;
    }

    MGlobal::displayInfo("This test failed.");
    return MS::kFailure;
}

MStatus initializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj, "Autodesk", "1.0", "Any" );
    plugin.registerCommand( "mayaHydraCppTest", mayaHydraCppTestCmd::creator, mayaHydraCppTestCmd::createSyntax );
    return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj );
    plugin.deregisterCommand( "mayaHydraCppTest" );
    return MS::kSuccess;
}