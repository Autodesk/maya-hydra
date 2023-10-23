
#include "testUtils.h"

#include <mayaHydraLib/mayaHydra.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <flowViewport/sceneIndex/fvpMergingSceneIndex.h>

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <maya/MDagPath.h>
#include <maya/MString.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

class SceneIndexDisplayNamePred {
    const std::string _name;
public:
    SceneIndexDisplayNamePred(const std::string& name) : _name(name) {}

    bool operator()(const HdSceneIndexBaseRefPtr& sceneIndex) {
        return sceneIndex->GetDisplayName() == _name;
    }
};

HdSceneIndexBaseRefPtr findSceneIndexInTree(
    const HdSceneIndexBaseRefPtr&                             sceneIndex,
    const std::function<bool(const HdSceneIndexBaseRefPtr&)>& predicate
)
{
    if (predicate(sceneIndex)) {
        return sceneIndex;
    }
    auto filteringSi = TfDynamic_cast<HdFilteringSceneIndexBaseRefPtr>(
        sceneIndex);
    // End recursion at leaf scene indices, which are not filtering scene
    // indices.
    if (!filteringSi) {
        return {};
    }

    auto sceneIndices = filteringSi->GetInputScenes();
    if (!sceneIndices.empty()) {
        for (const auto& childSceneIndex : sceneIndices) {
            if (auto si = findSceneIndexInTree(childSceneIndex, predicate)) {
                return si;
            }
        }
    }
    return {};
}

}

TEST(FlowViewport, mergingSceneIndex)
{
    // The Flow Viewport custom merging scene index is in the scene index tree.
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), static_cast<size_t>(0));

    auto isFvpMergingSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Merging Scene Index");
    auto mergingSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpMergingSceneIndex);
    ASSERT_TRUE(mergingSiBase);

    // The custom merging scene index has the MayaHydraSceneIndex as a child,
    // to produce Maya Dag data.
    auto mergingSi = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(mergingSiBase);
    ASSERT_TRUE(mergingSi);
    
    auto producers = mergingSi->GetInputScenes();
    auto isMayaProducerSceneIndex = SceneIndexDisplayNamePred(
        "MayaHydraSceneIndex");
    auto found = std::find_if(
        producers.begin(), producers.end(), isMayaProducerSceneIndex);

    ASSERT_NE(found, producers.end());
    auto mayaSi = TfDynamic_cast<PXR_NS::MayaHydraSceneIndexRefPtr>(*found);
    ASSERT_TRUE(mayaSi);

    // The Flow Viewport merging scene index supports the Flow Viewport path
    // interface, and forwards the call to the Maya scene index, which will
    // translate a Maya path into a scene index SdfPath.

    // Get the path to the sphere defined in the Python driver for this test.
    // We know this is a single-segment UFE path, and that its tail is
    // "aSphere".
    auto mayaPath = Ufe::PathString::path("|aSphere");
    ASSERT_EQ(mayaPath.nbSegments(), 1u);
    ASSERT_EQ(mayaPath.back().string(), "aSphere");

    // The Maya data producer scene index supports the path interface.  Ask it
    // to translate the application path into a scene index path.
    auto sceneIndexPath = mayaSi->SceneIndexPath(mayaPath);

    // Regardless of prefix, the scene index path tail component will match the
    // Maya node name
    ASSERT_EQ(sceneIndexPath.GetName(), mayaPath.back().string());

    // If we ask the terminal scene index for a prim at that path, there must be
    // one.  Prims that exist have a non-null data source.
    SdfPath nonExistentPrimPath("/foo/bar");
    auto nonExistentPrim = sceneIndices.front()->GetPrim(nonExistentPrimPath);
    ASSERT_FALSE(nonExistentPrim.dataSource);
    
    auto spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    ASSERT_TRUE(spherePrim.dataSource);

    // Flow Viewport merging scene index must give the same scene index path
    // answer as the Maya data producer scene index.
    ASSERT_EQ(mergingSi->SceneIndexPath(mayaPath), sceneIndexPath);
}

TEST(FlowViewport, mergingSceneIndexAddRemove)
{
    // Same setup as mergingSceneIndex test.
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto isFvpMergingSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Merging Scene Index");
    auto mergingSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpMergingSceneIndex);
    auto mergingSi = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(mergingSiBase);
    auto producers = mergingSi->GetInputScenes();
    auto isMayaProducerSceneIndex = SceneIndexDisplayNamePred(
        "MayaHydraSceneIndex");
    auto found = std::find_if(
        producers.begin(), producers.end(), isMayaProducerSceneIndex);
    auto mayaSi = TfDynamic_cast<PXR_NS::MayaHydraSceneIndexRefPtr>(*found);
    auto mayaPath = Ufe::PathString::path("|aSphere");
    auto sceneIndexPath = mayaSi->SceneIndexPath(mayaPath);

    // Pending resolution of HYDRA-606, determining validity of Maya scene
    // index prims by inspecting the data source is not reliable.  Transform
    // Maya scene index prims are created without a data source, which would
    // make them invalid.  However, the Flow Viewport selection scene index
    // transforms an invalid scene index path into a valid one if the invalid
    // scene index path corresponds to a selection, so any scene index after
    // the selection scene index (e.g. the terminal scene index) will see a
    // non-null data source.  When this is resolved, all data source-based
    // tests below should be uncommented.  PPT, 6-Oct-2023.

    // With Maya scene index in the merging scene index, the sphere prim has
    // a valid scene index path.
    // auto spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    // ASSERT_TRUE(spherePrim.dataSource);

    // Remove the Maya scene index from the Flow Viewport merging scene index.
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 1u);
    mergingSi->RemoveInputScene(mayaSi);
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 0u);

    // Without the Maya scene index in the merging scene index, the sphere prim
    // is no longer in the Hydra scene index scene.
    // spherePrim = mergingSi->GetPrim(sceneIndexPath);
    // EXPECT_FALSE(spherePrim.dataSource);

    // spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    // EXPECT_FALSE(spherePrim.dataSource);

    // Add the Maya scene index back to the Flow Viewport merging scene index,
    // sphere prim will reappear.  We know that the Maya scene index is added
    // with the absolute root path as scene root, so duplicate that here.
    mergingSi->AddInputScene(mayaSi, SdfPath::AbsoluteRootPath());
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 1u);
    // spherePrim = mergingSi->GetPrim(sceneIndexPath);
    // EXPECT_TRUE(spherePrim.dataSource);
}
