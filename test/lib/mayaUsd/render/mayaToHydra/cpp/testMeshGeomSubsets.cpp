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

#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/geomSubsetSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMeshShapeOptionVar = "mhGeomSubsetMeshShape";
const char* kMeshShapeFallback = "multiCubeShape";

// Collect every geomSubset prim that is a descendant of the given mesh prim path.
PrimEntriesVector FindGeomSubsetsUnderMesh(
    const SceneIndexInspector& inspector,
    const SdfPath&             meshPrimPath)
{
    FindPrimPredicate predicate
        = [&meshPrimPath](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        return primPath.HasPrefix(meshPrimPath)
            && sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->geomSubset;
    };
    return inspector.FindPrims(predicate);
}

// Read the face indices of a geomSubset prim into a sorted set for order-independent
// comparison.
std::set<int> GetGeomSubsetFaceIndices(const HdSceneIndexPrim& prim)
{
    std::set<int>      faces;
    HdGeomSubsetSchema gss = HdGeomSubsetSchema::GetFromParent(prim.dataSource);
    if (!gss.IsDefined() || !gss.GetIndices()) {
        return faces;
    }
    const VtIntArray indices = gss.GetIndices()->GetTypedValue(0.0f);
    for (int index : indices) {
        faces.insert(index);
    }
    return faces;
}

// Read the bound material path of a geomSubset prim (all-purpose binding).
SdfPath GetGeomSubsetMaterialPath(const HdSceneIndexPrim& prim)
{
    HdMaterialBindingsSchema mb = HdMaterialBindingsSchema::GetFromParent(prim.dataSource);
    if (!mb.IsDefined()) {
        return {};
    }
    auto pathDs = mb.GetMaterialBinding().GetPath();
    if (!pathDs) {
        return {};
    }
    return pathDs->GetTypedValue(0.0f);
}

// Locate the mesh prim in the MayaHydraSceneIndex; returns the inspector's scene index
// and the mesh prim path. Asserts (via gtest macros) when prerequisites are missing.
HdSceneIndexBaseRefPtr LocateMayaSceneIndexWithMesh(
    const std::string& shapeNamePart,
    SdfPath&           outMeshPrimPath)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    EXPECT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithMesh
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    EXPECT_TRUE(sceneIndexWithMesh)
        << "Mesh prim not found in any scene index (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";
    if (!sceneIndexWithMesh) {
        return nullptr;
    }

    HdSceneIndexBaseRefPtr mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    EXPECT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";
    if (!mayaSceneIndex) {
        return nullptr;
    }

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
    EXPECT_GE(foundPrims.size(), 1u) << "Mesh not found in MayaHydraSceneIndex";
    if (foundPrims.empty()) {
        return nullptr;
    }
    outMeshPrimPath = foundPrims.front().primPath;
    return mayaSceneIndex;
}

} // namespace

// What: per-face material assignments produce one faceSet geomSubset per material group,
//       each bound to a distinct material, with no full-coverage (overlapping) subset.
// How: the Python scene assigns shaderBSG to faces 0-2 of a 6-face cube (in addition to the
//      whole-object shaderASG), then we inspect the geomSubsets under the mesh prim.
// Expect: the faces {0,1,2} subset is a faceSet bound to a material named "shaderBSG"; every
//         subset binds to an existing material prim; no subset covers all 6 faces.
// Why: regression guard for multi-material geomSubset creation and the overlap-avoidance rule.
TEST(MultiMaterialMesh, PerFaceCreatesGeomSubsets)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex
        = LocateMayaSceneIndexWithMesh(shapeNamePart, meshPrimPath);
    ASSERT_TRUE(mayaSceneIndex);

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   subsets = FindGeomSubsetsUnderMesh(inspector, meshPrimPath);
    ASSERT_GE(subsets.size(), 1u)
        << "A multi-material mesh should expose at least one geomSubset";

    const int numFaces = 6; // cube

    bool foundShaderBFaceSubset = false;
    for (const auto& subsetEntry : subsets) {
        const HdSceneIndexPrim& subsetPrim = subsetEntry.prim;

        // Every subset must be a faceSet.
        HdGeomSubsetSchema gss = HdGeomSubsetSchema::GetFromParent(subsetPrim.dataSource);
        ASSERT_TRUE(gss.IsDefined()) << "geomSubset schema missing on " << subsetEntry.primPath;
        ASSERT_TRUE(gss.GetType());
        EXPECT_EQ(gss.GetType()->GetTypedValue(0.0f), HdGeomSubsetSchemaTokens->typeFaceSet);

        const std::set<int> faces = GetGeomSubsetFaceIndices(subsetPrim);
        EXPECT_FALSE(faces.empty()) << "geomSubset must reference at least one face";

        // Overlap guard: with multiple materials assigned, no subset may cover every face,
        // otherwise it would overlap the per-face subsets with undefined precedence.
        EXPECT_LT(static_cast<int>(faces.size()), numFaces)
            << "No geomSubset should cover all faces when per-face assignments exist";

        // Each subset must bind to a material prim that actually exists in the scene index.
        const SdfPath materialPath = GetGeomSubsetMaterialPath(subsetPrim);
        ASSERT_FALSE(materialPath.IsEmpty()) << "geomSubset must have a material binding";
        EXPECT_EQ(
            mayaSceneIndex->GetPrim(materialPath).primType, HdPrimTypeTokens->material)
            << "geomSubset material binding must resolve to a material prim: " << materialPath;

        const std::set<int> shaderBFaces = { 0, 1, 2 };
        if (materialPath.GetName() == "shaderBSG") {
            foundShaderBFaceSubset = true;
            EXPECT_EQ(faces, shaderBFaces)
                << "shaderBSG should be bound to faces {0,1,2}";
        }
    }

    EXPECT_TRUE(foundShaderBFaceSubset)
        << "Expected a geomSubset bound to the per-face material 'shaderBSG'";
}

// What: a mesh with a single whole-object material assignment must not create geomSubsets.
// How: the Python scene assigns one shading group to an entire cube; we inspect the mesh prim.
// Expect: no geomSubset prims exist under the mesh.
// Why: regression guard for the early-return when there is at most one shading assignment.
TEST(MultiMaterialMesh, SingleAssignmentNoGeomSubset)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex
        = LocateMayaSceneIndexWithMesh(shapeNamePart, meshPrimPath);
    ASSERT_TRUE(mayaSceneIndex);

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   subsets = FindGeomSubsetsUnderMesh(inspector, meshPrimPath);
    EXPECT_TRUE(subsets.empty())
        << "A single-material mesh must not create geomSubsets, found " << subsets.size();
}

// What: non-mesh shapes must never produce geomSubsets.
// How: the Python scene builds a NURBS sphere with a material; we inspect its prim subtree.
// Expect: no geomSubset prims exist under the NURBS shape (and no crash from MFnMesh use).
// Why: guards the mesh-only path of _InsertGeomSubsetsForMesh against non-mesh shapes.
TEST(MultiMaterialMesh, NonMeshNoGeomSubset)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    // The NURBS surface is exposed as a basisCurves/mesh-free prim; locate the maya scene
    // index from any terminal and scan the whole tree for geomSubsets carrying the shape name.
    HdSceneIndexBaseRefPtr mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndices.front());
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    FindPrimPredicate   predicate
        = [&shapeNamePart](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        return primPath.GetString().find(shapeNamePart) != std::string::npos
            && sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->geomSubset;
    };
    PrimEntriesVector subsets = inspector.FindPrims(predicate);
    EXPECT_TRUE(subsets.empty())
        << "Non-mesh shapes must not produce geomSubsets, found " << subsets.size();
}
