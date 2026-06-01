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

// HYDRA-2028: light and camera leaf params were always served by Get() but not
// advertised by GetNames(), so generic traversal (e.g. the Hydra Scene Browser)
// could not discover them. These tests walk the light/camera container's
// GetNames(), assert the newly-advertised leaf params are present, and that
// Get() returns a non-null, correctly-typed data source for each — i.e. that
// every advertised name is also served (no phantom entries) and that the typed
// routing is schema-conformant (notably shutterOpen/shutterClose as double).

#include "testUtils.h"

#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool Contains(const TfTokenVector& names, const TfToken& token)
{
    return std::find(names.begin(), names.end(), token) != names.end();
}

// Retrieve the child container data source at `key` of a prim's data source.
HdContainerDataSourceHandle GetChildContainer(
    const HdContainerDataSourceHandle& parent,
    const TfToken&                     key)
{
    if (!parent) {
        return nullptr;
    }
    return HdContainerDataSource::Cast(parent->Get(key));
}

} // namespace

// What: the light container advertises its leaf params and serves each one.
// How: create a directional light, reach its `light` container, walk GetNames().
// Expect: every new leaf token is advertised AND Get() returns a non-null typed
//         data source of the schema-conformant type (no phantom rows).
TEST(DataSourceNames, LightContainerAdvertisesAndServesLeafParams)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    // Under Storm a Maya directional light is translated to a simpleLight prim.
    HdSceneIndexBaseRefPtr sceneIndexWithLight = FindTerminalSceneIndexWithPrim(
        sceneIndices, "directionalLightShape", HdPrimTypeTokens->simpleLight);
    ASSERT_TRUE(sceneIndexWithLight) << "directionalLight prim not found in any scene index";

    // Inspect the MayaHydraSceneIndex directly so we read our data source output
    // unfiltered by any downstream terminal scene index.
    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithLight);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(
        CreatePrimPredicate("directionalLightShape", HdPrimTypeTokens->simpleLight), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "directionalLight not found in MayaHydraSceneIndex";

    HdSceneIndexPrim prim = foundPrims.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    HdContainerDataSourceHandle lightContainer
        = GetChildContainer(prim.dataSource, HdLightSchemaTokens->light);
    ASSERT_TRUE(lightContainer) << "light container data source missing on light prim";

    const TfTokenVector names = lightContainer->GetNames();

    // Each leaf param must be advertised and serve a non-null, correctly-typed value.
    const std::vector<TfToken> vec3fParams = {
        HdLightTokens->color,
        HdLightTokens->shadowColor,
    };
    const std::vector<TfToken> floatParams = {
        HdLightTokens->intensity,
        HdLightTokens->exposure,
        HdLightTokens->diffuse,
        HdLightTokens->specular,
        HdLightTokens->colorTemperature,
    };
    const std::vector<TfToken> boolParams = {
        HdLightTokens->normalize,
        HdLightTokens->enableColorTemperature,
        HdLightTokens->shadowEnable,
    };

    for (const TfToken& param : vec3fParams) {
        EXPECT_TRUE(Contains(names, param))
            << "light leaf param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdVec3fDataSource::Cast(lightContainer->Get(param));
        EXPECT_TRUE(ds) << "light leaf param '" << param.GetText()
                        << "' should be served as a GfVec3f data source";
    }
    for (const TfToken& param : floatParams) {
        EXPECT_TRUE(Contains(names, param))
            << "light leaf param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdFloatDataSource::Cast(lightContainer->Get(param));
        EXPECT_TRUE(ds) << "light leaf param '" << param.GetText()
                        << "' should be served as a float data source";
    }
    for (const TfToken& param : boolParams) {
        EXPECT_TRUE(Contains(names, param))
            << "light leaf param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdBoolDataSource::Cast(lightContainer->Get(param));
        EXPECT_TRUE(ds) << "light leaf param '" << param.GetText()
                        << "' should be served as a bool data source";
    }

    // Value check: the scene sets the light color to a known non-default value.
    auto colorDs = HdVec3fDataSource::Cast(lightContainer->Get(HdLightTokens->color));
    ASSERT_TRUE(colorDs);
    const GfVec3f color = colorDs->GetTypedValue(0.0f);
    EXPECT_NEAR(color[0], 0.25f, 1e-5f);
    EXPECT_NEAR(color[1], 0.50f, 1e-5f);
    EXPECT_NEAR(color[2], 0.75f, 1e-5f);
}

// What: the camera container advertises the additional params and serves each
//       one with the schema-conformant type — float for fStop/focusDistance,
//       double for shutterOpen/shutterClose.
// How: reach the persp camera's `camera` container, walk GetNames(), and cast
//      each served data source to its expected typed handle.
// Expect: all five new names are advertised; shutterOpen/shutterClose cast to
//         HdDoubleDataSource (the type-routing fix), not float.
TEST(DataSourceNames, CameraContainerAdvertisesAndServesParams)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera = FindTerminalSceneIndexWithPrim(
        sceneIndices, "perspShape", HdPrimTypeTokens->camera);
    ASSERT_TRUE(sceneIndexWithCamera) << "perspShape camera prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithCamera);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate("perspShape", HdPrimTypeTokens->camera), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "perspShape not found in MayaHydraSceneIndex";

    HdSceneIndexPrim prim = foundPrims.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    HdContainerDataSourceHandle cameraContainer
        = GetChildContainer(prim.dataSource, HdCameraSchemaTokens->camera);
    ASSERT_TRUE(cameraContainer) << "camera container data source missing on camera prim";

    const TfTokenVector names = cameraContainer->GetNames();

    // Newly-advertised float params.
    for (const TfToken& param : { HdCameraSchemaTokens->focusDistance, HdCameraSchemaTokens->fStop }) {
        EXPECT_TRUE(Contains(names, param))
            << "camera param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdFloatDataSource::Cast(cameraContainer->Get(param));
        EXPECT_TRUE(ds) << "camera param '" << param.GetText()
                        << "' should be served as a float data source";
    }

    // shutterOpen/shutterClose are doubles in HdCameraSchema. They must be
    // routed through a double-typed data source; the previous code routed every
    // remaining schema member through float, which fails this cast and silently
    // coerces the value to 0. This is the regression guard for that fix.
    for (const TfToken& param :
         { HdCameraSchemaTokens->shutterOpen, HdCameraSchemaTokens->shutterClose }) {
        EXPECT_TRUE(Contains(names, param))
            << "camera param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdDoubleDataSource::Cast(cameraContainer->Get(param));
        EXPECT_TRUE(ds) << "camera param '" << param.GetText()
                        << "' must be served as a DOUBLE data source (schema-conformant), not float";
    }

    // windowPolicy is not an HdCameraSchema member; it is keyed on HdCameraTokens
    // and already special-cased in Get(). Just confirm it is now advertised.
    EXPECT_TRUE(Contains(names, HdCameraTokens->windowPolicy))
        << "camera windowPolicy should be advertised by GetNames()";
    EXPECT_TRUE(cameraContainer->Get(HdCameraTokens->windowPolicy))
        << "camera windowPolicy should be served";
}
