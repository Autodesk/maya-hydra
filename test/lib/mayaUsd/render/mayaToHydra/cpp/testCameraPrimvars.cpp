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

// Scan dirty entries and report whether primvars or camera schema were dirtied.
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

// Count primvars-dirty notices for the camera prim since a starting index.
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

// What: non-param attribute changes should dirty primvars only.
// How: set a custom camera attribute and inspect notices since the change.
// Expect: primvars dirty is present; camera schema dirty is absent.
TEST(CameraPrimvars, NonParamAttrTriggersOnlyPrimvarDirty)
{
    const std::string cameraShapeFull = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->camera);
    ASSERT_TRUE(sceneIndexWithCamera) << "Camera prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->camera), 1);
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

// What: param attribute updates should not duplicate primvars dirty notices.
// How: change focalLength and count primvars dirty entries since the change.
// Expect: exactly one primvars dirty entry for the camera.
TEST(CameraPrimvars, FocalLengthUpdateNoDuplicatePrimvarsDirty)
{
    const std::string cameraShapeFull = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->camera);
    ASSERT_TRUE(sceneIndexWithCamera) << "Camera prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->camera), 1);
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

// What: camera param attributes list must cover GetCameraParamValue usage.
// How: compare the expected list against the adapter's param attribute set.
// Expect: every attribute used in GetCameraParamValue is present in the set.
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
