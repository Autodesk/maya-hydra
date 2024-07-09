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

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <maya/M3dView.h>

#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

bool isPrimSelectedPredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    auto selectionsSchema = HdSelectionsSchema::GetFromParent(sceneIndex->GetPrim(primPath).dataSource);
    if (!selectionsSchema.IsDefined()) {
        return false;
    }
    for (size_t iSelection = 0; iSelection < selectionsSchema.GetNumElements(); iSelection++) {
        if (selectionsSchema.GetElement(iSelection).GetFullySelected()) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST(TestSinglePicking, singlePick)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Preconditions
    ASSERT_TRUE(Ufe::GlobalSelection::get()->empty());
    ASSERT_TRUE(inspector.FindPrims(isPrimSelectedPredicate).empty());

    // Picking
    M3dView active3dView = M3dView::active3dView();
    QPoint centerMouseCoords(active3dView.portWidth() / 2, active3dView.portHeight() / 2);
    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), centerMouseCoords);
    active3dView.refresh();

    // Postconditions
    ASSERT_EQ(Ufe::GlobalSelection::get()->size(), 1u);
    std::cout << Ufe::GlobalSelection::get()->front()->path().string() << std::endl;
    ASSERT_EQ(inspector.FindPrims(isPrimSelectedPredicate).size(), 1u);
}
