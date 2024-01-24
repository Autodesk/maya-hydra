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

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/tokens.h>

#include <maya/MViewport2Renderer.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

template<typename AttrType>
}

TEST(NurbsPrimitives, nurbsSphere)
{
    // Setup inspector for the first scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Retrieve the sphere prim
    FindPrimPredicate findSpherePredicate
        = [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        if (prim.primType != HdPrimTypeTokens->mesh) {
            return false;
        }
        bool parentIsNurbsSphereShape
            = MakeRelativeToParentPath(primPath.GetParentPath()).GetAsString().find("nurbsSphereShape") != std::string::npos;
        return parentIsNurbsSphereShape;
    };
    PrimEntriesVector foundPrims = inspector.FindPrims(findSpherePredicate, 1);
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim spherePrim = foundPrims.front().prim;
    EXPECT_TRUE(spherePrim.primType != TfToken());
    EXPECT_TRUE(spherePrim.dataSource != nullptr);

    MDagPath sphereDagPath;
    ASSERT_TRUE(GetDagPathFromNodeName("nurbsSphere1", sphereDagPath));
    sphereDagPath.extendToShape();

    MStatus nurbsResult;
    MFnNurbsSurface nurbsSphere(sphereDagPath.node(), &nurbsResult);
}
