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
    PXR_NS::TfToken                name;
    PXR_NS::HdDataSourceBaseHandle dataSource;
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
                = PXR_NS::HdSampledDataSource::Cast(dataSourceEntry.dataSource)) {
                
                PXR_NS::VtValue value = sampledDataSource->GetValue(0.0f);
                if (value.IsHolding<PXR_NS::VtArray<PXR_NS::TfToken>>()) {
                    auto array = value.UncheckedGet<PXR_NS::VtArray<PXR_NS::TfToken>>();

                    int numPointReprs = 0;
                    int numWireReprs  = 0;
                    int numSurfReprs  = 0;

                    // Count representations in use 
                    for (size_t j = 0; j < array.size(); ++j) {
                        PXR_NS::TfToken reprName(array[j]);

                        if (reprName == PXR_NS::HdReprTokens->hull || 
                            reprName == PXR_NS::HdReprTokens->smoothHull || 
                            reprName == PXR_NS::HdReprTokens->refined) {
                            ++numSurfReprs;
                        } else if (reprName == PXR_NS::HdReprTokens->refinedWire ||
                                reprName == PXR_NS::HdReprTokens->wire) {
                            ++numWireReprs;
                        } else if (reprName == PXR_NS::HdReprTokens->refinedWireOnSurf ||
                                reprName == PXR_NS::HdReprTokens->wireOnSurf) {
                            ++numWireReprs;
                            ++numSurfReprs;
                        } else if (reprName == PXR_NS::HdReprTokens->points) {
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
            = PXR_NS::HdContainerDataSource::Cast(dataSourceEntry.dataSource)) {
            PXR_NS::TfTokenVector childNames = containerDataSource->GetNames();
            for (auto itChildNames = childNames.rbegin(); itChildNames != childNames.rend();
                 itChildNames++) {
                PXR_NS::TfToken                dataSourceName = *itChildNames;
                PXR_NS::HdDataSourceBaseHandle dataSource = containerDataSource->Get(dataSourceName);
                if (dataSource) {
                    dataSourceStack.push({ dataSourceName, dataSource });
                }
            }
        } else if (
            auto vectorDataSource = PXR_NS::HdVectorDataSource::Cast(dataSourceEntry.dataSource)) {
            for (size_t iElement = 0; iElement < vectorDataSource->GetNumElements(); iElement++) {
                size_t reversedElementIndex = vectorDataSource->GetNumElements() - 1 - iElement;
                PXR_NS::TfToken dataSourceName = PXR_NS::TfToken(std::to_string(reversedElementIndex));
                PXR_NS::HdDataSourceBaseHandle dataSource
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
    PXR_NS::HdSceneIndexBasePtr sceneIndex = sceneIndices.front();

    // Traverse the hierarchy
    std::stack<PXR_NS::SdfPath> primPathsStack({ PXR_NS::SdfPath::AbsoluteRootPath() });
    while (!primPathsStack.empty()) {
        PXR_NS::SdfPath primPath = primPathsStack.top();
        PXR_NS::HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);

        // Verify the data source
        VerifyDataSource({ primPath.GetNameToken(), prim.dataSource });

        // Prepare next step
        primPathsStack.pop();
        PXR_NS::SdfPathVector childPaths = sceneIndex->GetChildPrimPaths(primPath);
        for (auto itChildPaths = childPaths.rbegin(); itChildPaths != childPaths.rend();
             itChildPaths++) {
            primPathsStack.push(*itChildPaths);
        }
    }
}
