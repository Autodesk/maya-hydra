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

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

FindPrimPredicate getCameraPrimPredicate(const std::string& cameraName, const TfToken& primType)
{
    return [cameraName,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetAsString().find(cameraName) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == primType;
    };
}

void VerifyCameraPrims(const std::string& prefix, size_t primCount)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector cameraPrims
        = inspector.FindPrims(getCameraPrimPredicate(prefix, HdPrimTypeTokens->camera));
    ASSERT_EQ(cameraPrims.size(), primCount);
    auto testCameraPrims = [cameraPrims]() -> void {
        for (PrimEntry cameraPrim : cameraPrims) {
            EXPECT_EQ(cameraPrim.prim.primType, HdPrimTypeTokens->camera);
            ASSERT_NE(cameraPrim.prim.dataSource, nullptr);
        }
    };

    testCameraPrims();
}

TEST(Camera, defaultCameras)
{ 
    VerifyCameraPrims("sprims", 4u);
}

TEST(Camera, addMayaCamera)
{ 
    VerifyCameraPrims("sprims", 5u);
}

TEST(Camera, removeMayaCamera)
{ 
    VerifyCameraPrims("sprims", 4u);
}
}
