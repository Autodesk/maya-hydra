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

#include <mayaHydraLib/hydraUtils.h>

#include <flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h>
#include <flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPathInterface.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/selection/fvpSelection.h>
#include <flowViewport/fvpPrimUtils.h>

#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestHydraPrim, isVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    auto primSelections = Fvp::ufePathToPrimSelections(appPath);

    // If the prim is instanced, get the instancer, and determine if the
    // prim is being masked out by the instancer mask.
    if (!primSelections.empty()) {

        const auto& primSelection = primSelections[0];
        auto prim = siRoot->GetPrim(primSelection.primPath);

        if (Fvp::isPointInstancer(prim)) {

            // If the instancer has no nested instance indices, we want to know
            // the visibility of the instancer itself.
            if (primSelection.nestedInstanceIndices.empty()) {
                ASSERT_TRUE(visibility(siRoot, primSelection.primPath));
                return;
            }

            // Get the point instancer mask.  No mask means all visible.
            HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
            auto maskDs = instancerTopologySchema.GetMask();

            if (!maskDs) {
                // No mask data source means no mask, all visible.
                return;
            }

            auto mask = maskDs->GetTypedValue(0.0f);

            if (mask.empty()) {
                // Empty mask means all visible.
                return;
            }

            // Check if mask is true, i.e. visible.
            for (const auto& instancesSelection : primSelection.nestedInstanceIndices) {
                // When will instancesSelection.instanceIndices have more
                // than one index?
                for (auto instanceIndex : instancesSelection.instanceIndices) {
                    ASSERT_TRUE(mask[instanceIndex]);
                }
            }

            return;
        }

        auto instanceSchema = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instanceSchema.IsDefined()) {
            auto instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);

            // The instancer itself must be visible.  If not, none of its
            // instances will be.
            ASSERT_TRUE(visibility(siRoot, instancerPath));

            HdSceneIndexPrim instancerPrim = siRoot->GetPrim(instancerPath);
            HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);

            // Documentation
            // https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instancerTopologySchema.h#L86
            // says that instanceLocations is only meaningful for native
            // instancing, empty for point instancing.
            auto instanceLocationsDs = instancerTopologySchema.GetInstanceLocations();
            if (!TF_VERIFY(instanceLocationsDs, "Null instance location data source in isVisible() for instancer %s", instancerPath.GetText())) {
                return;
            }

            auto instanceLocations = instanceLocationsDs->GetTypedValue(0.0f);

            // Compute the index for the instance location we're concerned with.
            // This is O(n) complexity for n instances, which is only
            // acceptable because this is test code.
            auto found = std::find(
                instanceLocations.begin(), instanceLocations.end(), 
                primSelection.primPath);

            if (!TF_VERIFY(found != instanceLocations.end(), "Instance %s not found in instancer %s", primSelection.primPath.GetText(), instancerPath.GetText())) {
                return;
            }

            auto ndx = std::distance(instanceLocations.begin(), found);

            auto maskDs = instancerTopologySchema.GetMask();

            if (!TF_VERIFY(maskDs, "Null mask data source in isVisible() for instancer %s", instancerPath.GetText())) {
                return;
            }

            auto mask = maskDs->GetTypedValue(0.0f);

            ASSERT_TRUE(mask[ndx]);

            return;
        }
    }

    // If an application path maps to multiple prim selections, all prim
    // selections must be visible, else we fail.
    unsigned int primVis = 0;
    for (const auto& primSelection : primSelections) {
        if (visibility(siRoot, primSelection.primPath)) {
            ++primVis;
        }
    }

    ASSERT_EQ(primVis, primSelections.size());
}

TEST(TestHydraPrim, notVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    auto primSelections = Fvp::ufePathToPrimSelections(appPath);

    // If the prim is instanced, get the instancer, and determine if the
    // prim is being masked out by the instancer mask.
    if (!primSelections.empty()) {

        const auto& primSelection = primSelections[0];
        auto prim = siRoot->GetPrim(primSelection.primPath);

        if (Fvp::isPointInstancer(prim)) {

            const bool instancerVisibility = visibility(siRoot, primSelection.primPath);

            // If the instancer has no nested instance indices, we want to know
            // the visibility of the instancer itself.
            if (primSelection.nestedInstanceIndices.empty()) {
                ASSERT_FALSE(instancerVisibility);
                return;
            }

            // If the instancer is invisible, there is no point instancer mask,
            // and all instances are invisible.
            if (!instancerVisibility) {
                return;
            }

            // Get the point instancer mask.  No mask means all visible.
            HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
            auto maskDs = instancerTopologySchema.GetMask();

            // No mask data source means no mask, all visible.
            ASSERT_TRUE(maskDs);

            auto mask = maskDs->GetTypedValue(0.0f);

            ASSERT_FALSE(mask.empty());

            // Check if mask is false, i.e. invisible.
            for (const auto& instancesSelection : primSelection.nestedInstanceIndices) {
                // When will instancesSelection.instanceIndices have more
                // than one index?
                for (auto instanceIndex : instancesSelection.instanceIndices) {
                    ASSERT_FALSE(mask[instanceIndex]);
                }
            }

            return;
        }

        auto instanceSchema = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instanceSchema.IsDefined()) {
            auto instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);

            // If the instancer itself is not visible, none of its instances
            // are, which is what we're asserting.
            if (!visibility(siRoot, instancerPath)) {
                // Success.
                return;
            }

            HdSceneIndexPrim instancerPrim = siRoot->GetPrim(instancerPath);
            HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);

            // Documentation
            // https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instancerTopologySchema.h#L86
            // says that instanceLocations is only meaningful for native
            // instancing, empty for point instancing.
            auto instanceLocationsDs = instancerTopologySchema.GetInstanceLocations();
            if (!TF_VERIFY(instanceLocationsDs, "Null instance location data source in isVisible() for instancer %s", instancerPath.GetText())) {
                return;
            }

            auto instanceLocations = instanceLocationsDs->GetTypedValue(0.0f);

            // Compute the index for the instance location we're concerned with.
            // This is O(n) complexity for n instances, which is only
            // acceptable because this is test code.
            auto found = std::find(
                instanceLocations.begin(), instanceLocations.end(), 
                primSelection.primPath);

            if (!TF_VERIFY(found != instanceLocations.end(), "Instance %s not found in instancer %s", primSelection.primPath.GetText(), instancerPath.GetText())) {
                return;
            }

            auto ndx = std::distance(instanceLocations.begin(), found);

            auto maskDs = instancerTopologySchema.GetMask();

            if (!TF_VERIFY(maskDs, "Null mask data source in isVisible() for instancer %s", instancerPath.GetText())) {
                return;
            }

            auto mask = maskDs->GetTypedValue(0.0f);

            ASSERT_FALSE(mask[ndx]);

            return;
        }
    }

    // If an application path maps to multiple prim selections, all prim
    // selections must be invisible, else we fail.
    int primVis = 0;
    for (const auto& primSelection : primSelections) {
        if (visibility(siRoot, primSelection.primPath)) {
            ++primVis;
        }
    }

    ASSERT_EQ(primVis, 0);
}

TEST(TestIsolateSelection, add)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string viewportId(argv[0]);
    const Ufe::Path appPath(Ufe::PathString::path(argv[1]));

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    auto primSelections = Fvp::ufePathToPrimSelections(appPath);
    vpDataMgr.AddIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, remove)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string viewportId(argv[0]);
    const Ufe::Path appPath(Ufe::PathString::path(argv[1]));

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    auto primSelections = Fvp::ufePathToPrimSelections(appPath);
    vpDataMgr.RemoveIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, clear)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string viewportId(argv[0]);

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.ClearIsolateSelection(viewportId);
}

TEST(TestIsolateSelection, replace)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 2);
    const std::string viewportId(argv[0]);
    
    // Use allocator from Flow Viewport Toolkit library.  Directly calling
    //
    // auto isolateSelect = std::make_shared<Fvp::Selection>();
    //
    // here will cause a crash on test exit, as the last reference to the
    // isolate selection is in the isolate select scene index, which can be
    // destroyed and rebuilt on redraw.  On Windows, if the DLL that performed
    // the allocation of the shared_ptr has been unloaded from the process, the
    // isolate selection destruction called by the shared_ptr destructor will
    // crash.
    //
    // This is exactly our case, as our host mayaHydraCppTests DLL plugin is
    // unloaded at program exit, but Maya performs additional redraws after the
    // plugin unload, which cause the isolate selection shared_ptr to be
    // destroyed, which causes a call to code in an unloaded DLL, which crashes.
    auto isolateSelect = Fvp::Selection::New();

    for (int i=1; i < argc; ++i) {
        const Ufe::Path appPath(Ufe::PathString::path(argv[i]));
        auto primSelections = Fvp::ufePathToPrimSelections(appPath);
        for (const auto& primSelection : primSelections) {
            isolateSelect->Add(primSelection);
        }
    }

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.ReplaceIsolateSelection(viewportId, isolateSelect);
}

TEST(TestIsolateSelection, disable)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string viewportId(argv[0]);

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.DisableIsolateSelection(viewportId);
}

