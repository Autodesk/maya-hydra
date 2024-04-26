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

#include <flowViewport/fvpInstruments.h>

#include <maya/MString.h>
#include <maya/MFnPlugin.h>
#include <maya/MArgDatabase.h>
#include <maya/MSyntax.h>
#include <maya/MGlobal.h>
#include <gtest/gtest.h>

namespace 
{
constexpr auto _filter = "-f";
constexpr auto _filterLong = "-filter";

MStringArray constructGoogleTestArgs(const MArgDatabase& database)
{
    MString filter = "*";

    if (database.isFlagSet("-f")) {
        if (database.getFlagArgument("-f", 0, filter)) { }
    }

    MStringArray args;
    MStatus status = database.getObjects(args);
    CHECK_MSTATUS_AND_RETURN(status, MStringArray());

    // As per https://github.com/google/googletest/issues/3395
    // the InitGoogleTest() check verifies that the argv passed to it has at
    // least one argument, otherwise it emits the following warning:
    //
    // IMPORTANT NOTICE - DO NOT IGNORE:
    // This test program did NOT call testing::InitGoogleTest() before calling RUN_ALL_TESTS(). This is INVALID. Soon Google Test will start to enforce the valid usage. Please fix it ASAP, or IT WILL START TO FAIL.
    //
    // Therefore, add a dummy argument if required.
    if (args.length() == 0) {
        args.append("dummy");
    }

    ::testing::GTEST_FLAG(filter) = filter.asChar();

    return args;
}

}

MSyntax mayaHydraCppTestCmd::createSyntax()
{
    MSyntax syntax;
    syntax.addFlag(_filter, _filterLong, MSyntax::kString);
    syntax.setObjectType(MSyntax::kStringObjects);
    return syntax;
}

MStatus mayaHydraCppTestCmd::doIt( const MArgList& args ) 
{
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    auto arguments = constructGoogleTestArgs(db);

    char**  argv = new char*[arguments.length()];
    int32_t argc(arguments.length());
    for (int32_t i = 0; i < argc; ++i) {
        argv[i] = (char*)arguments[i].asChar();
    }

    // By default, if no filter flag is given, all tests will run
    ::testing::InitGoogleTest(&argc, argv);
    MayaHydra::setTestingArgs(argc, argv);
    if (RUN_ALL_TESTS() == 0 && ::testing::UnitTest::GetInstance()->test_to_run_count() > 0) {
        MGlobal::displayInfo("This test passed.");
        return MS::kSuccess;
    }

    MGlobal::displayInfo("This test failed.");
    return MS::kFailure;
}

//============================================================
// CLASS mayaHydraInstruments
//============================================================

MSyntax mayaHydraInstruments::createSyntax()
{
    MSyntax syntax;
    syntax.setMinObjects(1);
    syntax.setMaxObjects(1);
    syntax.setObjectType(MSyntax::kStringObjects);
    syntax.enableQuery(true);
    syntax.enableEdit(false);
    return syntax;
}

MStatus mayaHydraInstruments::doIt( const MArgList& args ) 
{
    MStatus status;
    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    if (!db.isQuery()) {
        return MS::kFailure;
    }

    MStringArray instruments;   // Will be size 1.
    status = db.getObjects(instruments);
    CHECK_MSTATUS_AND_RETURN_IT(status);
    
    if (instruments.length() != 1u) {
        displayError("Must query a single instrument.");
        return MS::kFailure;
    }

    auto value = Fvp::Instruments::instance().get(instruments[0].asChar());
    
    if (value.IsEmpty()) {
        displayError("Queried instrument has no value.");
        return MS::kFailure;
    }

    // Type-based translation of VtValue to result.
    if (value.IsHolding<int>()) {
        setResult(value.UncheckedGet<int>());
    }
    else if (value.IsHolding<long int>()) {
        setResult(int(value.UncheckedGet<long int>()));
    }
    else if (value.IsHolding<float>()) {
        setResult(value.UncheckedGet<float>());
    }
    else if (value.IsHolding<double>()) {
        setResult(value.UncheckedGet<double>());
    }

    return MS::kSuccess;
}

//============================================================
// CLASS mayaHydraCppTests
//============================================================

MStatus initializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj, "Autodesk", "1.0", "Any" );
    plugin.registerCommand( "mayaHydraCppTest", mayaHydraCppTestCmd::creator, mayaHydraCppTestCmd::createSyntax );
    plugin.registerCommand( "mayaHydraInstruments", mayaHydraInstruments::creator, mayaHydraInstruments::createSyntax );
    return MS::kSuccess;
}

MStatus uninitializePlugin( MObject obj ) 
{
    MFnPlugin plugin( obj );
    plugin.deregisterCommand( "mayaHydraCppTest" );
    plugin.deregisterCommand( "mayaHydraInstruments" );
    return MS::kSuccess;
}
