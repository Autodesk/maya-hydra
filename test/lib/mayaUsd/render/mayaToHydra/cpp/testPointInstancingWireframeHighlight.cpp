
#include "testUtils.h"

#include <mayaHydraLib/mayaHydra.h>

#include <flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.h>
#include <flowViewport/sceneIndex/fvpMergingSceneIndex.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hdx/selectionSceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/utils.h>

#include "ufe/pathSegment.h"
#include "ufe/sceneItem.h"
#include "ufe/selection.h"
#include <ufe/pathString.h>
#include <ufe/hierarchy.h>
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

const std::string selectionHighlightTag = "_SelectionHighlight";

const std::string stagePathSegment = "|NestedAndComposedPointInstancers|NestedAndComposedPointInstancersShape";

SdfPath getSelectionHighlightMirrorPathFromOriginal(const SdfPath& originalPath)
{
    return originalPath.ReplaceName(TfToken(originalPath.GetName() + selectionHighlightTag));
}

// SdfPath getOriginalPathFromSelectionHighlightMirror(const SdfPath& mirrorPath)
// {
//     const std::string primName = mirrorPath.GetName();
//     return mirrorPath.ReplaceName(TfToken(primName.substr(0, primName.size() - selectionHighlightTag.size())));
// }

TfToken getRefinedReprToken(const HdSceneIndexPrim& prim)
{
    HdLegacyDisplayStyleSchema displayStyle = HdLegacyDisplayStyleSchema::GetFromParent(prim.dataSource);
    if (!displayStyle.IsDefined() || !displayStyle.GetReprSelector()) {
        return {};
    }
    auto reprSelectors = displayStyle.GetReprSelector()->GetTypedValue(0);
    EXPECT_EQ(reprSelectors.size(), 3u); // refined, unrefined, points
    return reprSelectors.empty() ? TfToken() : reprSelectors.front();
}

VtArray<SdfPath> getPrototypeRoots(const HdSceneIndexPrim& prim)
{
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return instancedBy.IsDefined() && instancedBy.GetPrototypeRoots() 
        ? instancedBy.GetPrototypeRoots()->GetTypedValue(0) 
        : VtArray<SdfPath>({SdfPath::AbsoluteRootPath()});
}

void assertSelectionHighlightCorrectness(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath)
{
    HdSceneIndexPrimView view(sceneIndex, primPath);
    for (auto it = view.begin(); it != view.end(); ++it) {
        const SdfPath& currPath = *it;
        HdSceneIndexPrim currPrim = sceneIndex->GetPrim(currPath);

        VtArray<SdfPath> prototypeRoots = getPrototypeRoots(currPrim);
        bool isInSamePrototypeHierarchy = std::find_if(prototypeRoots.begin(), prototypeRoots.end(), [primPath](const auto& prototypeRoot) -> bool {
            return primPath.HasPrefix(prototypeRoot);
        }) != prototypeRoots.end();
        if (!isInSamePrototypeHierarchy) {
            it.SkipDescendants();
            continue;
        }

        HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(currPrim.dataSource);
        if (instancerTopology.IsDefined()) {
            ASSERT_NE(instancerTopology.GetPrototypes(), nullptr);
            auto prototypePaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
            ASSERT_GE(prototypePaths.size(), 1u);
            for (const auto& prototypePath : prototypePaths) {
                auto prototypeName = prototypePath.GetElementString();
                ASSERT_EQ(prototypeName.substr(prototypeName.size() - selectionHighlightTag.size()), selectionHighlightTag);
                assertSelectionHighlightCorrectness(sceneIndex, prototypePath);
            }
            it.SkipDescendants();
            continue;
        }

        if (currPrim.primType == HdPrimTypeTokens->mesh) {
            EXPECT_EQ(getRefinedReprToken(currPrim), HdReprTokens->refinedWire);
        }
    }
}

bool findSelectionHighlightMirrorsPredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    return primPath.GetElementString().find(selectionHighlightTag) != std::string::npos;
}

bool findMeshPrimsPredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    return sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
}

} // namespace

TEST(PointInstancingWireframeHighlight, pointInstancer)
{
    const SceneIndicesVector& terminalSceneIndices = GetTerminalSceneIndices();
    ASSERT_FALSE(terminalSceneIndices.empty());
    SceneIndexInspector inspector(terminalSceneIndices.front());

    auto isFvpMergingSceneIndexPredicate = SceneIndexDisplayNamePred("Flow Viewport Merging Scene Index");
    auto fvpMergingSceneIndex = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(
        findSceneIndexInTree(terminalSceneIndices.front(), isFvpMergingSceneIndexPredicate));

    auto ufeSelection = Ufe::GlobalSelection::get();

    HdxSelectionSceneIndexObserver selectionObserver;
    selectionObserver.SetSceneIndex(terminalSceneIndices.front());

    auto topInstancerPath = Ufe::PathString::path(stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer");
    auto secondInstancerPath = Ufe::PathString::path(stagePathSegment + "," + "/Root/SecondInstancer");
    auto thirdInstancerPath = Ufe::PathString::path(stagePathSegment + "," + "/Root/ThirdInstancer");
    auto fourthInstancerPath = Ufe::PathString::path(stagePathSegment + "," + "/Root/FourthInstancer");

    auto topInstancerItem = Ufe::Hierarchy::createItem(topInstancerPath);
    auto secondInstancerItem = Ufe::Hierarchy::createItem(secondInstancerPath);
    auto thirdInstancerItem = Ufe::Hierarchy::createItem(thirdInstancerPath);
    auto fourthInstancerItem = Ufe::Hierarchy::createItem(fourthInstancerPath);

    // Initial state : ensure nothing is highlighted
    ufeSelection->clear();

    auto selectionHighlightMirrors = inspector.FindPrims(findSelectionHighlightMirrorsPredicate);
    EXPECT_TRUE(selectionHighlightMirrors.empty());

    auto meshPrims = inspector.FindPrims(findMeshPrimsPredicate);
    for (const auto& meshPrim : meshPrims) {
        HdLegacyDisplayStyleSchema displayStyle = HdLegacyDisplayStyleSchema::GetFromParent(meshPrim.prim.dataSource);
        EXPECT_TRUE(displayStyle.IsDefined());
        EXPECT_FALSE(displayStyle.GetReprSelector());
    }

    // Select point instancers directly
    auto testInstancerDirectHighlightFn = [&](const Ufe::SceneItem::Ptr& instancerItem, const Ufe::Path& instancerPath) -> void {
        ufeSelection->replaceWith(instancerItem);

        auto instancerHydraSelections = fvpMergingSceneIndex->ConvertUfeSelectionToHydra(instancerPath);
        ASSERT_EQ(instancerHydraSelections.size(), 1u);
        auto instancerPrimPath = instancerHydraSelections.front().primPath;
    
        ASSERT_EQ(selectionObserver.GetSelection()->GetAllSelectedPrimPaths().size(), 1u);
        ASSERT_EQ(selectionObserver.GetSelection()->GetAllSelectedPrimPaths().front(), instancerPrimPath);

        ASSERT_FALSE(inspector.FindPrims(findMeshPrimsPredicate).empty());
        assertSelectionHighlightCorrectness(inspector.GetSceneIndex(), getSelectionHighlightMirrorPathFromOriginal(instancerPrimPath));
    };
    
    testInstancerDirectHighlightFn(topInstancerItem, topInstancerPath);
    testInstancerDirectHighlightFn(secondInstancerItem, secondInstancerPath);
    testInstancerDirectHighlightFn(thirdInstancerItem, thirdInstancerPath);
    testInstancerDirectHighlightFn(fourthInstancerItem, fourthInstancerPath);
    
    // Select point instancer ancestors
    auto testInstancerIndirectHighlightFn = [&](const Ufe::SceneItem::Ptr& instancerItem, const Ufe::Path& instancerPath) -> void {
        // This is not an actual selection, we use it to get the Hydra path
        auto instancerHydraSelections = fvpMergingSceneIndex->ConvertUfeSelectionToHydra(instancerPath);
        ASSERT_EQ(instancerHydraSelections.size(), 1u);
    
        ASSERT_FALSE(inspector.FindPrims(findMeshPrimsPredicate).empty());
        assertSelectionHighlightCorrectness(inspector.GetSceneIndex(), getSelectionHighlightMirrorPathFromOriginal(instancerHydraSelections.front().primPath));
    };
    auto testInstancerNoHighlightFn = [&](const Ufe::SceneItem::Ptr& instancerItem, const Ufe::Path& instancerPath) -> void {
        // This is not an actual selection, we use it to get the Hydra path
        auto instancerHydraSelections = fvpMergingSceneIndex->ConvertUfeSelectionToHydra(instancerPath);
        ASSERT_EQ(instancerHydraSelections.size(), 1u);
    
        HdSceneIndexPrim selectionHighlightMirrorPrim = inspector.GetSceneIndex()->GetPrim(
            getSelectionHighlightMirrorPathFromOriginal(instancerHydraSelections.front().primPath));
        ASSERT_EQ(selectionHighlightMirrorPrim.primType, TfToken());
    };

    // Select TopInstancer's parent : only TopInstancer should be highlighted
    auto topInstancerParentPath = Ufe::PathString::path(stagePathSegment + "," + "/Root/TopInstancerXform");
    auto topInstancerParentItem = Ufe::Hierarchy::createItem(topInstancerParentPath);
    ufeSelection->replaceWith(topInstancerParentItem);
    testInstancerIndirectHighlightFn(topInstancerItem, topInstancerPath);
    testInstancerNoHighlightFn(secondInstancerItem, secondInstancerPath);
    testInstancerNoHighlightFn(thirdInstancerItem, thirdInstancerPath);
    testInstancerNoHighlightFn(fourthInstancerItem, fourthInstancerPath);

    // Select Root : all instancers should be highlighted
    auto rootPath = Ufe::PathString::path(stagePathSegment + "," + "/Root");
    auto rootItem = Ufe::Hierarchy::createItem(rootPath);
    ufeSelection->replaceWith(rootItem);
    testInstancerIndirectHighlightFn(topInstancerItem, topInstancerPath);
    testInstancerIndirectHighlightFn(secondInstancerItem, secondInstancerPath);
    testInstancerIndirectHighlightFn(thirdInstancerItem, thirdInstancerPath);
    testInstancerIndirectHighlightFn(fourthInstancerItem, fourthInstancerPath);
}

TEST(PointInstancingWireframeHighlight, instance)
{
    const SceneIndicesVector& terminalSceneIndices = GetTerminalSceneIndices();
    ASSERT_FALSE(terminalSceneIndices.empty());
    SceneIndexInspector inspector(terminalSceneIndices.front());

    auto isFvpMergingSceneIndexPredicate = SceneIndexDisplayNamePred("Flow Viewport Merging Scene Index");
    auto fvpMergingSceneIndex = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(
        findSceneIndexInTree(terminalSceneIndices.front(), isFvpMergingSceneIndexPredicate));

    auto ufeSelection = Ufe::GlobalSelection::get();

    HdxSelectionSceneIndexObserver selectionObserver;
    selectionObserver.SetSceneIndex(terminalSceneIndices.front());

    auto topInstancerFirstInstancePath = Ufe::PathString::path(stagePathSegment + "," + "/Root/TopInstancerXform/TopInstancer/0");
    auto secondInstancerSecondInstancePath = Ufe::PathString::path(stagePathSegment + "," + "/Root/SecondInstancer/1");

    auto topInstancerFirstInstanceItem = Ufe::Hierarchy::createItem(topInstancerFirstInstancePath);
    auto secondInstancerSecondInstanceItem = Ufe::Hierarchy::createItem(secondInstancerSecondInstancePath);

    // Initial state : ensure nothing is highlighted
    ufeSelection->clear();

    auto selectionHighlightMirrors = inspector.FindPrims(findSelectionHighlightMirrorsPredicate);
    EXPECT_TRUE(selectionHighlightMirrors.empty());

    auto meshPrims = inspector.FindPrims(findMeshPrimsPredicate);
    for (const auto& meshPrim : meshPrims) {
        HdLegacyDisplayStyleSchema displayStyle = HdLegacyDisplayStyleSchema::GetFromParent(meshPrim.prim.dataSource);
        EXPECT_TRUE(displayStyle.IsDefined());
        EXPECT_FALSE(displayStyle.GetReprSelector());
    }

    // Select instances
    auto testInstanceHighlightFn = [&](const Ufe::SceneItem::Ptr& instanceItem, const Ufe::Path& instancePath) -> void {
        ufeSelection->replaceWith(instanceItem);

        auto instanceHydraSelections = fvpMergingSceneIndex->ConvertUfeSelectionToHydra(instancePath);
        ASSERT_EQ(instanceHydraSelections.size(), 1u);
        auto instancerPrimPath = instanceHydraSelections.front().primPath;

        ASSERT_EQ(selectionObserver.GetSelection()->GetAllSelectedPrimPaths().size(), 1u);
        ASSERT_EQ(selectionObserver.GetSelection()->GetAllSelectedPrimPaths().front(), instancerPrimPath);

        ASSERT_FALSE(inspector.FindPrims(findMeshPrimsPredicate).empty());
        assertSelectionHighlightCorrectness(inspector.GetSceneIndex(), getSelectionHighlightMirrorPathFromOriginal(instancerPrimPath));

        HdSceneIndexPrim instancerHighlightPrim = inspector.GetSceneIndex()->GetPrim(getSelectionHighlightMirrorPathFromOriginal(instancerPrimPath));
        HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(instancerHighlightPrim.dataSource);
        ASSERT_TRUE(instancerTopology.IsDefined());
        ASSERT_NE(instancerTopology.GetMask(), nullptr);
        auto mask = instancerTopology.GetMask()->GetTypedValue(0);
        ASSERT_FALSE(mask.empty());

        auto selectedInstanceIndex = std::stoi(instancePath.getSegments().back().components().back().string());
        for (size_t iMask = 0; iMask < mask.size(); iMask++) {
            EXPECT_EQ(mask[iMask], iMask == selectedInstanceIndex);
        }
    };
    
    testInstanceHighlightFn(topInstancerFirstInstanceItem, topInstancerFirstInstancePath);
    testInstanceHighlightFn(secondInstancerSecondInstanceItem, secondInstancerSecondInstancePath);
}
