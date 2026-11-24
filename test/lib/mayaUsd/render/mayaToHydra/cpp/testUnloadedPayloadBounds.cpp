// Copyright 2026 Autodesk
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

#include <cstdlib>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {
constexpr const char* kBoundsSuffix = "/__unloadedPayloadBounds";
}

// The bounding boxes of unloaded payloads are drawn by the secondary graphics pass, so
// they are not visible through the selected renderer's rprims. Inspect the secondary
// graphics pass scene index directly.
//
// Arguments: <USD prim path, e.g. "/A"> <expected number of bounding box prims>
TEST(UnloadedPayloadBounds, countBoundsPrims)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string usdPath(argv[0]);
    const long        expectedCount = std::strtol(argv[1], nullptr, 10);
    ASSERT_FALSE(usdPath.empty());

    const auto secondaryPassSceneIndex = GetSecondaryGraphicsPassSceneIndex();
    ASSERT_NE(secondaryPassSceneIndex, nullptr);

    const std::string suffix = usdPath + kBoundsSuffix;
    FindPrimPredicate isBoundsPrim
        = [suffix](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        const std::string pathString = primPath.GetString();
        if (pathString.size() < suffix.size()
            || pathString.compare(pathString.size() - suffix.size(), suffix.size(), suffix) != 0) {
            return false;
        }
        return sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->basisCurves;
    };

    const auto boundsPrims = SceneIndexInspector(secondaryPassSceneIndex).FindPrims(isBoundsPrim);
    ASSERT_EQ(static_cast<long>(boundsPrims.size()), expectedCount);
}
