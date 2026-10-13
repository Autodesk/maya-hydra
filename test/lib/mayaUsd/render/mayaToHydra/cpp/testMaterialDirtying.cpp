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

#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeParameterSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kMaterialName = "dirtyMaterial";
const char* kMaterialSgName = "dirtyMaterialSG";

const char* kDynamicAttrName = "extDirty";

bool HasDirtySince(
    const SceneIndexNotificationsAccumulator& accumulator,
    size_t                                    startIndex,
    const std::string&                        primName,
    const HdDataSourceLocator&                locator)
{
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath.GetName() == primName
            && entries[i].dirtyLocators.Intersects(locator)) {
            return true;
        }
    }
    return false;
}

// Find the MayaHydraSceneIndex holding the material prim and observe it.
HdSceneIndexBaseRefPtr FindMayaSceneIndexWithMaterial()
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    if (sceneIndices.empty()) {
        return nullptr;
    }
    auto terminalSceneIndex
        = FindTerminalSceneIndexWithPrim(sceneIndices, PrimNamePredicate(kMaterialSgName));
    return terminalSceneIndex ? FindMayaHydraSceneIndex(terminalSceneIndex) : nullptr;
}

bool ExecuteAndRefresh(const std::string& melCmd)
{
    const std::string cmd = melCmd + " refresh;";
    return MGlobal::executeCommand(cmd.c_str()) == MStatus::kSuccess;
}

// Drive a shader input from a new multiplyDivide node. Changing the multiplyDivide afterwards
// sends a node-dirty on the shader input without an attribute-changed message on the shader,
// which is what an Attribute Editor slider drag sends until the drag ends. (dgdirty does not send
// node-dirty plug messages, so it cannot be used for this.)
bool ConnectUpstreamNode(
    const std::string& sourceNode,
    const std::string& sourcePlug,
    const std::string& shaderAttr)
{
    return ExecuteAndRefresh(
        "createNode multiplyDivide -n " + sourceNode + "; connectAttr -f " + sourceNode + "."
        + sourcePlug + " " + kMaterialName + "." + shaderAttr + ";");
}

// Read the first diffuseColor parameter found in the material network of the material prim.
VtValue GetMaterialDiffuseColor(const HdSceneIndexBaseRefPtr& sceneIndex)
{
    SceneIndexInspector inspector(sceneIndex);
    const PrimEntriesVector foundPrims
        = inspector.FindPrims(PrimNamePredicate(kMaterialSgName), 1);
    if (foundPrims.empty()) {
        return {};
    }

    const HdSceneIndexPrim prim = sceneIndex->GetPrim(foundPrims.front().primPath);
    HdMaterialNodeContainerSchema nodes
        = HdMaterialSchema::GetFromParent(prim.dataSource).GetMaterialNetwork().GetNodes();
    static const TfToken diffuseColorToken("diffuseColor");
    for (const TfToken& nodeName : nodes.GetNames()) {
        HdSampledDataSourceHandle valueDs
            = nodes.Get(nodeName).GetParameters().Get(diffuseColorToken).GetValue();
        if (valueDs) {
            return valueDs->GetValue(0.0f);
        }
    }
    return {};
}

} // namespace

// What: a node-dirty on a surface shader input, without an attribute-changed message, should
//       dirty the material. Dragging an Attribute Editor slider sends only node-dirty until the
//       drag ends, so this is what keeps the viewport live during the drag.
// How: drive the shader color from a multiplyDivide node, change the multiplyDivide (node-dirty
//      only on the shader), refresh, then inspect dirtied entries.
// Expect: the shading group material prim gets a material-locator dirty.
TEST(MaterialDirtying, testShaderNodeDirtyDirtiesMaterial)
{
    auto mayaSceneIndex = FindMayaSceneIndexWithMaterial();
    ASSERT_TRUE(mayaSceneIndex) << "Material prim " << kMaterialSgName << " not found";
    ASSERT_TRUE(ConnectUpstreamNode("dirtyColorSource", "output", "color"));

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_TRUE(ExecuteAndRefresh("setAttr dirtyColorSource.input1X 0.5;"));

    EXPECT_TRUE(HasDirtySince(
        notifsAccumulator, startIndex, kMaterialSgName, HdMaterialSchema::GetDefaultLocator()))
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex);
}

// What: setting the shader color should update the color in the Hydra material network.
// How: set the lambert diffuse to 1 and its color to red, refresh, then read the material network
//      of the material prim. The lambert is translated to a UsdPreviewSurface whose diffuseColor
//      is color * diffuse.
// Expect: diffuseColor is red.
TEST(MaterialDirtying, testShaderColorUpdatesMaterialNetwork)
{
    auto mayaSceneIndex = FindMayaSceneIndexWithMaterial();
    ASSERT_TRUE(mayaSceneIndex) << "Material prim " << kMaterialSgName << " not found";

    const std::string cmd = std::string("setAttr \"") + kMaterialName + ".diffuse\" 1.0; "
        + "setAttr \"" + kMaterialName + ".color\" -type \"double3\" 1 0 0; refresh;";
    ASSERT_EQ(MGlobal::executeCommand(cmd.c_str()), MStatus::kSuccess);

    const VtValue diffuseColor = GetMaterialDiffuseColor(mayaSceneIndex);
    ASSERT_TRUE(diffuseColor.IsHolding<GfVec3f>())
        << "diffuseColor not found in the material network, or not a GfVec3f: " << diffuseColor;
    EXPECT_TRUE(GfIsClose(diffuseColor.UncheckedGet<GfVec3f>(), GfVec3f(1.0f, 0.0f, 0.0f), 1e-6))
        << "Expected red, got " << diffuseColor.UncheckedGet<GfVec3f>();
}

// What: a node-dirty on a dynamic attribute of the surface shader, without an attribute-changed
//       message, should dirty primvars only: such attributes are not part of the material network.
// How: drive a dynamic attribute of the shader from a multiplyDivide node, change the
//      multiplyDivide (node-dirty only on the shader), refresh, then inspect dirtied entries.
// Expect: the shading group material prim gets a primvars dirty and no material dirty.
TEST(MaterialDirtying, testShaderDynamicAttrNodeDirtyDirtiesPrimvarsOnly)
{
    auto mayaSceneIndex = FindMayaSceneIndexWithMaterial();
    ASSERT_TRUE(mayaSceneIndex) << "Material prim " << kMaterialSgName << " not found";
    ASSERT_TRUE(ConnectUpstreamNode("dirtyExtSource", "outputX", kDynamicAttrName));

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_TRUE(ExecuteAndRefresh("setAttr dirtyExtSource.input1X 0.5;"));

    EXPECT_TRUE(HasDirtySince(
        notifsAccumulator, startIndex, kMaterialSgName, HdPrimvarsSchema::GetDefaultLocator()))
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex);
    EXPECT_FALSE(HasDirtySince(
        notifsAccumulator, startIndex, kMaterialSgName, HdMaterialSchema::GetDefaultLocator()))
        << DescribeDirtyPrimEntriesSince(notifsAccumulator, startIndex);
}
