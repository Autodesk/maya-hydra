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

#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {
const HdDataSourceLocator subdivTagsLocator = HdMeshSchema::GetSubdivisionTagsLocator();
} // namespace

// What: subdivisionTags container of a Catmull-Clark mesh exposes the crease/corner data
// authored on the Maya source mesh.
// How: locate the creasedCubeShape mesh in the terminal scene index tree and dump its
// subdivisionTags container, comparing against a reference.
// Expect: cornerIndices/cornerSharpnesses, creaseIndices/creaseLengths/creaseSharpnesses,
// and the three boundary/face-varying/triangle interpolation rules match the reference.
//
// Note: this test only exercises the MayaHydraMeshAdapter path; it must therefore be driven
// with MAYA_HYDRA_USE_MESH_ADAPTER=1 (see INTERACTIVE_TEST_SCRIPT_FILES_MESH_ADAPTER).
TEST(MeshSubdivTags, creasedCubeTags)
{
#ifndef CONFIGURABLE_DECIMAL_STREAMING_AVAILABLE
    GTEST_SKIP() << "Skipping test, configurable decimal streaming is unavailable.";
#else
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    DecimalStreamingOverride decimalStreamingOverride(
        { PXR_NS::TfDecimalToStringMode::FIXED, 5, false });

    HdSceneIndexPrim cubePrim;
    bool             found = false;
    for (const HdSceneIndexBaseRefPtr& sceneIndex : sceneIndices) {
        SceneIndexInspector inspector(sceneIndex);
        PrimEntriesVector   foundPrims
            = inspector.FindPrims(CreatePrimPredicate("creasedCubeShape", HdPrimTypeTokens->mesh));
        if (foundPrims.size() == 1u) {
            cubePrim = foundPrims.front().prim;
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found) << "creasedCubeShape mesh prim not found in terminal scene indices";
    ASSERT_NE(cubePrim.dataSource, nullptr);
    EXPECT_EQ(cubePrim.primType, HdPrimTypeTokens->mesh);

    const std::filesystem::path subdivRef = getPathToSample("creasedCube_subdivTags_fresh.txt");
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(cubePrim.dataSource, subdivTagsLocator), subdivRef))
        << " See " << getDataSourceComparisonOutputPath(subdivRef).string() << " for actual output";
#endif
}
