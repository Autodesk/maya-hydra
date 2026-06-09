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

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMeshShapeOptionVar = "mhMeshShape";
const char* kMeshShapeFallback = "testCubeShape";

// Scan dirtied entries for a mesh prim and report primvars vs points dirty.
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

// Count primvars-dirty notices for the mesh prim since a starting index.
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

// What: non-param attribute changes should dirty primvars only.
// How: set a custom mesh attribute and inspect notices since the change.
// Expect: primvars dirty is present; points dirty is absent.
TEST(MeshPrimvars, NonParamAttrTriggersOnlyPrimvarDirty)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    ASSERT_TRUE(sceneIndexWithMesh) << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
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

// What: param attribute updates should not duplicate primvars dirty notices.
// How: change uvPivot and count primvars dirty entries since the change.
// Expect: exactly one primvars dirty entry for the mesh.
TEST(MeshPrimvars, UvPivotUpdateNoDuplicatePrimvarsDirty)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    ASSERT_TRUE(sceneIndexWithMesh) << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
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

// What: mesh param attributes list must match the adapter's attribute usage.
// How: compare an expected list against the adapter's param attribute set.
// Expect: all attributes handled by the mesh adapter are present in the set.
TEST(MeshPrimvars, ParamAttributesMatchGetLogic)
{
    static const std::vector<std::string> kAttrsHandledByCallbacks = {
        "pnts", "inMesh", "worldMatrix", "doubleSided", "intermediateObject",
        "uvPivot", "displaySmoothMesh", "smoothLevel", "instObjGroups"
    };

    const auto& paramAttrs = MayaHydra::GetMeshParamAttributeNamesForTest();
    for (const std::string& attr : kAttrsHandledByCallbacks) {
        EXPECT_TRUE(paramAttrs.count(attr)) << "Attribute '" << attr
            << "' is handled by NodeDirtiedCallback or AttributeChangedCallback but is missing "
               "from kMeshParamAttributeNames in meshAdapter.cpp.";
    }
}

// What: a mesh with UVs must expose tangents as a VtVec3fArray with the "vector" role.
// How: read the tangents primvar (value, role, interpolation) from the mesh prim, and
//      cross-check that the st primvar keeps the textureCoordinate role.
// Expect: tangents value holds VtVec3fArray (not a 2-component type), role is vector,
//         interpolation is faceVarying; st (if present) keeps the textureCoordinate role.
// Why: regression guard for the mesh adapter tangents fix, which previously reinterpret_cast
//      the Maya tangents to GfVec2f and tagged them with the textureCoordinate role.
TEST(MeshPrimvars, TangentsAreVec3WithVectorRole)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    ASSERT_TRUE(sceneIndexWithMesh)
        << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "Mesh not found in MayaHydraSceneIndex";

    HdSceneIndexPrim prim = foundPrims.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    HdPrimvarSchema tangents = primvars.GetPrimvar(TfToken("tangents"));
    ASSERT_TRUE(tangents.IsDefined()) << "tangents primvar should be present on a mesh with UVs";

    // Type: tangents are 3-component vectors, so the value must hold a VtVec3fArray.
    ASSERT_TRUE(tangents.GetPrimvarValue());
    VtValue tangentsValue = tangents.GetPrimvarValue()->GetValue(0.0f);
    EXPECT_TRUE(tangentsValue.IsHolding<VtVec3fArray>())
        << "tangents must be VtVec3fArray, got: " << tangentsValue.GetTypeName();

    // Role: tangents are direction vectors, not texture coordinates.
    ASSERT_TRUE(tangents.GetRole());
    EXPECT_EQ(tangents.GetRole()->GetTypedValue(0.0f), HdPrimvarRoleTokens->vector)
        << "tangents role must be 'vector'";

    // Interpolation: tangents are authored per face-vertex.
    ASSERT_TRUE(tangents.GetInterpolation());
    EXPECT_EQ(
        tangents.GetInterpolation()->GetTypedValue(0.0f), HdPrimvarSchemaTokens->faceVarying)
        << "tangents interpolation must be 'faceVarying'";

    // Control: the st primvar coexists with tangents and keeps the textureCoordinate role.
    HdPrimvarSchema st = primvars.GetPrimvar(TfToken("st"));
    if (st.IsDefined() && st.GetRole()) {
        EXPECT_EQ(st.GetRole()->GetTypedValue(0.0f), HdPrimvarRoleTokens->textureCoordinate)
            << "st role must remain 'textureCoordinate'";
    }
}
