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

#include <mayaHydraLib/adapters/meshAdapterTestUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMeshShapeOptionVar = "mhMeshShape";
const char* kMeshShapeFallback = "testCubeShape";

std::string GetOptionVarOrDefault(const char* optionVar, const char* fallback)
{
    if (MGlobal::optionVarExists(optionVar)) {
        return MGlobal::optionVarStringValue(optionVar).asChar();
    }
    return fallback;
}

FindPrimPredicate getMeshPredicate(const std::string& shapeNamePart)
{
    return [shapeNamePart](const HdSceneIndexBasePtr& sceneIndex,
                          const SdfPath&            primPath) -> bool {
        if (primPath.GetAsString().find(shapeNamePart) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == HdPrimTypeTokens->mesh;
    };
}

std::string getShapeNameFromFullPath(const std::string& fullPath)
{
    size_t lastPipe = fullPath.rfind('|');
    if (lastPipe != std::string::npos && lastPipe + 1 < fullPath.size()) {
        return fullPath.substr(lastPipe + 1);
    }
    return fullPath;
}

void CheckMeshDirtySince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      meshPrimPath,
    bool&                               outHadPrimvarsDirty,
    bool&                               outHadPointsDirty)
{
    outHadPrimvarsDirty = false;
    outHadPointsDirty = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto  pointsLocator = HdDataSourceLocator(HdTokens->points);

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != meshPrimPath) {
            continue;
        }
        if (entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            outHadPrimvarsDirty = true;
        }
        if (entries[i].dirtyLocators.Intersects(pointsLocator)) {
            outHadPointsDirty = true;
        }
    }
}

size_t CountPrimvarsDirtyEntriesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      meshPrimPath)
{
    size_t count = 0;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath == meshPrimPath
            && entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            ++count;
        }
    }
    return count;
}

} // namespace

// Verify that changing a non-param attribute (custom attr) triggers ONLY DirtyPrimvar,
// not schema dirty from NodeDirtiedCallback. Param attrs trigger NodeDirtiedCallback;
// primvar-only attrs must not.
TEST(MeshPrimvars, NonParamAttrTriggersOnlyPrimvarDirty)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = getShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh(nullptr);
    for (const HdSceneIndexBaseRefPtr& si : sceneIndices) {
        SceneIndexInspector inspector(si);
        PrimEntriesVector   foundPrims = inspector.FindPrims(getMeshPredicate(shapeNamePart), 1);
        if (foundPrims.size() >= 1u) {
            sceneIndexWithMesh = si;
            break;
        }
    }
    ASSERT_TRUE(sceneIndexWithMesh) << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(getMeshPredicate(shapeNamePart), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "Mesh not found in MayaHydraSceneIndex";
    const SdfPath meshPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change testCustomAttr (not in param attr list - primvar-only); Python adds this attr
    MGlobal::executeCommand(
        ("setAttr \"" + meshShapeFull + ".testCustomAttr\" 0.5").c_str());
    MGlobal::executeCommand("refresh");

    bool hadPrimvarsDirty = false;
    bool hadPointsDirty = false;
    CheckMeshDirtySince(
        notifsAccumulator, startIndex, meshPrimPath, hadPrimvarsDirty, hadPointsDirty);

    EXPECT_TRUE(hadPrimvarsDirty)
        << "Changing testCustomAttr (non-param-attr) should trigger DirtyPrimvar";
    EXPECT_FALSE(hadPointsDirty)
        << "Changing testCustomAttr (non-param-attr) must NOT trigger DirtyPoints - "
           "only primvars dirty expected";
}

// Verify that updating uvPivot (param attr) does not produce duplicate
// primvars dirty notifications - we should receive exactly one.
TEST(MeshPrimvars, UvPivotUpdateNoDuplicatePrimvarsDirty)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = getShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh(nullptr);
    for (const HdSceneIndexBaseRefPtr& si : sceneIndices) {
        SceneIndexInspector inspector(si);
        PrimEntriesVector   foundPrims = inspector.FindPrims(getMeshPredicate(shapeNamePart), 1);
        if (foundPrims.size() >= 1u) {
            sceneIndexWithMesh = si;
            break;
        }
    }
    ASSERT_TRUE(sceneIndexWithMesh) << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(getMeshPredicate(shapeNamePart), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "Mesh not found in MayaHydraSceneIndex";
    const SdfPath meshPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change uvPivot (param attribute - in kMeshParamAttributeNames)
    MGlobal::executeCommand(
        ("setAttr \"" + meshShapeFull + ".uvPivot\" 0.25 0.25").c_str());
    MGlobal::executeCommand("refresh");

    const size_t primvarsDirtyCount = CountPrimvarsDirtyEntriesSince(
        notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_EQ(primvarsDirtyCount, 1u)
        << "Updating uvPivot (param attr) should trigger exactly one primvars dirty "
           "notification, got " << primvarsDirtyCount << " (duplicate notifications)";
}

// Consistency check: every attribute in NodeDirtiedCallback _dirtyBits or handled
// in AttributeChangedCallback must be in kMeshParamAttributeNames.
TEST(MeshPrimvars, ParamAttributesMatchGetLogic)
{
    static const std::vector<std::string> kAttrsHandledByCallbacks = {
        "pnts", "inMesh", "worldMatrix", "doubleSided", "intermediateObject",
        "uvPivot", "displaySmoothMesh", "smoothLevel", "instObjGroups"
    };

    const auto& paramAttrs = GetMeshParamAttributeNamesForTest();
    for (const std::string& attr : kAttrsHandledByCallbacks) {
        EXPECT_TRUE(paramAttrs.count(attr)) << "Attribute '" << attr
            << "' is handled by NodeDirtiedCallback or AttributeChangedCallback but is missing "
               "from kMeshParamAttributeNames in meshAdapter.cpp.";
    }
}
