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
// Shared Maya integration tests for deformation vs topology dirty locators. Builds a live
// scene, performs edits via MEL, and classifies scene-index dirty notices against
// doc/render_delegate_topology_vs_deformation.md. Python wrappers testMeshDirtyLocators.py
// (mesh adapter) and testRenderItemDirtyLocators.py (render items) run these suites.

#include "testUtils.h"

#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>
#include <maya/MStringArray.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

const char* kMeshShapeOptionVar = "mhMeshShape";
const char* kMeshShapeFallback = "testCubeShape";
const char* kMeshTransformOptionVar = "mhMeshTransform";
const char* kMeshTransformFallback = "testCube";
const char* kCameraShapeOptionVar = "mhCameraShape";
const char* kCameraShapeFallback = "perspShape";

// ---------------------------------------------------------------------------
// Shared test-body helpers
// These free functions hold the actual test logic so that both the
// MeshDirtyLocators and RenderItemDirtyLocators GTest suites can call the
// same body without code duplication. Macros such as ASSERT_* and EXPECT_*
// still propagate failures correctly because they use the calling gtest frame.
// ---------------------------------------------------------------------------

void RunDeformationVertexMoveTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshShapeFull + ".vtx[0]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("move -r 0.5 0 0"), MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.points)
        << "Vertex move should dirty primvars/points\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.meshTopology)
        << "Vertex move must not dirty mesh topology\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "Vertex move must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.extCompPrimvars)
        << "Vertex move must not dirty extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

void RunRenderItemDeformationVertexMoveTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshShapeFull + ".vtx[0]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("move -r 0.5 0 0"), MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    // Deformation-only: Maya may set MVS_changedTopo alongside MVS_changedGeometry for
    // component moves, but the adapter suppresses topology locators when vertex count is
    // unchanged (see renderItemAdapter UpdateFromDelta).
    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.points)
        << "Vertex move should dirty primvars/points\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.meshTopology)
        << "Vertex move must not dirty mesh topology (deformation-only)\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.uvs)
        << "Vertex move should dirty primvars/st (geom/topo path)\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "Vertex move must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.extCompPrimvars)
        << "Vertex move must not dirty extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

void RunDynamicAttrOnMeshEmitsExtComputationPrimvarsTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string meshShapeName = GetShapeNameFromFullPath(meshShapeFull);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("addAttr -ln testExtCompGateAttr -at double " + meshShapeName).c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeName + ".testExtCompGateAttr 1.0").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.extCompPrimvars)
        << "Dynamic attribute change on a mesh (rprim) must emit extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.broadPrimvars)
        << "Dynamic attribute change must emit broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);

    MGlobal::executeCommand(("deleteAttr " + meshShapeName + ".testExtCompGateAttr").c_str());
    MGlobal::executeCommand("refresh");
}

void RunIntermediateObjectToggleVisibilityTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    ASSERT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Expected mesh visible before intermediateObject toggle";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);

    size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeFull + ".intermediateObject 1").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.visibility)
        << "intermediateObject=1 should dirty the visibility schema locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(visibility(mayaSceneIndex, meshPrimPath))
        << "Hydra visibility should reflect intermediateObject=1";

    startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeFull + ".intermediateObject 0").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    signals = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.visibility)
        << "intermediateObject=0 should dirty the visibility schema locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Hydra visibility should reflect intermediateObject=0";
}

void RunInstancedTransformVisibilityToggleTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string meshTransformFull
        = GetOptionVarOrDefault(kMeshTransformOptionVar, kMeshTransformFallback);
    const std::string meshTransformName = GetShapeNameFromFullPath(meshTransformFull);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    // Register _InstancerNodeDirty instead of _TransformNodeDirty on the dag adapter.
    ASSERT_EQ(MGlobal::executeCommand(
                  ("duplicate -rr -ilf " + meshTransformName).c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    ASSERT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Expected mesh visible before hiding the master transform";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshTransformFull + ".visibility 0").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.instancer)
        << "Instanced shape: master transform visibility=0 should dirty instancer locators\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.visibility)
        << "Instanced shape: master transform visibility=0 should dirty visibility locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(visibility(mayaSceneIndex, meshPrimPath))
        << "Hydra visibility should reflect hidden master transform on instanced shape";

    startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshTransformFull + ".visibility 1").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    signals = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.instancer)
        << "Instanced shape: master transform visibility=1 should dirty instancer locators\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.visibility)
        << "Instanced shape: master transform visibility=1 should dirty visibility locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Hydra visibility should reflect visible master transform on instanced shape";
}

void RunInstancedNonMasterVisibilityToggleTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string meshTransformFull
        = GetOptionVarOrDefault(kMeshTransformOptionVar, kMeshTransformFallback);
    const std::string meshTransformName = GetShapeNameFromFullPath(meshTransformFull);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    ASSERT_EQ(MGlobal::executeCommand(
                  ("duplicate -rr -ilf " + meshTransformName).c_str()),
              MStatus::kSuccess);
    const std::string duplicateTransformName = meshTransformName + "1";
    MStringArray duplicatePaths;
    ASSERT_EQ(MGlobal::executeCommand(
                  ("ls -l " + duplicateTransformName).c_str(), duplicatePaths),
              MStatus::kSuccess);
    ASSERT_GT(duplicatePaths.length(), 0u);
    const std::string duplicateTransformFull = duplicatePaths[0].asChar();
    MGlobal::executeCommand("refresh");

    ASSERT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Expected prototype visible while master and duplicate transforms are visible";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + duplicateTransformFull + ".visibility 0").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.instancer)
        << "Instanced shape: duplicate transform visibility=0 should dirty instancer locators\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.visibility)
        << "Instanced shape: duplicate-only visibility should not dirty prototype visibility\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Prototype visibility schema should stay visible when only a duplicate instance is hidden";

    startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + duplicateTransformFull + ".visibility 1").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    signals = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.instancer)
        << "Instanced shape: duplicate transform visibility=1 should dirty instancer locators\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.visibility)
        << "Instanced shape: duplicate-only visibility should not dirty prototype visibility\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(visibility(mayaSceneIndex, meshPrimPath))
        << "Prototype visibility schema should remain visible after showing duplicate instance";
}

void RunRenderItemConnectivityChangeSameVertexCountTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    // Ensure render-item topology baseline is captured before measuring dirties from the edit.
    MGlobal::executeCommand("refresh");
    size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Flip a cube edge: retriangulates a quad face without changing the position buffer size.
    // Maya may set topoChanged and geomChanged together; topology locators must still emit when
    // index connectivity changes while vertex count is unchanged.
    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshShapeFull + ".e[4]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("polyFlipEdge"), MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.meshTopology)
        << "Edge flip with unchanged vertex count should dirty mesh topology when connectivity "
           "changes\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "Render item connectivity change must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

void RunSkinnedMeshDeformationEmitsPointsTest()
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1 and skinned test scene)";

    // Evaluate frame 1 so the skinned mesh is in a known evaluated state before issuing dgdirty.
    ASSERT_EQ(MGlobal::viewFrame(1.0), MS::kSuccess);
    MGlobal::executeCommand("refresh");

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Reproduce the batch-render skin bug deterministically: dgdirty inMesh while evaluated
    // point positions are unchanged. NodeDirtiedCallback must not read mesh data during DG dirty
    // propagation; it should always dirty points/extent, not UV-only locators (primvars/st).
    // Joint rotation or a full time-change evaluation would move points first and would not
    // catch a UV-only misclassification on inMesh/pnts.
    ASSERT_EQ(MGlobal::executeCommand(
                  ("dgdirty " + meshShapeFull + ".inMesh").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the skinned mesh prim";
    EXPECT_TRUE(signals.points)
        << "inMesh dirty with unchanged point positions must still emit primvars/points "
           "(UV-only locators on inMesh/pnts would miss deformation)\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.extent)
        << "inMesh deformation path should dirty extent\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.meshTopology)
        << "Skinned deformation must not dirty mesh topology\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

} // namespace

// ===========================================================================
// MeshDirtyLocators suite — requires MAYA_HYDRA_USE_MESH_ADAPTER=1
// ===========================================================================

// What: moving a vertex changes points only (deformation-style), not mesh topology.
// How: translate vtx[0] on the test cube and inspect scene index dirty locators.
// Expect: primvars/points; no mesh/topology, no broad primvars, no extComputationPrimvars.
TEST(MeshDirtyLocators, DeformationVertexMoveEmitsPointsNotTopology)
{
    RunDeformationVertexMoveTest();
}

// What: skinned inMesh dirty with unchanged point positions must emit primvars/points, not UV-only locators.
// How: skinned scene at frame 1, then dgdirty inMesh while point positions are unchanged.
// Expect: points + extent; no mesh topology.
// Regression: inMesh/pnts dirties must always emit points/extent without reading mesh data during
//             DG dirty propagation (prior logic could emit UV-only locators on stale skin inMesh).
TEST(MeshDirtyLocators, SkinnedMeshDeformationEmitsPoints)
{
    RunSkinnedMeshDeformationEmitsPointsTest();
}

// What: a topology edit dirties mesh topology and broad primvars (face-varying invalidation).
// How: extrude one face on the test cube and inspect dirty locators.
// Expect: mesh/topology, broad primvars, points; no extComputationPrimvars.
//         extComputationPrimvars is reserved for skinning/blendshape attribute changes,
//         not connectivity edits (see doc/render_delegate_topology_vs_deformation.md).
TEST(MeshDirtyLocators, TopologyExtrudeEmitsTopologyAndBroadPrimvars)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshShapeFull + ".f[0]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("polyExtrudeFacet -ltz 0.5 -ch 1"), MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.meshTopology)
        << "Face extrude should dirty mesh topology\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.broadPrimvars)
        << "Topology change should emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.extCompPrimvars)
        << "Topology change must not dirty extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.points)
        << "Topology change should also dirty primvars/points\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

// What: adding/setting a dynamic attribute on a mesh (rprim) emits extComputationPrimvars.
// How: addAttr+setAttr a dynamic float on the test cube shape; inspect dirty locators.
// Expect: extCompPrimvars=true.
TEST(MeshDirtyLocators, DynamicAttrOnMeshEmitsExtComputationPrimvars)
{
    RunDynamicAttrOnMeshEmitsExtComputationPrimvarsTest();
}

// What: editing UV coordinates emits only the granular primvars/st locator (no topology, no broad
//       primvars).  The uvPivot attribute is the designated trigger for UV-coord dirtying in the
//       mesh adapter's lambda table.
// How: set uvPivot on the test cube shape and inspect scene index dirty locators.
// Expect: uvs=true; meshTopology=false, broadPrimvars=false, points=false.
TEST(MeshDirtyLocators, UVEditEmitsGranularUVsOnly)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // uvPivot is a float2 attribute that fires the lambda in _dirtyNotifiers, which calls
    // dirtyUVs() only — the targeted path for UV-coordinate editing.
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeFull + ".uvPivot 0.1 0.1").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.uvs)
        << "UV pivot change should dirty primvars/st\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.meshTopology)
        << "UV edit must not dirty mesh topology\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "UV edit must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.points)
        << "UV edit must not dirty primvars/points\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

// What: toggling smooth-mesh preview emits displayStyle + topology + subdivisionTags locators,
//       but NOT the broad primvars locator.
// How: set displaySmoothMesh=2 on the test cube and inspect dirty locators.
// Expect: displayStyle=true, meshTopology=true, subdivisionTags=true, broadPrimvars=false.
//         (normals may or may not be set depending on useMayaNormals — not asserted here.)
TEST(MeshDirtyLocators, SmoothMeshToggleEmitsDisplayStyleAndTopology)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found (ensure MAYA_HYDRA_USE_MESH_ADAPTER=1)";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // displaySmoothMesh=2 enables Catmull-Clark smooth preview: refineLevel > 0, which
    // transitions subdivisionScheme and GetDisplayStyle().refineLevel simultaneously.
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeFull + ".displaySmoothMesh 2").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.displayStyle)
        << "Smooth mesh toggle should dirty displayStyle\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.meshTopology)
        << "Smooth mesh toggle should dirty mesh topology (subdivisionScheme flip)\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.subdivisionTags)
        << "Smooth mesh toggle should dirty subdivisionTags\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "Smooth mesh toggle must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);

    // Reset so the mesh is in a known state for tests that run after this one.
    MGlobal::executeCommand(("setAttr " + meshShapeFull + ".displaySmoothMesh 0").c_str());
    MGlobal::executeCommand("refresh");
}

// What: toggling intermediateObject dirties the visibility locator and updates the Hydra schema.
// How: set intermediateObject on/off on the test cube shape (mesh adapter NodeDirtiedCallback path).
// Expect: visibility locator on each toggle; schema matches intermediateObject state after refresh.
TEST(MeshDirtyLocators, IntermediateObjectToggleEmitsVisibilityAndUpdatesSchema)
{
    RunIntermediateObjectToggleVisibilityTest();
}

// What: visibility changes on an instanced shape dirty instancer + prototype visibility via
//       _InstancerNodeDirty when the master transform is toggled.
// How: duplicate -rr -ilf to create shape instancing, then toggle master transform visibility.
// Expect: instancer + visibility locators on each toggle; schema matches master visibility.
TEST(MeshDirtyLocators, InstancedTransformVisibilityToggleEmitsVisibilityAndUpdatesSchema)
{
    RunInstancedTransformVisibilityToggleTest();
}

// What: per-instance visibility on a duplicate transform dirty instancer locators only.
// How: duplicate -rr -ilf, hide/show the duplicate transform while master stays visible.
// Expect: instancer locators on each toggle; prototype visibility schema unchanged.
TEST(MeshDirtyLocators, InstancedNonMasterVisibilityToggleEmitsInstancerNotPrototypeVisibility)
{
    RunInstancedNonMasterVisibilityToggleTest();
}

// ===========================================================================
// RenderItemDirtyLocators suite — render items mode (no MAYA_HYDRA_USE_MESH_ADAPTER)
// ===========================================================================

// What: moving a vertex dirties granular primvars (points, st, …) but never the broad primvars
//       locator or mesh topology.  Maya may set MVS_changedTopo alongside MVS_changedGeometry,
//       but the adapter suppresses topology locators when vertex count is unchanged.
// How: translate vtx[0] on the test cube and inspect scene index dirty locators.
// Expect: points + uvs; no meshTopology, no broadPrimvars, no extComputationPrimvars.
TEST(RenderItemDirtyLocators, DeformationVertexMoveEmitsGranularPrimvarsNotBroadPrimvars)
{
    RunRenderItemDeformationVertexMoveTest();
}

// What: a topology edit (face extrude) dirties mesh topology but NOT broadPrimvars or
//       extComputationPrimvars.  The render item adapter intentionally skips both on topology
//       changes: broadPrimvars would defeat the granular useMayaNormals skip; extCompPrimvars
//       is reserved for skinning/blendshape computations, not connectivity changes.
// How: extrude one face on the test cube and inspect dirty locators.
// Expect: meshTopology=true, points=true; uvs/tangents may also appear when Maya sets
//         geomChanged alongside topoChanged (vertex buffers re-read), but not from the
//         topology locator helper itself. broadPrimvars=false, extCompPrimvars=false.
TEST(RenderItemDirtyLocators, TopologyExtrudeEmitsTopologyNotBroadPrimvars)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshShapeFull + ".f[0]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("polyExtrudeFacet -ltz 0.5 -ch 1"), MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.meshTopology)
        << "Face extrude should dirty mesh topology\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.points)
        << "Topology change (vertex count change) should also dirty primvars/points\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    // UVs/tangents may appear when geomChanged accompanies topoChanged (buffer re-read).
    // The topology helper itself emits topology locators only.
    // Render item adapter intentionally does NOT emit broadPrimvars on topology changes.
    EXPECT_FALSE(signals.broadPrimvars)
        << "Render item topology change must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    // extComputationPrimvars is reserved for skinning/blendshapes, not topology.
    EXPECT_FALSE(signals.extCompPrimvars)
        << "Render item topology change must not dirty extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

// What: adding/setting a dynamic attribute on a mesh (rprim) emits extComputationPrimvars.
//       The render item adapter's attribute-changed callback calls
//       MaybeMarkPrimvarDirtyForAttributeChange, which gates on IsRprimTypeSupportedForPrim()
//       — same as the mesh
//       adapter path — so the behavior is identical in both modes.
// How: addAttr+setAttr a dynamic float on the test cube shape; inspect dirty locators.
// Expect: extCompPrimvars=true.
TEST(RenderItemDirtyLocators, DynamicAttrOnMeshEmitsExtComputationPrimvars)
{
    RunDynamicAttrOnMeshEmitsExtComputationPrimvarsTest();
}

// What: intermediateObject toggle dirties visibility and updates the Hydra schema (render items path).
// How: set intermediateObject on/off; Maya pushes MVS_changedVisibility through UpdateFromDelta,
//       which updates _visible before dirtyVisibility() — no dag-adapter visibility cache.
// Expect: visibility locator on each toggle; schema matches intermediateObject after refresh.
TEST(RenderItemDirtyLocators, IntermediateObjectToggleEmitsVisibilityAndUpdatesSchema)
{
    RunIntermediateObjectToggleVisibilityTest();
}

// What: connectivity change with unchanged position-buffer vertex count must dirty mesh topology.
// How: polyFlipEdge on a cube edge (render items mode); Maya may flag topo+geom together.
// Expect: meshTopology=true; no broadPrimvars. Complements DeformationVertexMove (same count,
//         unchanged connectivity → no meshTopology) and TopologyExtrude (count changes).
TEST(RenderItemDirtyLocators, ConnectivityChangeSameVertexCountEmitsTopology)
{
    RunRenderItemConnectivityChangeSameVertexCountTest();
}

// What: editing UV coordinates via the render item geomChanged path dirties primvars/st together
//       with primvars/points (vertex buffers are re-read as a batch).  Unlike the mesh adapter,
//       built-in attributes such as uvPivot do not reach the render item adapter — UV edits must
//       go through MRenderItem geometry updates triggered by component transforms.
// How: select UV components and polyEditUV, then inspect dirty locators.
// Expect: uvs + points; no broadPrimvars, no extComputationPrimvars.
TEST(RenderItemDirtyLocators, UVEditViaGeomChangedEmitsPointsAndUVsNotTopology)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    const std::string meshTransform
        = GetOptionVarOrDefault(kMeshTransformOptionVar, kMeshTransformFallback);
    // UV components live on the transform; use the short name — MEL rejects wildcards like map[*]
    // and long paths such as |testCube.map[…] with a syntax error.
    const std::string meshTransformName = GetShapeNameFromFullPath(meshTransform);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // polyEditUV with an object argument alone does not push an MRenderItem delta; select UV
    // components first, then edit in UV space.
    // Default polyCube has 24 map vertices (6 faces × 4 corners).
    ASSERT_EQ(MGlobal::executeCommand(
                  ("select -r " + meshTransformName + ".map[0:23]").c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("polyEditUV -u 0.1 -v 0.1 -relative true"),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.uvs)
        << "UV edit should dirty primvars/st\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_TRUE(signals.points)
        << "Render item geomChanged re-reads vertex buffers together; points must also dirty\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "UV edit must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.extCompPrimvars)
        << "UV edit must not dirty extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
}

// What: toggling smooth-mesh preview updates the render item topology (subdivisionScheme flip)
//       via the topoChanged path.  The render item adapter does NOT listen to displaySmoothMesh
//       as a DAG attribute — Maya pushes the subdivided geometry through UpdateFromDelta instead.
//       Unlike the mesh adapter, this path emits topology + face-varying primvar locators only;
//       displayStyle and subdivisionTags are mesh-adapter concerns.
// How: set displaySmoothMesh=2 on the test cube and inspect dirty locators.
// Expect: meshTopology; no broadPrimvars, no displayStyle, no subdivisionTags.
//         UV/tangent locators are not emitted on the topology path (geomChanged may add them).
TEST(RenderItemDirtyLocators, SmoothMeshToggleEmitsTopologyNotDisplayStyle)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeFallback);
    SdfPath meshPrimPath;
    HdSceneIndexBaseRefPtr mayaSceneIndex;
    ASSERT_TRUE(TryFindMeshPrim(meshShapeFull, &meshPrimPath, &mayaSceneIndex))
        << "Mesh prim not found";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // displaySmoothMesh=2 enables Catmull-Clark smooth preview; Maya regenerates the render item
    // geometry, which the adapter receives as topoChanged (and possibly geomChanged).
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + meshShapeFull + ".displaySmoothMesh 2").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, meshPrimPath);

    EXPECT_TRUE(signals.anyForPrim) << "Expected dirty notices for the mesh prim";
    EXPECT_TRUE(signals.meshTopology)
        << "Smooth mesh toggle should dirty mesh topology (subdivisionScheme flip)\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.broadPrimvars)
        << "Render item smooth mesh toggle must not emit the broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.displayStyle)
        << "Render item path does not emit displayStyle on smooth mesh toggle\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);
    EXPECT_FALSE(signals.subdivisionTags)
        << "Render item path does not emit subdivisionTags on smooth mesh toggle\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, meshPrimPath);

    // Reset so the mesh is in a known state for tests that run after this one.
    MGlobal::executeCommand(("setAttr " + meshShapeFull + ".displaySmoothMesh 0").c_str());
    MGlobal::executeCommand("refresh");
}

// ===========================================================================
// ExtCompGate suite — mode-agnostic, runs under both Python wrappers
// ===========================================================================

// What: adding/setting a dynamic attribute on a camera (sprim) must NOT emit extComputationPrimvars.
//       The gate in _maybeDirtyExtComputationPrimvars must stay closed for non-rprim prim types.
// How: addAttr+setAttr a dynamic float on a scene camera; inspect dirty locators for that prim.
// Expect: extCompPrimvars=false.  Attribute is removed at the end to avoid polluting other tests.
TEST(ExtCompGate, DynamicAttrOnCameraSkipsExtComputationPrimvars)
{
    // Default perspShape is always present after file -new; invisible cameras are still
    // translated because they can be renderable (see InsertDag camera handling).
    const std::string cameraShapeFull
        = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeFallback);
    const std::string camShapeName = GetShapeNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera = FindTerminalSceneIndexWithPrim(
        sceneIndices, camShapeName, HdPrimTypeTokens->camera);
    ASSERT_TRUE(sceneIndexWithCamera)
        << camShapeName << " camera prim not found in any scene index";

    HdSceneIndexBaseRefPtr mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(camShapeName, HdPrimTypeTokens->camera), 1);
    ASSERT_GE(found.size(), 1u) << camShapeName << " not found in MayaHydraSceneIndex";
    const SdfPath camPrimPath = found.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_EQ(MGlobal::executeCommand(
                  ("addAttr -ln testExtCompGateAttr -at double " + camShapeName).c_str()),
              MStatus::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand(
                  ("setAttr " + camShapeName + ".testExtCompGateAttr 1.0").c_str()),
              MStatus::kSuccess);
    MGlobal::executeCommand("refresh");

    // Reuse ClassifyMeshDirtySince on the camera path: extCompPrimvars classification is
    // prim-type-agnostic, so this correctly reports whether the locator was emitted.
    const MeshDirtySignals signals
        = ClassifyMeshDirtySince(notifsAccumulator, startIndex, camPrimPath);

    EXPECT_FALSE(signals.extCompPrimvars)
        << "Dynamic attribute change on a camera (sprim) must NOT emit extComputationPrimvars\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, camPrimPath);
    EXPECT_TRUE(signals.broadPrimvars)
        << "Dynamic attribute change on a camera (sprim) must emit broad primvars locator\n"
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex, camPrimPath);

    MGlobal::executeCommand(("deleteAttr " + camShapeName + ".testExtCompGateAttr").c_str());
    MGlobal::executeCommand("refresh");
}
