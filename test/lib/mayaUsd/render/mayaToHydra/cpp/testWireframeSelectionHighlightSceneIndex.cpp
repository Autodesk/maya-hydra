
#include "testUtils.h"

#include <mayaHydraLib/mayaHydra.h>

#include <flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.h>
#include <flowViewport/sceneIndex/fvpMergingSceneIndex.h>

#include <pxr/imaging/hdx/selectionSceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>

#include <ufe/pathString.h>
#include <ufe/hierarchy.h>
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

/// \class WfShSiObserver
///
/// Wireframe selection highlight scene index observer to observe dirty
/// notifications on prims.
///
class WfShSiObserver : public HdSceneIndexObserver
{
public:
    WfShSiObserver(const HdSceneIndexBaseRefPtr& sceneIndex)
        : _sceneIndex(sceneIndex)
    {
        _sceneIndex->AddObserver(HdSceneIndexObserverPtr(this));
    }

    void PrimsDirtied(
        const HdSceneIndexBase&   sender,
        const DirtiedPrimEntries& entries
    ) override
    {
        for (const auto& entry : entries) {
            if (entry.dirtyLocators.Contains(Fvp::WireframeSelectionHighlightSceneIndex::ReprSelectorLocator())) {
                _dirtiedPrims.insert(entry.primPath);
            }
        }
    }

    const SdfPathSet& DirtiedPrims() const { return _dirtiedPrims; }
    void ClearDirtiedPrims() { _dirtiedPrims.clear(); }

    // Don't care about other dirty notifications.
    void PrimsAdded(const HdSceneIndexBase&, const AddedPrimEntries&) override
    {}
    void PrimsRemoved(
        const HdSceneIndexBase&, const RemovedPrimEntries&
    ) override {}
    void PrimsRenamed(
        const HdSceneIndexBase&, const RenamedPrimEntries&
    ) override {}

private:

    HdSceneIndexBaseRefPtr _sceneIndex;

    SdfPathSet             _dirtiedPrims;
};

bool hasSelectionHighlight(const HdSceneIndexPrim& prim)
{
    if (!prim.dataSource) {
        return false;
    }

    auto taDs = HdTypedSampledDataSource<VtArray<TfToken>>::Cast(
        HdContainerDataSource::Get(
            prim.dataSource, 
            Fvp::WireframeSelectionHighlightSceneIndex::ReprSelectorLocator()
        ));
    if (!taDs) {
        return false;
    }
    
    static VtArray<TfToken> expected({HdReprTokens->refinedWireOnSurf, TfToken(), TfToken()});

    return taDs->GetValue(0) == expected;
}

}

TEST(FlowViewport, wireframeSelectionHighlightSceneIndex)
{
    // The Flow Viewport wireframe selection highlight scene index is in the
    // scene index tree.
    const auto& si = GetTerminalSceneIndices();
    ASSERT_GT(si.size(), 0u);

    auto isFvpWireframeSelectionHighlightSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Wireframe Selection Highlight Scene Index");
    auto wireframeSiBase = findSceneIndexInTree(
        si.front(), isFvpWireframeSelectionHighlightSceneIndex);
    ASSERT_TRUE(wireframeSiBase);
}

TEST(FlowViewport, wireframeSelectionHighlightSceneIndexDirty)
{
    // Python scene setup should have created the following scene:
    //
    // |sphereAndCube
    //  |sphereAndCubeShape
    //   /sphereAndCubeParent
    //    /sphere
    //    /cube
    // |cylinderAndCone
    //  |cylinderAndConeShape
    //   /cylinderAndConeParent
    //    /cylinder
    //    /cone

    auto scParentPath = Ufe::PathString::path(
        "|sphereAndCube|sphereAndCubeShape,/sphereAndCubeParent");
    auto ccParentPath = Ufe::PathString::path(
        "|cylinderAndCone|cylinderAndConeShape,/cylinderAndConeParent");

    auto scItem = Ufe::Hierarchy::createItem(scParentPath);
    auto ccItem = Ufe::Hierarchy::createItem(ccParentPath);
    auto scHierarchy = Ufe::Hierarchy::hierarchy(scItem);
    auto ccHierarchy = Ufe::Hierarchy::hierarchy(ccItem);

    ASSERT_EQ(scHierarchy->children().size(), 2u);
    ASSERT_EQ(ccHierarchy->children().size(), 2u);

    const auto& si = GetTerminalSceneIndices();
    auto isFvpMergingSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Merging Scene Index");
    auto mergingSi = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(
        findSceneIndexInTree(si.front(), isFvpMergingSceneIndex));

    // See testSelectionSceneIndex.cpp for selection scene index observer
    // comments.
    HdxSelectionSceneIndexObserver ssio;
    ssio.SetSceneIndex(si.front());
    WfShSiObserver wfshsio(si.front());

    // Maya selection API doesn't understand USD data, which can only be
    // represented through UFE, so use UFE API to modify Maya selection.
    auto sn = Ufe::GlobalSelection::get();
    sn->clear();

    // Nothing is selected, no wireframe selection highlight repr locator is
    // dirty.
    auto hdSn = ssio.GetSelection();
    const auto& shDirtiedPrims = wfshsio.DirtiedPrims();
    ASSERT_TRUE(hdSn->GetAllSelectedPrimPaths().empty());
    ASSERT_EQ(shDirtiedPrims.size(), 0u);

    // Select the sphere.
    auto spherePath = scParentPath + "sphere";
    auto sphereItem = Ufe::Hierarchy::createItem(spherePath);
    sn->append(sphereItem);

    // Find the sphere in the Hydra scene index scene.
    auto sphereSelections = mergingSi->ConvertUfePathToHydraSelections(spherePath);
    ASSERT_EQ(sphereSelections.size(), 1u);
    auto sphereSiPath = sphereSelections.front().primPath;
    auto cubeSelections = mergingSi->ConvertUfePathToHydraSelections(scParentPath + "cube");
    ASSERT_EQ(cubeSelections.size(), 1u);
    auto cubeSiPath = cubeSelections.front().primPath;

    // Sphere is selected.
    hdSn = ssio.GetSelection();
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths().size(), 1u);
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths()[0], sphereSiPath);

    // Sphere is a mesh, so its repr selector locator will be marked dirty.
    ASSERT_EQ(shDirtiedPrims.size(), 1u);
    ASSERT_NE(shDirtiedPrims.find(sphereSiPath), shDirtiedPrims.end());

    // Pull on prim, sphere repr selector has been set by wireframe selection
    // highlighting.
    auto spherePrim = si.front()->GetPrim(sphereSiPath);
    ASSERT_TRUE(hasSelectionHighlight(spherePrim));

    // Cube is not selected and thus has no highlighting.
    auto cubePrim = si.front()->GetPrim(cubeSiPath);
    ASSERT_FALSE(hasSelectionHighlight(cubePrim));

    wfshsio.ClearDirtiedPrims();
    ASSERT_EQ(shDirtiedPrims.size(), 0u);

    // Select the cone and cylinder parent.
    Ufe::Selection newSn;
    newSn.append(ccItem);

    sn->replaceWith(newSn);

    auto ccSelections = mergingSi->ConvertUfePathToHydraSelections(ccParentPath);
    ASSERT_EQ(ccSelections.size(), 1u);
    auto ccSiPath = ccSelections.front().primPath;
    auto coneSelections = mergingSi->ConvertUfePathToHydraSelections(ccParentPath + "cone");
    ASSERT_EQ(coneSelections.size(), 1u);
    auto coneSiPath = coneSelections.front().primPath;
    auto cylinderSelections = mergingSi->ConvertUfePathToHydraSelections(ccParentPath + "cylinder");
    ASSERT_EQ(cylinderSelections.size(), 1u);
    auto cylinderSiPath = cylinderSelections.front().primPath;

    // Cone and cylinder parent is selected.
    // Cone is not selected.
    // Cylinder is not selected.
    // Sphere is not selected.
    hdSn = ssio.GetSelection();
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths().size(), 1u);
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths()[0], ccSiPath);

    // Sphere repr selector locator is dirty.
    // Cube repr selector locator is NOT dirty.
    // Cone and cylinder parent repr selector locator is dirty.
    // Cone repr selector locator is dirty.
    // Cylinder repr selector locator is dirty.
    ASSERT_NE(shDirtiedPrims.find(sphereSiPath), shDirtiedPrims.end());
    ASSERT_EQ(shDirtiedPrims.find(cubeSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(ccSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(coneSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(cylinderSiPath), shDirtiedPrims.end());

    wfshsio.ClearDirtiedPrims();

    // Cone and cylinder parent is selected but has no highlight repr, as it is
    // not a mesh.  Cone and cylinder have selection lighlight repr.
    ASSERT_FALSE(hasSelectionHighlight(si.front()->GetPrim(ccSiPath)));
    ASSERT_TRUE(hasSelectionHighlight(si.front()->GetPrim(coneSiPath)));
    ASSERT_TRUE(hasSelectionHighlight(si.front()->GetPrim(cylinderSiPath)));

    // Clear selection.
    sn->clear();
    hdSn = ssio.GetSelection();
    ASSERT_TRUE(hdSn->GetAllSelectedPrimPaths().empty());

    // Sphere and cube repr selector locators are NOT dirty, as these were not
    // in the selection, nor did they have a selected ancestor.
    // Cone and cylinder parent repr selector locator is dirty.
    // Cone and cylinder repr selector locators are dirty, as they had a
    // selected ancestor.
    ASSERT_EQ(shDirtiedPrims.find(sphereSiPath), shDirtiedPrims.end());
    ASSERT_EQ(shDirtiedPrims.find(cubeSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(ccSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(coneSiPath), shDirtiedPrims.end());
    ASSERT_NE(shDirtiedPrims.find(cylinderSiPath), shDirtiedPrims.end());

    // Selection cleared: no more selection highlighting.
    ASSERT_FALSE(hasSelectionHighlight(si.front()->GetPrim(ccSiPath)));
    ASSERT_FALSE(hasSelectionHighlight(si.front()->GetPrim(coneSiPath)));
    ASSERT_FALSE(hasSelectionHighlight(si.front()->GetPrim(cylinderSiPath)));
}
