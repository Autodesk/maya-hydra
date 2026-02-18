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

#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <algorithm>
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

bool AttrExists(const std::string& nodeName)
{
    int exists = 0;
    const std::string cmd = std::string("attributeQuery -exists -node \"")
        + nodeName + "\" " + kAttrName;
    MGlobal::executeCommand(cmd.c_str(), exists);
    return exists != 0;
}

void SetAttrAndRefresh(const std::string& nodeName, double value)
{
    const std::string cmd = std::string("setAttr \"")
        + nodeName + "." + kAttrName + "\" " + std::to_string(value);
    MGlobal::executeCommand(cmd.c_str());
    MGlobal::executeCommand("refresh");
}

bool FindDirtyPrimWithPrimvarValueSince(
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
            return true;
        }
    }
    return false;
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
        EXPECT_TRUE(FindDirtyPrimWithPrimvarValueSince(notifsAccumulator, startIndex, value));
    };

    checkDirty(GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeName), 1.0);
    checkDirty(GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeName), 2.0);
    checkDirty(GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeName), 3.0);
    checkDirty(GetOptionVarOrDefault(kMaterialSgOptionVar, kMaterialSgName), 4.0);
}
