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

#include "testUtils.h"

#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/tokens.h>

#include <maya/MViewport2Renderer.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

bool IsUfeLight(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    bool isUfeLightPrim = primPath.GetElementString().find("ufeLightProxy") < std::string::npos;
    if (!isUfeLightPrim) {
        return false;
    }
    // The shape prim is used to display the light's wireframe and is the only prim we want to allow
    // for UFE lights
    bool isShapePrim = primPath.GetElementString().find("ufeLightProxy") < std::string::npos;
    return !isShapePrim;
}

TEST(MayaUsdUfeItems, skipUsdUfeLights)
{
    // Setup inspector for the first scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), static_cast<size_t>(0));
    SceneIndexInspector inspector(sceneIndices.front());

    // Find UFE lights
    PrimEntriesVector ufeLights = inspector.FindPrims(&IsUfeLight, 1);
    EXPECT_EQ(ufeLights.size(), static_cast<size_t>(0));
}
