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

#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/MDagPath.h>
#include <maya/MMatrix.h>
#include <maya/MString.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

// What:   A selection-highlight wire whose first translation was deferred gets its transform.
// How:    Python selects a translated cube in a shaded panel and forces "ogs -reset", so the
//         DormantPolyWire is first seen while selected and skipped. Switching the panel to
//         wireframe then recovers it through reconsiderSkippedHighlightWires, from a delta that
//         carries only visibility bits.
// Expect: the wire prim exists and its xform matches the shape's world matrix.
//
// mayaHydraSceneIndex.cpp forces MVS_changedMatrix for every new adapter. Without it the recovered
// wire keeps its identity transform and draws at the origin. No other test catches this.
//
// No material assertion: a wire prim never carries a material binding, so the forced
// MVS_changedEffect is not observable here.
//
// In legacy mode nothing is skipped, so this only checks that the wire is correct.
TEST(DeferredHighlightWireInit, testDeferredTransform)
{
    const auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);

    // Locate the MayaHydraSceneIndex through the mesh prim: the wire is only routed to the
    // secondary pass. Query the Maya scene index directly so pass routing is not under test.
    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(shapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Could not resolve a mesh prim and MayaHydraSceneIndex for " << shapeFull;

    // The scene holds a single mesh, hence exactly one wire prim.
    PrimEntriesVector wirePrims = SceneIndexInspector(mayaSceneIndex)
        .FindPrims(CreatePrimPredicate("DormantPolyWire", HdPrimTypeTokens->basisCurves));
    ASSERT_EQ(wirePrims.size(), 1u)
        << "Expected exactly one DormantPolyWire basisCurves prim, found " << wirePrims.size()
        << ". Zero means the wire was skipped and never recovered.";

    const HdSceneIndexPrim wirePrim = wirePrims.front().prim;
    ASSERT_TRUE(wirePrim.dataSource);

    HdXformSchema xform = HdXformSchema::GetFromParent(wirePrim.dataSource);
    ASSERT_TRUE(xform.GetMatrix()) << "DormantPolyWire prim has no xform matrix data source.";
    const GfMatrix4d actual = xform.GetMatrix()->GetTypedValue(0.0);

    MDagPath shapeDag;
    ASSERT_EQ(GetDagPathFromNodeName(MString(shapeFull.c_str()), shapeDag), MStatus::kSuccess);
    MMatrix expected;
    ASSERT_EQ(GetMayaMatrixFromDagPath(shapeDag, expected), MStatus::kSuccess);

    // Loose tolerance: the defect is an identity matrix, not a last-bit difference.
    EXPECT_TRUE(MatricesAreClose(actual, expected, 1e-6))
        << "DormantPolyWire transform was not initialized on its deferred first translation: hydra "
        << actual << " vs maya " << expected;
}