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

#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/sceneIndexPrimView.h>

#include <maya/MGlobal.h>
#include <maya/MString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(MayaHydraSceneIndex, PrimAncestors)
{   
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    
    // Find the MayaHydraSceneIndex in the scene index tree
    auto mayaSceneIndex = TfDynamic_cast<MayaHydraSceneIndexRefPtr>(sceneIndices.front());
    if (!mayaSceneIndex) {
        // Try to find it in the input scene indices if not at the root
        auto isDataProducerMergingSceneIndex = SceneIndexDisplayNamePred("Data Producer Merging Scene Index");
        auto mergingSiBase = findSceneIndexInTree(sceneIndices.front(), isDataProducerMergingSceneIndex);
        
        if (mergingSiBase) {
            auto mergingSi = TfDynamic_cast<HdMergingSceneIndexRefPtr>(mergingSiBase);
            if (mergingSi) {
                auto producers = mergingSi->GetInputScenes();
                auto isMayaProducerSceneIndex = SceneIndexDisplayNamePred("MayaHydraSceneIndex");
                auto found = std::find_if(producers.begin(), producers.end(), isMayaProducerSceneIndex);
                if (found != producers.end()) {
                    mayaSceneIndex = TfDynamic_cast<MayaHydraSceneIndexRefPtr>(*found);
                }
            }
        }
    }
    
    ASSERT_TRUE(mayaSceneIndex) << "Could not find MayaHydraSceneIndex in scene index tree";
    
    // Setup of group hierarchy done in the Python driver
    // Test 1: _AddPrimAncestors, check ancestor creation
    SdfPath leafPrimPath;

    for (const auto& primPath : HdSceneIndexPrimView(mayaSceneIndex, SdfPath::AbsoluteRootPath())) {
        if (primPath.GetName() == "leafShape") {
            leafPrimPath = primPath;
            break;
        }
    }
    
    ASSERT_FALSE(leafPrimPath.IsEmpty()) << "Could not find the leaf prim in the scene index";
    
    SdfPath currentPath = leafPrimPath;
    while (!currentPath.IsAbsoluteRootPath() && currentPath != mayaSceneIndex->GetRprimPath()) {
        auto prim = mayaSceneIndex->GetPrim(currentPath);
        ASSERT_TRUE(prim.dataSource) << "Ancestor prim should exist at path: " << currentPath;
        currentPath = currentPath.GetParentPath();
    }
    
    // Test 2: _RemoveEmptyAncestors, check ancestor removal
    MStatus status = MGlobal::executeCommand("delete leafShape");
    ASSERT_EQ(status, MS::kSuccess);
    
    ASSERT_EQ(MGlobal::executeCommand("refresh"), MS::kSuccess);
    
    auto leafPrim = mayaSceneIndex->GetPrim(leafPrimPath);
    ASSERT_FALSE(leafPrim.dataSource) << "Leaf prim should be removed after deletion";
    
    SdfPath parentPath = leafPrimPath.GetParentPath();
    while (!parentPath.IsAbsoluteRootPath() && parentPath != mayaSceneIndex->GetRprimPath()) {
        auto parentPrim = mayaSceneIndex->GetPrim(parentPath);
        // Remaining ancestors should still have children
        if (parentPrim.dataSource) { 
            auto children = mayaSceneIndex->GetChildPrimPaths(parentPath);
            ASSERT_GT(children.size(), 0u) << "Parent prim with no children should be cleaned up at path: " << parentPath;
        }
        parentPath = parentPath.GetParentPath();
    }
    
    // Cleanup
    ASSERT_EQ(MGlobal::executeCommand("delete group1"), MS::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("refresh"), MS::kSuccess);
} 