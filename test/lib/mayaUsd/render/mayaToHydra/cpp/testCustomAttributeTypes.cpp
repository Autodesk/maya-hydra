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

#include <mayaHydraLib/mayaUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MAngle.h>
#include <maya/MDataHandle.h>
#include <maya/MDistance.h>
#include <maya/MDoubleArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFloatArray.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MIntArray.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MMatrixArray.h>
#include <maya/MPoint.h>
#include <maya/MPointArray.h>
#include <maya/MStringArray.h>
#include <maya/MTime.h>
#include <maya/MVector.h>
#include <maya/MVectorArray.h>

#include <gtest/gtest.h>

#include <iostream>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

// Unit test: verifies custom Maya extension attributes are translated into Hydra primvars.
namespace {

// Build a predicate to find a prim by name and type.
FindPrimPredicate getPrimPredicate(const std::string& primName, const TfToken& primType)
{
    return [primName,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetAsString().find(primName) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == primType;
    };
}

// Convert an MMatrix to a GfMatrix4d.
GfMatrix4d ToGfMatrix(const MMatrix& mayaMat)
{
    GfMatrix4d mat;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 4; ++col) {
            mat[row][col] = mayaMat[row][col];
        }
    }
    return mat;
}

// Assert that a primvar exists and matches the expected value.
template <typename T>
void ExpectPrimvarValue(const HdSceneIndexPrim& prim, const TfToken& name, const T& expected)
{
    HdSampledDataSourceHandle valueDs
        = HdPrimvarsSchema::GetFromParent(prim.dataSource).GetPrimvar(name).GetPrimvarValue();
    if (!valueDs) {
        std::cout << "Missing primvar: " << name.GetText() << std::endl;
        HdContainerDataSourceHandle primvarsDs = HdContainerDataSource::Cast(
            HdContainerDataSource::Get(prim.dataSource, HdPrimvarsSchema::GetDefaultLocator()));
        std::cout << "Primvar names: ";
        if (!primvarsDs) {
            std::cout << "<none>" << std::endl;
        } else {
            TfTokenVector names = primvarsDs->GetNames();
            for (const TfToken& token : names) {
                std::cout << token.GetText() << " ";
            }
            std::cout << std::endl;
        }
    }
    ASSERT_TRUE(valueDs);
    VtValue value = valueDs->GetValue(0.0f);
    ASSERT_TRUE(value.IsHolding<T>());
    EXPECT_EQ(value.UncheckedGet<T>(), expected);
}

// Fetch a typed attribute plug if present.
MPlug GetTypedAttribute(MFnDependencyNode& node, const char* name)
{
    MStatus status;
    MPlug   plug = node.findPlug(name, true, &status);
    if (status) {
        return plug;
    }
    return MPlug();
}

} // namespace

// Validate extension attribute values are translated into primvars.
TEST(CustomAttributes, extensionAttributeTypes)
{
    MObject cubeNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("pCubeShape1", cubeNode));

    // Values are authored in testCustomAttributes.py. This test only verifies
    // the Hydra translation against the expected values.
    MMatrix typedMatrix;
    typedMatrix[0][0] = 1.0;
    typedMatrix[1][1] = 2.0;
    typedMatrix[2][2] = 3.0;
    typedMatrix[3][3] = 1.0;
    typedMatrix[3][0] = 4.0;
    typedMatrix[3][1] = 5.0;
    typedMatrix[3][2] = 6.0;

    MMatrix arrayMatrix;
    arrayMatrix[0][0] = 2.0;
    arrayMatrix[1][1] = 2.0;
    arrayMatrix[2][2] = 2.0;
    arrayMatrix[3][3] = 1.0;
    arrayMatrix[3][0] = 7.0;
    arrayMatrix[3][1] = 8.0;
    arrayMatrix[3][2] = 9.0;

    MMatrix matrixAttrValue;
    matrixAttrValue[0][0] = 9.0;
    matrixAttrValue[1][1] = 8.0;
    matrixAttrValue[2][2] = 7.0;
    matrixAttrValue[3][3] = 1.0;
    matrixAttrValue[3][0] = 2.0;
    matrixAttrValue[3][1] = 3.0;
    matrixAttrValue[3][2] = 4.0;

    MGlobal::executeCommand("dgdirty -a");
    MGlobal::executeCommand("refresh");

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexPrim prim;
    bool             testPassed = false;
    for (const HdSceneIndexBaseRefPtr& sceneIndex : sceneIndices) {
        SceneIndexInspector inspector(sceneIndex);
        PrimEntriesVector foundPrims
            = inspector.FindPrims(getPrimPredicate("pCube1", HdPrimTypeTokens->mesh));
        if (foundPrims.size() == 1u) {
            prim = foundPrims.front().prim;
            ASSERT_EQ(prim.primType, HdPrimTypeTokens->mesh);
            ASSERT_NE(prim.dataSource, nullptr);
            testPassed = true;
            break;
        }
    }
    ASSERT_TRUE(testPassed);

    ExpectPrimvarValue(prim, TfToken("extEnum"), static_cast<short>(2));
    ExpectPrimvarValue(prim, TfToken("extString"), std::string("hello"));

    VtStringArray expectedStringArray;
    expectedStringArray.push_back("alpha");
    expectedStringArray.push_back("beta");
    ExpectPrimvarValue(prim, TfToken("extStringArray"), expectedStringArray);

    VtIntArray expectedIntArray;
    expectedIntArray.push_back(1);
    expectedIntArray.push_back(2);
    expectedIntArray.push_back(3);
    ExpectPrimvarValue(prim, TfToken("extIntArray"), expectedIntArray);

    VtFloatArray expectedFloatArray;
    expectedFloatArray.push_back(1.25f);
    expectedFloatArray.push_back(2.5f);
    ExpectPrimvarValue(prim, TfToken("extFloatArray"), expectedFloatArray);

    VtDoubleArray expectedDoubleArray;
    expectedDoubleArray.push_back(3.5);
    expectedDoubleArray.push_back(4.25);
    ExpectPrimvarValue(prim, TfToken("extDoubleArray"), expectedDoubleArray);

    VtArray<GfVec3d> expectedVectorArray;
    expectedVectorArray.push_back(GfVec3d(1.0, 2.0, 3.0));
    expectedVectorArray.push_back(GfVec3d(4.0, 5.0, 6.0));
    ExpectPrimvarValue(prim, TfToken("extVectorArray"), expectedVectorArray);

    VtArray<GfVec4d> expectedPointArray;
    expectedPointArray.push_back(GfVec4d(1.0, 2.0, 3.0, 1.0));
    expectedPointArray.push_back(GfVec4d(4.0, 5.0, 6.0, 1.0));
    ExpectPrimvarValue(prim, TfToken("extPointArray"), expectedPointArray);

    ExpectPrimvarValue(prim, TfToken("extMatrix"), ToGfMatrix(typedMatrix));

    VtArray<GfMatrix4d> expectedMatrixArray;
    expectedMatrixArray.push_back(ToGfMatrix(MMatrix::identity));
    expectedMatrixArray.push_back(ToGfMatrix(arrayMatrix));
    ExpectPrimvarValue(prim, TfToken("extMatrixArray"), expectedMatrixArray);

    ExpectPrimvarValue(prim, TfToken("extBool"), true);
    ExpectPrimvarValue(prim, TfToken("extByte"), 7);
    ExpectPrimvarValue(prim, TfToken("extShort"), static_cast<short>(12));
    ExpectPrimvarValue(prim, TfToken("extInt"), 42);
    ExpectPrimvarValue(prim, TfToken("extFloat"), 1.75f);
    ExpectPrimvarValue(prim, TfToken("extDouble"), 2.25);
    ExpectPrimvarValue(prim, TfToken("extInt64"), static_cast<MInt64>(9876543210LL));
    ExpectPrimvarValue(prim, TfToken("extAddr"), static_cast<MInt64>(0x1234));

    ExpectPrimvarValue(prim, TfToken("extShort2"), GfVec2i(3, 4));
    ExpectPrimvarValue(prim, TfToken("extShort3"), GfVec3i(5, 6, 7));
    ExpectPrimvarValue(prim, TfToken("extInt2"), GfVec2i(8, 9));
    ExpectPrimvarValue(prim, TfToken("extInt3"), GfVec3i(10, 11, 12));
    ExpectPrimvarValue(prim, TfToken("extFloat2"), GfVec2f(1.1f, 2.2f));
    ExpectPrimvarValue(prim, TfToken("extFloat3"), GfVec3f(3.3f, 4.4f, 5.5f));
    ExpectPrimvarValue(prim, TfToken("extDouble2"), GfVec2d(6.6, 7.7));
    ExpectPrimvarValue(prim, TfToken("extDouble3"), GfVec3d(8.8, 9.9, 10.1));
    ExpectPrimvarValue(prim, TfToken("extDouble4"), GfVec4d(11.1, 12.2, 13.3, 14.4));

    ExpectPrimvarValue(prim, TfToken("extAngle"), 0.75);
    ExpectPrimvarValue(prim, TfToken("extDistance"), 2.5);
    ExpectPrimvarValue(prim, TfToken("extTime"), 1.25);

    ExpectPrimvarValue(prim, TfToken("extMatrixAttr"), ToGfMatrix(matrixAttrValue));
}

// Validate typed numeric attributes are translated into primvars.
TEST(CustomAttributes, extensionAttributeTypedNumeric)
{
    MObject cubeNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("pCubeShape1", cubeNode));
    MFnDependencyNode depNode(cubeNode);

    MPlug numericPlug = GetTypedAttribute(depNode, "extNumeric2Float");
    if (numericPlug.isNull()) {
        GTEST_SKIP() << "Typed kNumeric attributes are not supported in this Maya build.";
    }

    MStatus     status;
    MDataHandle handle = numericPlug.asMDataHandle(&status);
    ASSERT_TRUE(status);
    handle.set(0.5f, 1.5f);
    EXPECT_TRUE(numericPlug.setValue(handle));

    MGlobal::executeCommand("dgdirty -a");
    MGlobal::executeCommand("refresh");

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexPrim prim;
    bool             testPassed = false;
    for (const HdSceneIndexBaseRefPtr& sceneIndex : sceneIndices) {
        SceneIndexInspector inspector(sceneIndex);
        PrimEntriesVector foundPrims
            = inspector.FindPrims(getPrimPredicate("pCube1", HdPrimTypeTokens->mesh));
        if (foundPrims.size() == 1u) {
            prim = foundPrims.front().prim;
            ASSERT_EQ(prim.primType, HdPrimTypeTokens->mesh);
            ASSERT_NE(prim.dataSource, nullptr);
            testPassed = true;
            break;
        }
    }
    ASSERT_TRUE(testPassed);

    ExpectPrimvarValue(prim, TfToken("extNumeric2Float"), GfVec2f(0.5f, 1.5f));
}
