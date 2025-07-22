// Copyright 2024 Autodesk
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

#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

#include <stack>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

struct DataSourceEntry
{
    pxr::TfToken                name;
    pxr::HdDataSourceBaseHandle dataSource;
};

void VerifyDataSource(DataSourceEntry rootDataSourceEntry)
{
    // Traverse the hierarchy and verify
    std::stack<DataSourceEntry> dataSourceStack({ rootDataSourceEntry });
    while (!dataSourceStack.empty()) {
        DataSourceEntry  dataSourceEntry = dataSourceStack.top();

        // Verify representation selector's correctness
        if (dataSourceEntry.name == "reprSelector") {
            if (auto sampledDataSource
                = pxr::HdSampledDataSource::Cast(dataSourceEntry.dataSource)) {
                
                pxr::VtValue value = sampledDataSource->GetValue(0.0f);
                if (value.IsHolding<pxr::VtArray<pxr::TfToken>>()) {
                    auto array = value.UncheckedGet<pxr::VtArray<pxr::TfToken>>();

                    int numPointReprs = 0;
                    int numWireReprs  = 0;
                    int numSurfReprs  = 0;

                    // Count representations in use 
                    for (size_t j = 0; j < array.size(); ++j) {
                        pxr::TfToken reprName(array[j]);

                        if (reprName == pxr::HdReprTokens->hull || 
                            reprName == pxr::HdReprTokens->smoothHull || 
                            reprName == pxr::HdReprTokens->refined) {
                            ++numSurfReprs;
                        } else if (reprName == pxr::HdReprTokens->refinedWire ||
                                reprName == pxr::HdReprTokens->wire) {
                            ++numWireReprs;
                        } else if (reprName == pxr::HdReprTokens->refinedWireOnSurf ||
                                reprName == pxr::HdReprTokens->wireOnSurf) {
                            ++numWireReprs;
                            ++numSurfReprs;
                        } else if (reprName == pxr::HdReprTokens->points) {
                            ++numPointReprs;
                        }
                    }

                    // Verify that we don't draw the same geometry more than once
                    EXPECT_LE(numPointReprs, 1);
                    EXPECT_LE(numWireReprs, 1);
                    EXPECT_LE(numSurfReprs, 1);
                }
            }
        }

        // Prepare next step
        dataSourceStack.pop();
        if (auto containerDataSource
            = pxr::HdContainerDataSource::Cast(dataSourceEntry.dataSource)) {
            pxr::TfTokenVector childNames = containerDataSource->GetNames();
            for (auto itChildNames = childNames.rbegin(); itChildNames != childNames.rend();
                 itChildNames++) {
                pxr::TfToken                dataSourceName = *itChildNames;
                pxr::HdDataSourceBaseHandle dataSource = containerDataSource->Get(dataSourceName);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        } else if (
            auto vectorDataSource = pxr::HdVectorDataSource::Cast(dataSourceEntry.dataSource)) {
            for (size_t iElement = 0; iElement < vectorDataSource->GetNumElements(); iElement++) {
                size_t reversedElementIndex = vectorDataSource->GetNumElements() - 1 - iElement;
                pxr::TfToken dataSourceName = pxr::TfToken(std::to_string(reversedElementIndex));
                pxr::HdDataSourceBaseHandle dataSource
                    = vectorDataSource->GetElement(reversedElementIndex);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        }
    }
}

} // namespace

TEST(HydraScene, testHydraSceneCorrectness)
{
    // Retrieve the scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    EXPECT_TRUE(!sceneIndices.empty());
    pxr::HdSceneIndexBasePtr sceneIndex = sceneIndices.front();

    // Traverse the hierarchy
    std::stack<pxr::SdfPath> primPathsStack({ pxr::SdfPath::AbsoluteRootPath() });
    while (!primPathsStack.empty()) {
        pxr::SdfPath primPath = primPathsStack.top();
        pxr::HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);

        // Verify the data source
        VerifyDataSource({ primPath.GetNameToken(), prim.dataSource });

        // Prepare next step
        primPathsStack.pop();
        pxr::SdfPathVector childPaths = sceneIndex->GetChildPrimPaths(primPath);
        for (auto itChildPaths = childPaths.rbegin(); itChildPaths != childPaths.rend();
             itChildPaths++) {
            primPathsStack.push(*itChildPaths);
        }
    }
}
