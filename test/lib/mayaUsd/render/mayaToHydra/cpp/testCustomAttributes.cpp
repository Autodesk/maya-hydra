// Copyright 2025 Autodesk
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

#include "testUtils.h"

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {
HdDataSourceLocator primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();

FindPrimPredicate getPrimPredicate(const std::string& primName, const TfToken& primType)
{
    return [primName,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetAsString().find(primName) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == primType;
    };
}

TEST(CustomAttributes, defaultArnoldCustomAttributes)
{ 
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    DecimalStreamingOverride decimalStreamingOverride({ pxr::TfDecimalToStringMode::FIXED, 5, false });

    HdSceneIndexPrim prim;
    bool             testPassed = false;
    for (const HdSceneIndexBaseRefPtr& sceneIndex : sceneIndices) {
        SceneIndexInspector inspector(sceneIndex);

        PrimEntriesVector foundPrims
            = inspector.FindPrims(getPrimPredicate("pCube1", HdPrimTypeTokens->mesh));
        if (foundPrims.size() == 1u) {
            prim = foundPrims.front().prim;
            EXPECT_EQ(prim.primType, HdPrimTypeTokens->mesh);
            ASSERT_NE(prim.dataSource, nullptr);

            HdDataSourceLocator visLocator
                = primvarsLocator.Append(TfToken("aiAutobumpVisibility"));
            dataSourceMatchesReference(
                HdContainerDataSource::Get(prim.dataSource, visLocator),
                getPathToSample("cube_primvar_aiAutobumpVisibility_fresh.txt"));
            // 
            //EXPECT_TRUE(dataSourceMatchesReference(
            //    HdContainerDataSource::Get(prim.dataSource, visLocator),
            //    getPathToSample("cube_primvar_aiAutobumpVisibility_fresh.txt")));

            //HdDataSourceLocator iterLocator = primvarsLocator.Append(TfToken("aiSubdivIterations"));
            //EXPECT_TRUE(dataSourceMatchesReference(
            //    HdContainerDataSource::Get(prim.dataSource, iterLocator),
            //    getPathToSample("cube_primvar_aiSubdivIterations_fresh.txt")));

            //MObject cubeNode;
            //ASSERT_TRUE(GetDependNodeFromNodeName("pCubeShape1", cubeNode));
            //EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiAutobumpVisibility", 0));
            //EXPECT_TRUE(dataSourceMatchesReference(
            //    HdContainerDataSource::Get(prim.dataSource, visLocator),
            //    getPathToSample("cube_primvar_aiAutobumpVisibility_modified.txt")));

            testPassed = true;
            break;
        }
    }
    ASSERT_TRUE(testPassed);
}

}
