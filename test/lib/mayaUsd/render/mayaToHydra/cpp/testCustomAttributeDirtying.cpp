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

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <cmath>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kAttrName = "extDirty";
const char* kMeshShapeName = "dirtyMeshShape";
const char* kCameraShapeName = "dirtyCameraShape";
const char* kLightShapeName = "dirtyLightShape";
const char* kMaterialSgName = "dirtyMaterialSG";
const char* kMeshShapeOptionVar = "mhDirtyMeshShape";
const char* kCameraShapeOptionVar = "mhDirtyCameraShape";
const char* kLightShapeOptionVar = "mhDirtyLightShape";
const char* kMaterialSgOptionVar = "mhDirtyMaterialSg";

std::string GetOptionVarOrDefault(const char* optionVar, const char* fallback)
{
    if (MGlobal::optionVarExists(optionVar)) {
        return MGlobal::optionVarStringValue(optionVar).asChar();
    }
    return fallback;
}

void SetAttrAndRefresh(const std::string& nodeName, double value)
{
    const std::string cmd = std::string("setAttr \"")
        + nodeName + "." + kAttrName + "\" " + std::to_string(value);
    MGlobal::executeCommand(cmd.c_str());
    MGlobal::executeCommand("refresh");
}

// Return the prim path from the first dirty entry that has the primvar with expectedValue,
// or empty path if none found.
SdfPath FindPrimPathWithPrimvarValueSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t startIndex,
    double expectedValue)
{
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    auto sceneIndex = accumulator.GetObservedSceneIndex();
    const TfToken primvarName(kAttrName);

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (!entries[i].dirtyLocators.Contains(HdPrimvarsSchema::GetDefaultLocator())) {
            continue;
        }

        HdSceneIndexPrim prim = sceneIndex->GetPrim(entries[i].primPath);
        if (!prim.dataSource) {
            continue;
        }

        HdSampledDataSourceHandle valueDs
            = HdPrimvarsSchema::GetFromParent(prim.dataSource)
                  .GetPrimvar(primvarName)
                  .GetPrimvarValue();
        if (!valueDs) {
            continue;
        }

        VtValue value = valueDs->GetValue(0.0f);
        if (!value.IsHolding<double>()) {
            continue;
        }

        const double actual = value.UncheckedGet<double>();
        if (std::abs(actual - expectedValue) <= 1e-6) {
            return entries[i].primPath;
        }
    }
    return SdfPath();
}

// Return true if any dirty entry for primPath in [startIndex, end) contains the given locator.
// Used to detect redundant schema dirtying when only extension attrs change (should only dirty
// primvars, not schema params).
bool AnyPrimEntryHasLocator(
    const HdSceneIndexObserver::DirtiedPrimEntries& entries,
    size_t startIndex,
    const SdfPath& primPath,
    const HdDataSourceLocator& locator)
{
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath == primPath
            && entries[i].dirtyLocators.Intersects(locator)) {
            return true;
        }
    }
    return false;
}

// Shared runner for "no duplicate dirty on extension attr change" tests.
// Verifies that changing an extension attribute only dirties primvars, not the given schema
// locator (camera params, light params, mesh topology, etc.).
void RunNoDuplicateDirtyOnExtAttrChangeTest(
    const char* shapeOptionVar,
    const char* shapeNameFallback,
    double testValue,
    const HdDataSourceLocator& redundantLocator,
    const char* primType,
    const char* redundantWhat)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexNotificationsAccumulator notifsAccumulator(sceneIndices.front());

    const std::string shapeName = GetOptionVarOrDefault(shapeOptionVar, shapeNameFallback);

    MGlobal::executeCommand("refresh");

    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
    SetAttrAndRefresh(shapeName, testValue);

    const SdfPath primPath
        = FindPrimPathWithPrimvarValueSince(notifsAccumulator, startIndex, testValue);
    ASSERT_FALSE(primPath.IsEmpty())
        << "Expected to find " << primType << " prim with updated extDirty value " << testValue;

    const bool hasRedundantDirty = AnyPrimEntryHasLocator(
        notifsAccumulator.GetDirtiedPrimEntries(), startIndex, primPath, redundantLocator);

    EXPECT_FALSE(hasRedundantDirty)
        << "Changing an extension attribute on a " << primType << " should only dirty primvars, "
           "not " << redundantWhat << ". Found schema locator in dirty entries.";
}

} // namespace

// Unit test: ensure extension-attribute primvars dirty for mesh, camera, light, material.
TEST(CustomAttributeDirtying, testDirtyPrimvars)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexNotificationsAccumulator notifsAccumulator(sceneIndices.front());

    auto checkDirty = [&](const std::string& nodeName, double value) {
        const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
        SetAttrAndRefresh(nodeName, value);
        EXPECT_FALSE(
            FindPrimPathWithPrimvarValueSince(notifsAccumulator, startIndex, value).IsEmpty());
    };

    checkDirty(GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeName), 1.0);
    checkDirty(GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeName), 2.0);
    checkDirty(GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeName), 3.0);
    checkDirty(GetOptionVarOrDefault(kMaterialSgOptionVar, kMaterialSgName), 4.0);
}

// Unit test: ensure changing an extension attribute on a camera does NOT redundantly dirty
// camera params (only primvars should be dirtied).
TEST(CustomAttributeDirtying, testNoDuplicateCameraDirtyOnExtAttrChange)
{
    RunNoDuplicateDirtyOnExtAttrChangeTest(
        kCameraShapeOptionVar,
        kCameraShapeName,
        42.0,
        HdCameraSchema::GetDefaultLocator(),
        "camera",
        "camera params");
}

// Unit test: ensure changing an extension attribute on a light does NOT redundantly dirty
// light params (only primvars should be dirtied).
TEST(CustomAttributeDirtying, testNoDuplicateLightDirtyOnExtAttrChange)
{
    RunNoDuplicateDirtyOnExtAttrChangeTest(
        kLightShapeOptionVar,
        kLightShapeName,
        43.0,
        HdLightSchema::GetDefaultLocator(),
        "light",
        "light params");
}

// Unit test: ensure changing an extension attribute on a mesh does NOT redundantly dirty
// mesh topology (only primvars should be dirtied).
TEST(CustomAttributeDirtying, testNoDuplicateMeshDirtyOnExtAttrChange)
{
    RunNoDuplicateDirtyOnExtAttrChangeTest(
        kMeshShapeOptionVar,
        kMeshShapeName,
        44.0,
        HdMeshSchema::GetTopologyLocator(),
        "mesh",
        "mesh topology");
}
