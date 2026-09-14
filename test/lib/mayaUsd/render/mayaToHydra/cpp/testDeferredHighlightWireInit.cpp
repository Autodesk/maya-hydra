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
// How:    Python leaves a translated cube selected in a shaded panel, forces "ogs -reset" so the
//         DormantPolyWire reaches the scene index for the first time while the shape is selected
//         (which is what makes isLegacySelectionHighlightWire() skip it), then switches the panel
//         to wireframe so reconsiderSkippedHighlightWires recovers it from a thin delta.
// Expect: the wire prim exists and its xform matches the shape's world matrix.
//
// The delta that revives a skipped wire describes what changed since the last frame, not what a
// brand new adapter needs, so it can legitimately omit MVS_changedMatrix. mayaHydraSceneIndex.cpp
// forces that bit for every new adapter; without it MayaHydraRenderItemAdapter::_transform keeps
// its identity initializer (UpdateTransform is its only writer) and the wire draws at the origin
// instead of on the shape. Removing the flag passes every other test in the suite, which is why
// this one exists.
//
// Deliberately no material assertion. mayaHydraSceneIndex.cpp forces MVS_changedEffect alongside
// MVS_changedMatrix, but that bit is unobservable on a wire: GetMaterialId() short-circuits on
// kLines/kLineStrip and returns the intentionally empty _fallbackMaterial without consulting the
// adapter's stored material, and _GetMaterialBindingDataSource() yields no data source for an
// empty path. A wire prim therefore never carries a material binding, in either mode. The effect
// bit still matters for a mesh adapter first translated from a thin delta, which this scenario
// cannot produce -- only wires are ever skipped.
//
// Runs in both selection-highlight modes. In legacy mode nothing is ever skipped, so the wire is
// created normally and this degenerates into a weaker "the wire is correct" assertion rather than
// failing -- worth keeping as a cheap control.
TEST(DeferredHighlightWireInit, testDeferredTransform)
{
    const auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);

    // Anchor on the cube's mesh prim to locate the MayaHydraSceneIndex. The wire prim itself is a
    // poor anchor: pass filtering routes it to the secondary graphics pass only, so it is not
    // reachable from every terminal scene index. Assert against the Maya scene index rather than a
    // pass scene index -- pass routing is what testLegacyHighlightPassRouting covers, and going
    // through a pass here would let this test fail for two unrelated reasons.
    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(shapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Could not resolve a mesh prim and MayaHydraSceneIndex for " << shapeFull;

    // The wire must exist at all: a regression that skips it permanently shows up here rather than
    // as a wrong matrix. The scene holds a single mesh, hence exactly one such prim.
    PrimEntriesVector wirePrims = SceneIndexInspector(mayaSceneIndex)
        .FindPrims(CreatePrimPredicate("DormantPolyWire", HdPrimTypeTokens->basisCurves));
    ASSERT_EQ(wirePrims.size(), 1u)
        << "Expected exactly one DormantPolyWire basisCurves prim, found " << wirePrims.size()
        << ". Zero means the wire was skipped and never recovered.";

    const HdSceneIndexPrim wirePrim = wirePrims.front().prim;
    ASSERT_TRUE(wirePrim.dataSource);

    // The transform half of the defect: _transform has no writer other than UpdateTransform(), so
    // without MVS_changedMatrix this read back the default-constructed value -- in practice the
    // zero matrix on a fresh heap page, which collapses the wire to a point and reads as "the
    // wireframe is missing" rather than "the wireframe is in the wrong place".
    HdXformSchema xform = HdXformSchema::GetFromParent(wirePrim.dataSource);
    ASSERT_TRUE(xform.GetMatrix()) << "DormantPolyWire prim has no xform matrix data source.";
    const GfMatrix4d actual = xform.GetMatrix()->GetTypedValue(0.0);

    MDagPath shapeDag;
    ASSERT_EQ(GetDagPathFromNodeName(MString(shapeFull.c_str()), shapeDag), MStatus::kSuccess);
    MMatrix expected;
    ASSERT_EQ(GetMayaMatrixFromDagPath(shapeDag, expected), MStatus::kSuccess);

    // Explicit tolerance rather than the DEFAULT_TOLERANCE epsilon: the comparison round-trips a
    // Maya MMatrix through GfMatrix4d, and the defect being caught is a zero or garbage matrix, not
    // a last-bit difference.
    EXPECT_TRUE(MatricesAreClose(actual, expected, 1e-6))
        << "DormantPolyWire transform was not initialized on its deferred first translation: hydra "
        << actual << " vs maya " << expected;
}