
#include "testUtils.h"

#include <mayaHydraLib/hydraUtils.h>

#include <maya/MGlobal.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

TEST(TestUsdAnim, timeVaryingTransform)
{
    const auto& si = GetTerminalSceneIndices();
    ASSERT_GT(si.size(), 0u);
    auto siRoot = si.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path cubePath(Ufe::PathString::path(argv[0]));

    // Translate the cube application path into the cube scene index path using
    // the selection scene index.
    const auto snSi = findSelectionSceneIndexInTree(siRoot);
    ASSERT_TRUE(snSi);

    const auto primSelections = snSi->UfePathToPrimSelections(cubePath);
    ASSERT_EQ(primSelections.size(), 1u);
    const auto cubeSiPath = primSelections.front().primPath;

    ASSERT_FALSE(cubeSiPath.IsEmpty());

    // Get cube scene index prim.
    const auto cubePrim = siRoot->GetPrim(cubeSiPath);
    ASSERT_TRUE(cubePrim.dataSource);

    // Extract the Hydra xform matrix from the cube prim
    // Loop over time, and check translation, from frames [0..10].  The z value
    // of the translation has been set equal to time.
    for (double t=0; t < 11.0; t+=1.0) {
        ASSERT_EQ(MGlobal::viewFrame(t), MS::kSuccess);

        GfMatrix4d cubeHydraMatrix;
        ASSERT_TRUE(GetXformMatrixFromPrim(cubePrim, cubeHydraMatrix));
        ASSERT_TRUE(GfIsClose(cubeHydraMatrix.ExtractTranslation()[2], t, std::numeric_limits<double>::epsilon()));
    }
}
