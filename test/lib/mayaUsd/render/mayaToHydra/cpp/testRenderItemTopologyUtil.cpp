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
// Unit tests for renderItemTopologyUtil: connectivity comparison on mesh and curve
// topologies, and RenderItemShouldEmitTopologyLocators policy branches. No Maya scene dependency.

#include <mayaHydraLib/adapters/renderItemTopologyUtil.h>

#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>

#include <maya/MHWGeometry.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

HdMeshTopology MakeTriangleMeshTopology(const VtIntArray& counts, const VtIntArray& indices)
{
    return HdMeshTopology(
        PxOsdOpenSubdivTokens->none, UsdGeomTokens->rightHanded, counts, indices);
}

} // namespace

TEST(RenderItemTopologyUtil, ConnectivityChangedWhenIndicesDiffer)
{
    const VtIntArray counts { 3, 3 };
    const VtIntArray indicesA { 0, 1, 2, 2, 1, 3 };
    const VtIntArray indicesB { 0, 1, 2, 0, 2, 3 };

    const HdMeshTopology stored = MakeTriangleMeshTopology(counts, indicesA);

    EXPECT_FALSE(RenderItemTopologyConnectivityChanged(
        &stored, MGeometry::Primitive::kTriangles, indicesA, counts, 0));
    EXPECT_TRUE(RenderItemTopologyConnectivityChanged(
        &stored, MGeometry::Primitive::kTriangles, indicesB, counts, 0));
}

TEST(RenderItemTopologyUtil, ConnectivityChangedWhenStoredTopologyMissing)
{
    const VtIntArray counts { 3 };
    const VtIntArray indices { 0, 1, 2 };

    EXPECT_FALSE(RenderItemTopologyConnectivityChanged(
        nullptr, MGeometry::Primitive::kTriangles, indices, counts, 0));
    EXPECT_FALSE(RenderItemTopologyConnectivityChanged(
        nullptr, MGeometry::Primitive::kTriangles, {}, {}, 0));
}

TEST(RenderItemTopologyUtil, ShouldEmitWhenTopoOnly)
{
    EXPECT_TRUE(RenderItemShouldEmitTopologyLocators(
        /*topoChanged*/ true,
        /*geomChanged*/ false,
        /*hasGeomAndBuffers*/ true,
        /*positionsEmpty*/ false,
        /*storedPositionCount*/ 24,
        /*currentVertexCount*/ 24,
        /*storedTopology*/ nullptr,
        MGeometry::Primitive::kTriangles,
        {},
        {}));
}

TEST(RenderItemTopologyUtil, ShouldEmitWhenVertexCountChanges)
{
    EXPECT_TRUE(RenderItemShouldEmitTopologyLocators(
        true,
        true,
        true,
        false,
        24,
        26,
        nullptr,
        MGeometry::Primitive::kTriangles,
        {},
        {}));
}

TEST(RenderItemTopologyUtil, ShouldSuppressWhenSameCountAndSameConnectivity)
{
    const VtIntArray counts { 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3 };
    const VtIntArray indices(36);
    const HdMeshTopology stored = MakeTriangleMeshTopology(counts, indices);

    EXPECT_FALSE(RenderItemShouldEmitTopologyLocators(
        true,
        true,
        true,
        false,
        24,
        24,
        &stored,
        MGeometry::Primitive::kTriangles,
        indices,
        counts));
}

TEST(RenderItemTopologyUtil, ShouldEmitWhenSameCountButConnectivityChanges)
{
    const VtIntArray counts { 3, 3 };
    const VtIntArray storedIndices { 0, 1, 2, 2, 1, 3 };
    const VtIntArray newIndices { 0, 1, 2, 0, 2, 3 };
    const HdMeshTopology stored = MakeTriangleMeshTopology(counts, storedIndices);

    EXPECT_TRUE(RenderItemShouldEmitTopologyLocators(
        true,
        true,
        true,
        false,
        24,
        24,
        &stored,
        MGeometry::Primitive::kTriangles,
        newIndices,
        counts));
}

TEST(RenderItemTopologyUtil, ShouldEmitWhenGeomOnlyAndConnectivityChanges)
{
    const VtIntArray counts { 3, 3 };
    const VtIntArray storedIndices { 0, 1, 2, 2, 1, 3 };
    const VtIntArray newIndices { 0, 1, 2, 0, 2, 3 };
    const HdMeshTopology stored = MakeTriangleMeshTopology(counts, storedIndices);

    EXPECT_TRUE(RenderItemShouldEmitTopologyLocators(
        false,
        true,
        true,
        false,
        24,
        24,
        &stored,
        MGeometry::Primitive::kTriangles,
        newIndices,
        counts));
}

TEST(RenderItemTopologyUtil, ShouldSuppressLineStripDeformationWithSameVertexCount)
{
    EXPECT_FALSE(RenderItemShouldEmitTopologyLocators(
        true,
        true,
        true,
        false,
        10,
        10,
        nullptr,
        MGeometry::Primitive::kLineStrip,
        {},
        {}));
}
