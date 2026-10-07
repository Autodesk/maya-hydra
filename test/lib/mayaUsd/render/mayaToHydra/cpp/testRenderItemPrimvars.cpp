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
// Primvar declarations of the MRenderItem adapter path. Python wrapper
// testRenderItemPrimvars.py runs these suites in render items mode (no mesh adapter env var).

#include "testUtils.h"

#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMeshShapeOptionVar = "mhMeshShape";

size_t GetPrimvarArraySize(const HdPrimvarsSchema& primvars, const TfToken& name)
{
    HdSampledDataSourceHandle valueDs = primvars.GetPrimvar(name).GetPrimvarValue();
    return valueDs ? valueDs->GetValue(0.0f).GetArraySize() : 0;
}

// Collect the mesh render item prims of a Maya shape from the MayaHydraSceneIndex.
PrimEntriesVector FindMeshRenderItemPrims(const std::string& meshShapeFull)
{
    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    if (!TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex)) {
        return {};
    }
    SceneIndexInspector inspector(mayaSceneIndex);
    return inspector.FindPrims(
        CreatePrimPredicate(GetShapeNameFromFullPath(meshShapeFull), HdPrimTypeTokens->mesh));
}

// Every optional vertex primvar that is declared must hold one value per point: a declared
// but empty st makes render delegates build a degenerate tangent frame (black shading).
void ExpectDeclaredPrimvarsArePopulated(const PrimEntry& primEntry)
{
    HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
    const size_t     numPoints = GetPrimvarArraySize(primvars, HdTokens->points);
    ASSERT_GT(numPoints, 0u) << primEntry.primPath.GetText() << " has no points";

    for (const TfToken& name : { HdTokens->normals, TfToken("st"), TfToken("tangents") }) {
        if (primvars.GetPrimvar(name).IsDefined()) {
            EXPECT_EQ(GetPrimvarArraySize(primvars, name), numPoints)
                << primEntry.primPath.GetText() << " declares primvar '" << name.GetString()
                << "' without one value per point";
        }
    }
}

} // namespace

// What: a mesh with an untextured MaterialX shader must not advertise unpopulated primvars.
// How: open RedMtlxSphere.ma (standard_surface, no textures: VP2 supplies no UV stream) and
//      inspect the primvars of every mesh render item of the sphere.
// Expect: normals/st/tangents, when declared, hold one value per point.
// Regression: the adapter declared st/tangents unconditionally, so Flash shaded the sphere black.
TEST(RenderItemPrimvars, UntexturedMaterialXDeclaresOnlyPopulatedPrimvars)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, "pSphereShape1");
    const PrimEntriesVector prims = FindMeshRenderItemPrims(meshShapeFull);
    ASSERT_FALSE(prims.empty()) << meshShapeFull << " render item not found";

    for (const PrimEntry& primEntry : prims) {
        ExpectDeclaredPrimvarsArePopulated(primEntry);
    }
}

// What: a mesh with a file-textured shader must keep its UVs.
// How: open testUVs.ma (blinn with file textures) and inspect the mesh render items of a plane.
// Expect: at least one render item declares st, and declared primvars are fully populated.
TEST(RenderItemPrimvars, TexturedMeshDeclaresPopulatedUVs)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, "pPlaneShape1");
    const PrimEntriesVector prims = FindMeshRenderItemPrims(meshShapeFull);
    ASSERT_FALSE(prims.empty()) << meshShapeFull << " render item not found";

    bool hasUVs = false;
    for (const PrimEntry& primEntry : prims) {
        ExpectDeclaredPrimvarsArePopulated(primEntry);
        HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
        hasUVs = hasUVs || primvars.GetPrimvar(TfToken("st")).IsDefined();
    }
    EXPECT_TRUE(hasUVs) << "A textured mesh render item should declare the st primvar";
}
