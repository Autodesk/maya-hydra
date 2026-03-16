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

#include <mayaHydraLib/adapters/cameraAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kCameraShapeOptionVar = "mhCameraShape";
const char* kCameraShapeFallback = "cameraShape1";

std::string GetOptionVarOrDefault(const char* optionVar, const char* fallback)
{
    if (MGlobal::optionVarExists(optionVar)) {
        return MGlobal::optionVarStringValue(optionVar).asChar();
    }
    return fallback;
}

FindPrimPredicate getCameraPredicate(const std::string& shapeNamePart)
{
    return [shapeNamePart](const HdSceneIndexBasePtr& sceneIndex,
                          const SdfPath&            primPath) -> bool {
        if (primPath.GetAsString().find(shapeNamePart) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == HdPrimTypeTokens->camera;
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

void CheckCameraDirtySince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      cameraPrimPath,
    bool&                               outHadPrimvarsDirty,
    bool&                               outHadCameraSchemaDirty)
{
    outHadPrimvarsDirty = false;
    outHadCameraSchemaDirty = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto  cameraSchemaLocator = HdCameraSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != cameraPrimPath) {
            continue;
        }
        if (entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            outHadPrimvarsDirty = true;
        }
        if (entries[i].dirtyLocators.Intersects(cameraSchemaLocator)) {
            outHadCameraSchemaDirty = true;
        }
    }
}

size_t CountPrimvarsDirtyEntriesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      cameraPrimPath)
{
    size_t count = 0;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath == cameraPrimPath
            && entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            ++count;
        }
    }
    return count;
}

} // namespace

// Verify that changing a non-param attribute (custom attr) triggers ONLY DirtyPrimvar,
// not DirtyParams. Param attrs trigger both; primvar-only attrs must not.
TEST(CameraPrimvars, NonParamAttrTriggersOnlyPrimvarDirty)
{
    const std::string cameraShapeFull = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeFallback);
    const std::string shapeNamePart = getShapeNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera(nullptr);
    for (const HdSceneIndexBaseRefPtr& si : sceneIndices) {
        SceneIndexInspector inspector(si);
        PrimEntriesVector   foundPrims = inspector.FindPrims(getCameraPredicate(shapeNamePart), 1);
        if (foundPrims.size() >= 1u) {
            sceneIndexWithCamera = si;
            break;
        }
    }
    ASSERT_TRUE(sceneIndexWithCamera) << "Camera prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(getCameraPredicate(shapeNamePart), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "Camera not found in MayaHydraSceneIndex";
    const SdfPath cameraPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change testCustomAttr (not in param attr list - primvar-only); Python adds this attr
    MGlobal::executeCommand(
        ("setAttr \"" + cameraShapeFull + ".testCustomAttr\" 0.5").c_str());
    MGlobal::executeCommand("refresh");

    bool hadPrimvarsDirty = false;
    bool hadCameraSchemaDirty = false;
    CheckCameraDirtySince(
        notifsAccumulator, startIndex, cameraPrimPath, hadPrimvarsDirty, hadCameraSchemaDirty);

    EXPECT_TRUE(hadPrimvarsDirty)
        << "Changing testCustomAttr (non-param-attr) should trigger DirtyPrimvar";
    EXPECT_FALSE(hadCameraSchemaDirty)
        << "Changing testCustomAttr (non-param-attr) must NOT trigger DirtyParams - "
           "only primvars dirty expected";
}

// Verify that updating focalLength (param attr) does not produce duplicate
// primvars dirty notifications - we should receive exactly one.
TEST(CameraPrimvars, FocalLengthUpdateNoDuplicatePrimvarsDirty)
{
    const std::string cameraShapeFull = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeFallback);
    const std::string shapeNamePart = getShapeNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera(nullptr);
    for (const HdSceneIndexBaseRefPtr& si : sceneIndices) {
        SceneIndexInspector inspector(si);
        PrimEntriesVector   foundPrims = inspector.FindPrims(getCameraPredicate(shapeNamePart), 1);
        if (foundPrims.size() >= 1u) {
            sceneIndexWithCamera = si;
            break;
        }
    }
    ASSERT_TRUE(sceneIndexWithCamera) << "Camera prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(getCameraPredicate(shapeNamePart), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "Camera not found in MayaHydraSceneIndex";
    const SdfPath cameraPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change focalLength (param attribute)
    MGlobal::executeCommand(
        ("setAttr \"" + cameraShapeFull + ".focalLength\" 100").c_str());
    MGlobal::executeCommand("refresh");

    const size_t primvarsDirtyCount = CountPrimvarsDirtyEntriesSince(
        notifsAccumulator, startIndex, cameraPrimPath);

    EXPECT_EQ(primvarsDirtyCount, 1u)
        << "Updating focalLength (param attr) should trigger exactly one primvars dirty "
           "notification, got " << primvarsDirtyCount << " (duplicate notifications)";
}

// Consistency check: every attribute read by GetCameraParamValue must be in the
// camera param attribute list (kCameraParamAttributeNames).
TEST(CameraPrimvars, ParamAttributesMatchGetLogic)
{
    static const std::vector<std::string> kAttrsUsedInGetLogic = {
        "nearClipPlane", "farClipPlane", "shutterAngle", "focusDistance", "focalLength",
        "fStop", "horizontalFilmAperture", "verticalFilmAperture", "lensSqueezeRatio",
        "shakeEnabled", "horizontalFilmOffset", "horizontalShake", "verticalFilmOffset",
        "verticalShake", "filmFit", "depthOfField", "orthographic",
    };

    const auto& paramAttrs = MayaHydraCameraAdapter::GetCameraParamAttributeNamesForTest();
    for (const std::string& attr : kAttrsUsedInGetLogic) {
        EXPECT_TRUE(paramAttrs.count(attr)) << "Attribute '" << attr
            << "' is used in GetCameraParamValue but is missing from "
               "kCameraParamAttributeNames in cameraAdapter.cpp.";
    }
}
