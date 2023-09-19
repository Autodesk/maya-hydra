
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
    bool isShapePrim = primPath.GetElementString().find("ufeLightProxyShape") < std::string::npos;
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
