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

#include "adskHydraSceneBrowserTesting.h"

#include "adskHydraSceneBrowserTestFixture.h"

#include <gtest/gtest.h>

bool RunTestsWithFilter(std::string filter)
{
    // Create GoogleTest arguments
    int                argc = 1;
    std::vector<char*> argv(argc);
    argv[0] = const_cast<char*>("AdskHydraSceneBrowserTesting");

    // Setup GoogleTest
    ::testing::GTEST_FLAG(filter) = filter;
    ::testing::InitGoogleTest(&argc, argv.data());

    // Run the tests
    int testsResult = RUN_ALL_TESTS();

    // Return pass/fail status (testsResult == 0 if all tests passed, 1 otherwise)
    return testsResult == 0;
}

namespace AdskHydraSceneBrowserTesting {
bool RunFullSceneIndexComparisonTest(pxr::HdSceneIndexBasePtr referenceSceneIndex)
{
    AdskHydraSceneBrowserTestFixture::SetReferenceSceneIndex(referenceSceneIndex);
    return RunTestsWithFilter("AdskHydraSceneBrowserTestFixture.FullSceneIndexComparison");
}
bool RunSceneCorrectnessTest(pxr::HdSceneIndexBasePtr referenceSceneIndex)
{
    AdskHydraSceneBrowserTestFixture::SetReferenceSceneIndex(referenceSceneIndex);
    return RunTestsWithFilter("AdskHydraSceneBrowserTestFixture.VerifySceneCorrectness");
}
} // namespace AdskHydraSceneBrowserTesting

TEST_F(AdskHydraSceneBrowserTestFixture, FullSceneIndexComparison)
{
    // We want to do a full comparison, so also compare both data sources hierarchy and values
    bool compareDataSourceHierarchy = true;
    bool compareDataSourceValues = true;
    ComparePrimHierarchy(compareDataSourceHierarchy, compareDataSourceValues);
}

TEST_F(AdskHydraSceneBrowserTestFixture, VerifySceneCorrectness)
{
    VerifySceneCorrectness();
}
