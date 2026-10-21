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

#include <flowViewport/tokens.h>

#include <pxr/base/tf/stringUtils.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

DEFINE_PRIVATE_OVERRIDEWIREFRAMECOLOR_TOKEN

namespace {

// Node names created by testFootPrintNode.py.
const std::string kFootPrintShapeName = "footPrintNotifShape";
const std::string kCubeName = "footPrintNotifCube";

const HdDataSourceLocator kDisplayColorLocator
    = HdPrimvarsSchema::GetDefaultLocator().Append(HdTokens->displayColor);

// Footprint prims live under /MhFootPrint_<ptr>. In legacy selection highlighting, the highlight
// mirror prims live under /FlowViewportSelectionHighlights and are not footprint prims.
bool IsFootPrintPath(const SdfPath& path)
{
    const SdfPathVector prefixes = path.GetPrefixes();
    return !prefixes.empty() && TfStringStartsWith(prefixes.front().GetName(), "MhFootPrint_");
}

bool IsFootPrintMeshPath(const SdfPath& path)
{
    const std::string& name = path.GetName();
    return IsFootPrintPath(path)
        && (TfStringStartsWith(name, "sole_") || TfStringStartsWith(name, "heel_"));
}

// Each frame pass has its own terminal scene index. Pass filtering keeps the paths of the prims
// it filters out but returns them with an empty type and drops their dirty notifications, so
// pick the terminal scene index where the footprint is actually a mesh.
HdSceneIndexBaseRefPtr FindFootPrintTerminalSceneIndex()
{
    return FindTerminalSceneIndexWithPrim(
        GetTerminalSceneIndices(),
        [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
            return IsFootPrintMeshPath(primPath)
                && sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
        });
}

// The sole and heel mesh prims of the single footprint node in the scene.
SdfPathVector GetFootPrintMeshPaths(const HdSceneIndexBaseRefPtr& sceneIndex)
{
    SdfPathVector meshPaths;
    for (const auto& rootChild : sceneIndex->GetChildPrimPaths(SdfPath::AbsoluteRootPath())) {
        if (!IsFootPrintPath(rootChild)) {
            continue;
        }
        for (const auto& child : sceneIndex->GetChildPrimPaths(rootChild)) {
            if (IsFootPrintMeshPath(child)) {
                meshPaths.push_back(child);
            }
        }
    }
    return meshPaths;
}

void ExecuteAndRefresh(const std::string& melCommand)
{
    EXPECT_EQ(MGlobal::executeCommand(melCommand.c_str()), MS::kSuccess) << melCommand;
    MGlobal::executeCommand("refresh -force");
}

size_t CountDirties(
    const SceneIndexNotificationsAccumulator& accumulator,
    const SdfPath&                            primPath,
    const HdDataSourceLocator&                locator)
{
    size_t count = 0;
    for (const auto& entry : accumulator.GetDirtiedPrimEntries()) {
        if (entry.primPath == primPath && entry.dirtyLocators.Contains(locator)) {
            ++count;
        }
    }
    return count;
}

} // namespace

// In shaded mode the original prims do not read a selection color: the highlight is drawn by
// the highlight mirror prims in legacy selection highlighting, or by the outline in outline
// selection highlighting. So only the lead object change dirties their colors.
TEST(FootPrintNotifications, shadedSelectionSendsOneColorDirtyPerChange)
{
    const HdSceneIndexBaseRefPtr sceneIndex = FindFootPrintTerminalSceneIndex();
    ASSERT_TRUE(sceneIndex);
    const SdfPathVector meshPaths = GetFootPrintMeshPaths(sceneIndex);
    ASSERT_EQ(meshPaths.size(), 2u);

    for (const std::string& command :
         { "select -replace " + kFootPrintShapeName, std::string("select -clear") }) {
        SceneIndexNotificationsAccumulator accumulator(sceneIndex);
        ExecuteAndRefresh(command);
        for (const auto& meshPath : meshPaths) {
            EXPECT_EQ(CountDirties(accumulator, meshPath, kDisplayColorLocator), 1u)
                << command << ": " << DescribeDirtyPrimEntriesSince(accumulator, 0, meshPath);
        }
    }
}

// Deselecting a footprint that is not the lead object changes no lead, so only the
// selection-state color dirty can refresh its wireframe color.
TEST(FootPrintNotifications, wireframeNonLeadDeselectDirtiesWireframeColor)
{
    const HdSceneIndexBaseRefPtr sceneIndex = FindFootPrintTerminalSceneIndex();
    ASSERT_TRUE(sceneIndex);
    const SdfPathVector meshPaths = GetFootPrintMeshPaths(sceneIndex);
    ASSERT_EQ(meshPaths.size(), 2u);

    ExecuteAndRefresh("select -replace " + kFootPrintShapeName + "; select -add " + kCubeName);

    SceneIndexNotificationsAccumulator accumulator(sceneIndex);
    ExecuteAndRefresh("select -deselect " + kFootPrintShapeName);

    for (const auto& meshPath : meshPaths) {
        EXPECT_GE(CountDirties(accumulator, meshPath, primvarsOverrideWireframeColorLocator), 1u)
            << DescribeDirtyPrimEntriesSince(accumulator, 0, meshPath);
    }
}
