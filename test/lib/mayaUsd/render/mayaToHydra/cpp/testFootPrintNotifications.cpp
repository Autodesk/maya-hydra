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
// pick the terminal scene index where the footprint has a type: a mesh, or basis curves in
// bounding box display mode.
HdSceneIndexBaseRefPtr FindFootPrintTerminalSceneIndex()
{
    return FindTerminalSceneIndexWithPrim(
        GetTerminalSceneIndices(),
        [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
            if (!IsFootPrintMeshPath(primPath)) {
                return false;
            }
            const TfToken primType = sceneIndex->GetPrim(primPath).primType;
            return primType == HdPrimTypeTokens->mesh
                || primType == HdPrimTypeTokens->basisCurves;
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

// Before USD 26.05, HdDependencyForwardingSceneIndex duplicated dirty notifications, and the
// terminal scene index can sit behind several of them. So color dirties are counted at the
// output of MhDirtySelectionColorsSceneIndex, before any duplication.
// The scene index has no display name, which defaults to its demangled class name.
HdSceneIndexBaseRefPtr FindDirtySelectionColorsSceneIndex(const HdSceneIndexBaseRefPtr& terminal)
{
    return findSceneIndexInTree(terminal, [](const HdSceneIndexBaseRefPtr& sceneIndex) {
        return TfStringEndsWith(sceneIndex->GetDisplayName(), "MhDirtySelectionColorsSceneIndex");
    });
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

// Selects then deselects the footprint, and expects each of its prims to get
// expectedDirtiesPerChange wireframe color dirties at each change.
void ExpectShadedSelectionColorDirties(size_t expectedDirtiesPerChange)
{
    const HdSceneIndexBaseRefPtr sceneIndex = FindFootPrintTerminalSceneIndex();
    ASSERT_TRUE(sceneIndex) << "No terminal scene index has the footprint as a mesh.";
    const SdfPathVector meshPaths = GetFootPrintMeshPaths(sceneIndex);
    ASSERT_EQ(meshPaths.size(), 2u);
    const HdSceneIndexBaseRefPtr dirtySelectionColorsSceneIndex
        = FindDirtySelectionColorsSceneIndex(sceneIndex);
    ASSERT_TRUE(dirtySelectionColorsSceneIndex);

    for (const std::string& command :
         { "select -replace " + kFootPrintShapeName, std::string("select -clear") }) {
        SceneIndexNotificationsAccumulator accumulator(dirtySelectionColorsSceneIndex);
        ExecuteAndRefresh(command);
        for (const auto& meshPath : meshPaths) {
            EXPECT_EQ(
                CountDirties(accumulator, meshPath, primvarsOverrideWireframeColorLocator),
                expectedDirtiesPerChange)
                << command << ": " << DescribeDirtyPrimEntriesSince(accumulator, 0, meshPath);
        }
    }
}

} // namespace

// In shaded mode the original prims do not read a selection color. In legacy selection
// highlighting the highlight mirror prims do, and only the lead object change dirties them.
TEST(FootPrintNotifications, legacyShadedSelectionSendsOneColorDirtyPerChange)
{
    // Exactly one: counted where the dirties are emitted, so this holds on every USD version.
    ExpectShadedSelectionColorDirties(1u);
}

// With outline selection highlighting, or with highlighting disabled, no prim reads a selection
// color in shaded mode, so a selection change sends no color dirty.
TEST(FootPrintNotifications, shadedSelectionWithoutLegacyHighlightSendsNoColorDirty)
{
    ExpectShadedSelectionColorDirties(0u);
}

// Deselecting a footprint that is not the lead object changes no lead, so only the
// selection-state color dirty can refresh its wireframe color.
TEST(FootPrintNotifications, wireframeNonLeadDeselectDirtiesWireframeColor)
{
    const HdSceneIndexBaseRefPtr sceneIndex = FindFootPrintTerminalSceneIndex();
    ASSERT_TRUE(sceneIndex)
        << "No terminal scene index has the footprint as a mesh or as bounding box curves.";
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
