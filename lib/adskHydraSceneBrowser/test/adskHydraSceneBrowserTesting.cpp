
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
} // namespace AdskHydraSceneBrowserTesting

TEST_F(AdskHydraSceneBrowserTestFixture, FullSceneIndexComparison)
{
    // We want to do a full comparison, so also compare both data sources hierarchy and values
    bool compareDataSourceHierarchy = true;
    bool compareDataSourceValues = true;
    ComparePrimHierarchy(compareDataSourceHierarchy, compareDataSourceValues);
}
