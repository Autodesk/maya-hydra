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
// Primvar declarations of the MRenderItem adapter path. Python wrappers
// testRenderItemPrimvars.py and testRenderItemPrimvarsMaterialX.py run these suites in
// render items mode (no mesh adapter env var).

#include "testUtils.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MFloatArray.h>
#include <maya/MFnMesh.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MString.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMeshShapeOptionVar = "mhMeshShape";

constexpr float kTolerance = 1e-4f;

size_t GetPrimvarArraySize(const HdPrimvarsSchema& primvars, const TfToken& name)
{
    HdSampledDataSourceHandle valueDs = primvars.GetPrimvar(name).GetPrimvarValue();
    return valueDs ? valueDs->GetValue(0.0f).GetArraySize() : 0;
}

template <typename T>
VtArray<T> GetPrimvarArray(const HdPrimvarsSchema& primvars, const TfToken& name)
{
    HdSampledDataSourceHandle valueDs = primvars.GetPrimvar(name).GetPrimvarValue();
    const VtValue             value = valueDs ? valueDs->GetValue(0.0f) : VtValue();
    return value.IsHolding<VtArray<T>>() ? value.UncheckedGet<VtArray<T>>() : VtArray<T>();
}

// Collect the mesh render item prims of a Maya shape from the MayaHydraSceneIndex.
PrimEntriesVector FindMeshRenderItemPrims(
    const std::string&      meshShapeFull,
    HdSceneIndexBaseRefPtr* outMayaSceneIndex = nullptr)
{
    SdfPath                meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    if (!TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex)) {
        return {};
    }
    if (outMayaSceneIndex) {
        *outMayaSceneIndex = mayaSceneIndex;
    }
    SceneIndexInspector inspector(mayaSceneIndex);
    return inspector.FindPrims(
        CreatePrimPredicate(GetShapeNameFromFullPath(meshShapeFull), HdPrimTypeTokens->mesh));
}

// The UVs of the current UV set of a Maya mesh shape.
std::vector<GfVec2f> GetMayaMeshUVs(const std::string& meshShapeFull)
{
    MSelectionList selection;
    MDagPath       dagPath;
    if (!selection.add(meshShapeFull.c_str()) || !selection.getDagPath(0, dagPath)) {
        return {};
    }
    MFnMesh     mesh(dagPath);
    MFloatArray us, vs;
    mesh.getUVs(us, vs);
    std::vector<GfVec2f> uvs;
    for (unsigned int i = 0; i < us.length(); ++i) {
        uvs.emplace_back(us[i], vs[i]);
    }
    return uvs;
}

// Decoded values must match the Maya data, not just its size: a wrong offset, stride or
// component count still yields one value per point, but reads across element boundaries.
void ExpectDecodedPrimvarValues(const PrimEntry& primEntry, const std::vector<GfVec2f>& mayaUVs)
{
    HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
    const char*      primPath = primEntry.primPath.GetText();

    if (primvars.GetPrimvar(TfToken("st")).IsDefined()) {
        const VtVec2fArray st = GetPrimvarArray<GfVec2f>(primvars, TfToken("st"));
        ASSERT_FALSE(st.empty()) << primPath << " st is not a VtVec2fArray";
        for (size_t i = 0; i < st.size(); ++i) {
            bool isMayaUV = false;
            for (const GfVec2f& uv : mayaUVs) {
                isMayaUV = isMayaUV || GfIsClose(st[i], uv, kTolerance);
            }
            EXPECT_TRUE(isMayaUV) << primPath << " st[" << i << "] = (" << st[i][0] << ", "
                                  << st[i][1] << ") is not a UV of the Maya mesh";
        }
    }

    for (const TfToken& name : { HdTokens->normals, TfToken("tangents") }) {
        if (!primvars.GetPrimvar(name).IsDefined()) {
            continue;
        }
        const VtVec3fArray values = GetPrimvarArray<GfVec3f>(primvars, name);
        ASSERT_FALSE(values.empty())
            << primPath << " " << name.GetString() << " is not a VtVec3fArray";
        for (size_t i = 0; i < values.size(); ++i) {
            EXPECT_NEAR(values[i].GetLength(), 1.0f, kTolerance)
                << primPath << " " << name.GetString() << "[" << i << "] is not unit length";
        }
    }
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
// Expect: at least one render item declares st, and declared primvars are fully populated with
//      the decoded Maya values: st values are UVs of the mesh, normals and tangents are unit
//      length.
TEST(RenderItemPrimvars, TexturedMeshDeclaresPopulatedUVs)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, "pPlaneShape1");
    const PrimEntriesVector prims = FindMeshRenderItemPrims(meshShapeFull);
    ASSERT_FALSE(prims.empty()) << meshShapeFull << " render item not found";
    const std::vector<GfVec2f> mayaUVs = GetMayaMeshUVs(meshShapeFull);
    ASSERT_FALSE(mayaUVs.empty()) << meshShapeFull << " has no UVs";

    bool hasUVs = false;
    for (const PrimEntry& primEntry : prims) {
        ExpectDeclaredPrimvarsArePopulated(primEntry);
        ExpectDecodedPrimvarValues(primEntry, mayaUVs);
        HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
        hasUVs = hasUVs || primvars.GetPrimvar(TfToken("st")).IsDefined();
    }
    EXPECT_TRUE(hasUVs) << "A textured mesh render item should declare the st primvar";
}

// What: a shader losing its textures must withdraw the UVs of the mesh render item.
// How: open testUVs.ma and disconnect the file textures of blinn1, so VP2 stops supplying the
//      UV stream to the plane's existing render item.
// Expect: every render item that declared st (or tangents) gets that primvar dirtied, st is no
//      longer declared, and declared primvars are fully populated.
// Regression: the CPU UV array read while the shader was textured was kept, so st stayed
//      declared with stale values. Without the dirty notification, consumers downstream of the
//      MayaHydraSceneIndex would keep the stale primvar even though a fresh query omits it.
TEST(RenderItemPrimvars, TexturedToUntexturedWithdrawsUVs)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, "pPlaneShape1");
    HdSceneIndexBaseRefPtr  mayaSceneIndex;
    const PrimEntriesVector texturedPrims = FindMeshRenderItemPrims(meshShapeFull, &mayaSceneIndex);
    ASSERT_FALSE(texturedPrims.empty()) << meshShapeFull << " render item not found";

    std::vector<std::pair<SdfPath, bool>> primsWithUVs; // prim path, declares tangents
    for (const PrimEntry& primEntry : texturedPrims) {
        HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
        if (primvars.GetPrimvar(TfToken("st")).IsDefined()) {
            primsWithUVs.emplace_back(
                primEntry.primPath, primvars.GetPrimvar(TfToken("tangents")).IsDefined());
        }
    }
    ASSERT_FALSE(primsWithUVs.empty()) << meshShapeFull << " declares no st while textured";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    for (const char* connection :
         { "file1.oc blinn1.c", "file2.oc blinn1.ic", "file3.oc blinn1.sc" }) {
        ASSERT_EQ(MGlobal::executeCommand(MString("disconnectAttr ") + connection), MS::kSuccess)
            << "Failed to disconnect " << connection;
    }
    MGlobal::executeCommand("refresh");

    const PrimEntriesVector prims = FindMeshRenderItemPrims(meshShapeFull);
    ASSERT_FALSE(prims.empty()) << meshShapeFull << " render item not found";
    for (const PrimEntry& primEntry : prims) {
        ExpectDeclaredPrimvarsArePopulated(primEntry);
        HdPrimvarsSchema primvars = HdPrimvarsSchema::GetFromParent(primEntry.prim.dataSource);
        EXPECT_FALSE(primvars.GetPrimvar(TfToken("st")).IsDefined())
            << primEntry.primPath.GetText() << " still declares st after its shader lost its "
            << "textures";

        const auto texturedPrim = std::find_if(
            primsWithUVs.begin(), primsWithUVs.end(), [&primEntry](const auto& entry) {
                return entry.first == primEntry.primPath;
            });
        if (texturedPrim == primsWithUVs.end()) {
            continue;
        }
        const MeshDirtySignals signals
            = ClassifyMeshDirtySince(notifsAccumulator, startIndex, primEntry.primPath);
        EXPECT_TRUE(signals.uvs || signals.broadPrimvars)
            << primEntry.primPath.GetText() << " st was not dirtied when withdrawn. Dirtied: "
            << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, primEntry.primPath);
        // The untextured shader may still request tangents; only a withdrawal must be notified.
        const bool tangentsWithdrawn
            = texturedPrim->second && !primvars.GetPrimvar(TfToken("tangents")).IsDefined();
        if (tangentsWithdrawn) {
            EXPECT_TRUE(signals.tangents || signals.broadPrimvars)
                << primEntry.primPath.GetText()
                << " tangents were not dirtied when withdrawn. Dirtied: "
                << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, primEntry.primPath);
        }
    }
}
