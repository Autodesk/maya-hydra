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

#include <mayaHydraLib/renderThread/renderThreadCaptureSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool Contains(const TfTokenVector& names, const TfToken& token)
{
    return std::find(names.begin(), names.end(), token) != names.end();
}

HdContainerDataSourceHandle
GetChildContainer(const HdContainerDataSourceHandle& parent, const TfToken& key)
{
    if (!parent) {
        return nullptr;
    }
    return HdContainerDataSource::Cast(parent->Get(key));
}

// Lists only visibility, but also serves xform, and extent with an empty value.
class PartlyListedContainer : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(PartlyListedContainer);

    TfTokenVector GetNames() override { return { HdVisibilitySchema::GetSchemaToken() }; }

    HdDataSourceBaseHandle Get(const TfToken& name) override
    {
        if (name == HdVisibilitySchema::GetSchemaToken()) {
            return HdVisibilitySchema::Builder()
                .SetVisibility(HdRetainedTypedSampledDataSource<bool>::New(true))
                .Build();
        }
        if (name == HdXformSchema::GetSchemaToken()) {
            return HdXformSchema::Builder()
                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(GfMatrix4d(1.0)))
                .Build();
        }
        if (name == HdExtentSchema::GetSchemaToken()) {
            return HdRetainedSampledDataSource::New(VtValue());
        }
        return nullptr;
    }
};

// A value the test changes in place, as a live Maya-backed data source would.
template <typename T> class SettableDataSource : public HdTypedSampledDataSource<T>
{
public:
    HD_DECLARE_DATASOURCE(SettableDataSource<T>);

    void Set(const T& value) { _value = value; }

    VtValue GetValue(HdSampledDataSource::Time) override { return VtValue(_value); }
    T       GetTypedValue(HdSampledDataSource::Time) override { return _value; }
    bool    GetContributingSampleTimesForInterval(
           HdSampledDataSource::Time,
           HdSampledDataSource::Time,
           std::vector<HdSampledDataSource::Time>*) override
    {
        return false;
    }

private:
    explicit SettableDataSource(const T& value)
        : _value(value)
    {
    }

    T _value;
};

// A transform that, when next read, runs a callback first, as a Maya transform
// read through MDagPath evaluates lazily and fires its matrix callbacks.
class LazyMatrixDataSource : public HdMatrixDataSource
{
public:
    HD_DECLARE_DATASOURCE(LazyMatrixDataSource);

    void OnNextRead(std::function<void()> callback) { _onNextRead = std::move(callback); }

    VtValue GetValue(HdSampledDataSource::Time time) override
    {
        return VtValue(GetTypedValue(time));
    }
    GfMatrix4d GetTypedValue(HdSampledDataSource::Time) override
    {
        if (_onNextRead) {
            std::function<void()> callback = std::move(_onNextRead);
            _onNextRead = nullptr;
            callback();
        }
        return GfMatrix4d(1.0);
    }
    bool GetContributingSampleTimesForInterval(
        HdSampledDataSource::Time,
        HdSampledDataSource::Time,
        std::vector<HdSampledDataSource::Time>*) override
    {
        return false;
    }

private:
    LazyMatrixDataSource() = default;

    std::function<void()> _onNextRead;
};

// Reads each dirtied prim while handling the notice, as Flash's observer does.
class ReadingObserver : public HdSceneIndexObserver
{
public:
    explicit ReadingObserver(HdSceneIndexBase& sceneIndex)
        : _sceneIndex(sceneIndex)
    {
    }

    void PrimsAdded(const HdSceneIndexBase&, const AddedPrimEntries&) override { }
    void PrimsRemoved(const HdSceneIndexBase&, const RemovedPrimEntries&) override { }
    void PrimsDirtied(const HdSceneIndexBase&, const DirtiedPrimEntries& entries) override
    {
        dirtied += entries.size();
        if (!reading) {
            return;
        }
        for (const auto& entry : entries) {
            HdXformSchema::GetFromParent(_sceneIndex.GetPrim(entry.primPath).dataSource)
                .GetMatrix()
                ->GetTypedValue(0.0f);
        }
    }
    void PrimsRenamed(const HdSceneIndexBase&, const RenamedPrimEntries&) override { }

    size_t dirtied = 0;
    bool   reading = true;

private:
    HdSceneIndexBase& _sceneIndex;
};

class DirtyCounter : public HdSceneIndexObserver
{
public:
    void PrimsAdded(const HdSceneIndexBase&, const AddedPrimEntries&) override { }
    void PrimsRemoved(const HdSceneIndexBase&, const RemovedPrimEntries&) override { }
    void PrimsDirtied(const HdSceneIndexBase&, const DirtiedPrimEntries& entries) override
    {
        dirtied += entries.size();
    }
    void PrimsRenamed(const HdSceneIndexBase&, const RenamedPrimEntries&) override { }

    size_t dirtied = 0;
};

GfMatrix4d Translation(double x)
{
    GfMatrix4d matrix(1.0);
    matrix.SetTranslate(GfVec3d(x, 0.0, 0.0));
    return matrix;
}

void CollectPrimPaths(
    const HdSceneIndexBaseRefPtr& sceneIndex,
    const SdfPath&                path,
    SdfPathVector&                out)
{
    out.push_back(path);
    for (const SdfPath& child : sceneIndex->GetChildPrimPaths(path)) {
        CollectPrimPaths(sceneIndex, child, out);
    }
}

} // namespace

// What: the capture's probe for names a container serves but does not list.
// How: a container that lists visibility, serves xform unlisted, and serves extent
//      with an empty value.
// Expect: exactly xform is reported; the empty extent is not.
TEST(DataSourceNames, CaptureFindsUnlistedNames)
{
    const std::vector<HdDataSourceLocator> unlisted
        = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::FindUnlistedNames(
            PartlyListedContainer::New());
    ASSERT_EQ(unlisted.size(), 1u);
    EXPECT_EQ(unlisted.front(), HdXformSchema::GetDefaultLocator());
}

// What: the capture's copy of a multisampled value answers a renderer's shutter as
//       the live source would.
// How: a prim whose transform is sampled at -1, -0.25, 0.25 and 1 frames, with
//      translate x = 4 * time, read through an active, held capture.
// Expect: a centered shutter reports its own two samples, with no sample added at
//         the frame; a shutter starting at the frame also reports the nearest
//         sample beyond each edge; values between samples are interpolated, and
//         the frame value is the source's own.
TEST(DataSourceNames, CaptureAnswersShutterIntervals)
{
    HdSampledDataSource::Time times[] = { -1.0f, -0.25f, 0.25f, 1.0f };
    GfMatrix4d                matrices[4];
    for (size_t i = 0; i < 4; ++i) {
        matrices[i].SetTranslate(GfVec3d(4.0 * times[i], 0.0, 0.0));
    }
    HdMatrixDataSourceHandle liveMatrix
        = HdRetainedTypedMultisampledDataSource<GfMatrix4d>::New(4, times, matrices);
    const SdfPath primPath("/prim");

    HdRetainedSceneIndexRefPtr input = HdRetainedSceneIndex::New();
    input->AddPrims({ { primPath,
                        HdPrimTypeTokens->mesh,
                        HdRetainedContainerDataSource::New(
                            HdXformSchema::GetSchemaToken(),
                            HdXformSchema::Builder().SetMatrix(liveMatrix).Build()) } });

    auto capture = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::New(input);
    capture->Activate();
    capture->Hold();
    const HdMatrixDataSourceHandle matrix
        = HdXformSchema::GetFromParent(capture->GetPrim(primPath).dataSource).GetMatrix();
    ASSERT_TRUE(matrix);
    EXPECT_NE(matrix, liveMatrix) << "the transform was not copied";

    std::vector<HdSampledDataSource::Time> sampleTimes;
    ASSERT_TRUE(matrix->GetContributingSampleTimesForInterval(-0.25f, 0.25f, &sampleTimes));
    EXPECT_EQ(sampleTimes, (std::vector<HdSampledDataSource::Time> { -0.25f, 0.25f }));

    ASSERT_TRUE(matrix->GetContributingSampleTimesForInterval(0.0f, 0.5f, &sampleTimes));
    EXPECT_EQ(sampleTimes, (std::vector<HdSampledDataSource::Time> { -0.25f, 0.25f, 1.0f }));

    // Interpolated with a float weight.
    EXPECT_NEAR(matrix->GetTypedValue(0.5f).ExtractTranslation()[0], 2.0, 1e-5);
    EXPECT_DOUBLE_EQ(
        matrix->GetTypedValue(0.0f).ExtractTranslation()[0],
        liveMatrix->GetTypedValue(0.0f).ExtractTranslation()[0]);
    EXPECT_DOUBLE_EQ(matrix->GetTypedValue(1.0f).ExtractTranslation()[0], 4.0);

    capture->Release();
}

// What: the capture holds its copy fixed, then recopies only what changed.
// How: a mesh whose transform and a light whose intensity change in place, each
//      dirtied while the capture is held, then released.
// Expect: while held, the copy keeps the old values and no notice is forwarded;
//         after release both notices are forwarded and the copy shows the new
//         values, while what was not dirtied is the same copy as before.
TEST(DataSourceNames, CaptureHoldsReleasesAndRecopies)
{
    const SdfPath meshPath("/mesh");
    const SdfPath lightPath("/light");
    auto          meshMatrix = SettableDataSource<GfMatrix4d>::New(Translation(1.0));
    auto          intensity = SettableDataSource<float>::New(1.0f);

    HdRetainedSceneIndexRefPtr input = HdRetainedSceneIndex::New();
    input->AddPrims(
        { { meshPath,
            HdPrimTypeTokens->mesh,
            HdRetainedContainerDataSource::New(
                HdXformSchema::GetSchemaToken(),
                HdXformSchema::Builder().SetMatrix(meshMatrix).Build(),
                HdVisibilitySchema::GetSchemaToken(),
                HdVisibilitySchema::Builder()
                    .SetVisibility(HdRetainedTypedSampledDataSource<bool>::New(true))
                    .Build()) },
          { lightPath,
            HdPrimTypeTokens->sphereLight,
            HdRetainedContainerDataSource::New(
                HdXformSchema::GetSchemaToken(),
                HdXformSchema::Builder()
                    .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(Translation(3.0)))
                    .Build(),
                HdLightSchema::GetSchemaToken(),
                HdRetainedContainerDataSource::New(
                    HdLightTokens->intensity,
                    intensity,
                    HdLightTokens->color,
                    HdRetainedTypedSampledDataSource<GfVec3f>::New(GfVec3f(1.0f)))) } });

    auto capture = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::New(input);
    DirtyCounter observer;
    capture->AddObserver(HdSceneIndexObserverPtr(&observer));
    capture->Activate();
    capture->Hold();

    auto meshX = [&] {
        return HdXformSchema::GetFromParent(capture->GetPrim(meshPath).dataSource)
            .GetMatrix()
            ->GetTypedValue(0.0f)
            .ExtractTranslation()[0];
    };
    auto lightContainer = [&] {
        return GetChildContainer(
            capture->GetPrim(lightPath).dataSource, HdLightSchema::GetSchemaToken());
    };
    auto lightIntensity = [&] {
        return HdFloatDataSource::Cast(lightContainer()->Get(HdLightTokens->intensity))
            ->GetTypedValue(0.0f);
    };
    const HdDataSourceBaseHandle meshVisibility
        = capture->GetPrim(meshPath).dataSource->Get(HdVisibilitySchema::GetSchemaToken());
    const HdDataSourceBaseHandle lightXform
        = capture->GetPrim(lightPath).dataSource->Get(HdXformSchema::GetSchemaToken());
    ASSERT_DOUBLE_EQ(meshX(), 1.0);
    ASSERT_FLOAT_EQ(lightIntensity(), 1.0f);

    meshMatrix->Set(Translation(2.0));
    intensity->Set(5.0f);
    input->DirtyPrims(
        { { meshPath, HdXformSchema::GetDefaultLocator() },
          { lightPath, HdLightSchema::GetDefaultLocator().Append(HdLightTokens->intensity) } });

    EXPECT_EQ(observer.dirtied, 0u) << "a notice was forwarded while held";
    EXPECT_DOUBLE_EQ(meshX(), 1.0) << "the held copy changed";
    EXPECT_FLOAT_EQ(lightIntensity(), 1.0f) << "the held copy changed";

    capture->Release();
    EXPECT_EQ(observer.dirtied, 2u) << "the held notices were not forwarded on release";
    EXPECT_DOUBLE_EQ(meshX(), 2.0) << "the transform was not recopied";
    EXPECT_FLOAT_EQ(lightIntensity(), 5.0f) << "the light was not recopied";
    EXPECT_TRUE(lightContainer()->Get(HdLightTokens->color)) << "the light recopy lost a param";
    EXPECT_EQ(
        capture->GetPrim(meshPath).dataSource->Get(HdVisibilitySchema::GetSchemaToken()),
        meshVisibility)
        << "visibility was not dirtied but was copied again";
    EXPECT_EQ(
        capture->GetPrim(lightPath).dataSource->Get(HdXformSchema::GetSchemaToken()), lightXform)
        << "the light's transform was not dirtied but was copied again";

    capture->RemoveObserver(HdSceneIndexObserverPtr(&observer));
}

// What: a notice sent while the capture copies a prim, by the copy's own read of
//       upstream, neither deadlocks nor is lost.
// How: a transform that dirties its prim when read. First an observer reads the
//      prim while handling its dirty notice, so the capture copies it then; then
//      the capture copies it in Hold().
// Expect: each notice the reads send is forwarded once the copy is done, after
//         release if it came while held.
TEST(DataSourceNames, CaptureDefersNoticesSentByItsReads)
{
    const SdfPath primPath("/prim");
    auto          matrix = LazyMatrixDataSource::New();

    HdRetainedSceneIndexRefPtr input = HdRetainedSceneIndex::New();
    input->AddPrims({ { primPath,
                        HdPrimTypeTokens->mesh,
                        HdRetainedContainerDataSource::New(
                            HdXformSchema::GetSchemaToken(),
                            HdXformSchema::Builder().SetMatrix(matrix).Build()) } });
    HdRetainedSceneIndex* const inputPtr = input.operator->();
    auto dirtyOnRead = [inputPtr, &primPath] {
        inputPtr->DirtyPrims({ { primPath, HdXformSchema::GetDefaultLocator() } });
    };

    auto            capture = MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::New(input);
    ReadingObserver observer(*capture);
    capture->AddObserver(HdSceneIndexObserverPtr(&observer));
    capture->Activate();
    capture->Hold();
    capture->Release();

    matrix->OnNextRead(dirtyOnRead);
    input->DirtyPrims({ { primPath, HdXformSchema::GetDefaultLocator() } });
    EXPECT_EQ(observer.dirtied, 2u) << "the notice sent by a lazy copy was not forwarded";

    observer.reading = false;
    input->DirtyPrims({ { primPath, HdXformSchema::GetDefaultLocator() } });
    EXPECT_EQ(observer.dirtied, 3u);
    matrix->OnNextRead(dirtyOnRead);
    capture->Hold();
    EXPECT_EQ(observer.dirtied, 3u) << "a notice was forwarded while held";
    capture->Release();
    EXPECT_EQ(observer.dirtied, 4u) << "the notice sent by Hold's copy was not forwarded";

    capture->RemoveObserver(HdSceneIndexObserverPtr(&observer));
}

// What: every prim MayaHydraSceneIndex produces lists every name it serves, so the
//       render thread's copy of the scene drops nothing.
// How: walk all of its prims and probe each with the capture's FindUnlistedNames.
// Expect: nothing reported.
TEST(DataSourceNames, MayaHydraPrimsListEveryName)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndices.front());
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SdfPathVector paths;
    CollectPrimPaths(mayaSceneIndex, SdfPath::AbsoluteRootPath(), paths);
    ASSERT_GT(paths.size(), 1u);

    for (const SdfPath& path : paths) {
        const HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(path);
        for (const HdDataSourceLocator& locator :
             MAYAHYDRA_NS_DEF::RenderThreadCaptureSceneIndex::FindUnlistedNames(
                 prim.dataSource)) {
            ADD_FAILURE() << path.GetText() << " (" << prim.primType.GetText() << ") serves '"
                          << locator.GetString() << "' but does not list it in GetNames()";
        }
    }
}

// What: the light container advertises its leaf params and serves each one.
// How: create a directional light, reach its `light` container, walk GetNames().
// Expect: every new leaf token is advertised AND Get() returns a non-null typed
//         data source of the schema-conformant type (no phantom rows).
TEST(DataSourceNames, LightContainerAdvertisesAndServesLeafParams)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    // Under Storm a Maya directional light is translated to a simpleLight prim; other
    // renderers get a distantLight (see MayaHydraDirectionalLightAdapter::LightType).
    const std::string rendererName = GetActiveRendererName();
    ASSERT_FALSE(rendererName.empty()) << "No Hydra viewport found to determine the renderer";
    const TfToken lightType = IsStormRendererName(rendererName) ? HdPrimTypeTokens->simpleLight
                                                                : HdPrimTypeTokens->distantLight;

    HdSceneIndexBaseRefPtr sceneIndexWithLight
        = FindTerminalSceneIndexWithPrim(sceneIndices, "directionalLightShape", lightType);
    ASSERT_TRUE(sceneIndexWithLight)
        << "directionalLight prim of type " << lightType.GetText()
        << " not found in any scene index (renderer '" << rendererName << "')";

    // Inspect the MayaHydraSceneIndex directly so we read our data source output
    // unfiltered by any downstream terminal scene index.
    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithLight);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims = inspector.FindPrims(
        CreatePrimPredicate("directionalLightShape", lightType), 1);
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
        HdLightTokens->intensity, HdLightTokens->exposure,         HdLightTokens->diffuse,
        HdLightTokens->specular,  HdLightTokens->colorTemperature,
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
//       one with the schema-conformant type: float for fStop/focusDistance and
//       double for shutterOpen/shutterClose.
// How: reach the persp camera's `camera` container, walk GetNames(), and cast
//      each served data source to its expected typed handle.
// Expect: all five new names are advertised; shutterOpen/shutterClose cast to
//         HdDoubleDataSource (the type-routing fix), not float.
TEST(DataSourceNames, CameraContainerAdvertisesAndServesParams)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithCamera
        = FindTerminalSceneIndexWithPrim(sceneIndices, "perspShape", HdPrimTypeTokens->camera);
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

    for (const TfToken& param : {
             HdCameraSchemaTokens->horizontalAperture,
             HdCameraSchemaTokens->verticalAperture,
             HdCameraSchemaTokens->horizontalApertureOffset,
             HdCameraSchemaTokens->verticalApertureOffset,
             HdCameraSchemaTokens->focalLength,
             HdCameraSchemaTokens->focusDistance,
             HdCameraSchemaTokens->fStop,
#if PXR_VERSION >= 2505
             HdCameraSchemaTokens->linearExposureScale,
#endif
         }) {
        EXPECT_TRUE(Contains(names, param))
            << "camera param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdFloatDataSource::Cast(cameraContainer->Get(param));
        EXPECT_TRUE(ds) << "camera param '" << param.GetText()
                        << "' should be served as a float data source";
    }

    // projection is served as a TfToken (perspective/orthographic).
    EXPECT_TRUE(Contains(names, HdCameraSchemaTokens->projection))
        << "camera projection should be advertised by GetNames()";
    EXPECT_TRUE(HdTokenDataSource::Cast(cameraContainer->Get(HdCameraSchemaTokens->projection)))
        << "camera projection should be served as a TfToken data source";

    // clippingRange is served as a GfVec2f (near, far).
    EXPECT_TRUE(Contains(names, HdCameraSchemaTokens->clippingRange))
        << "camera clippingRange should be advertised by GetNames()";
    EXPECT_TRUE(HdVec2fDataSource::Cast(cameraContainer->Get(HdCameraSchemaTokens->clippingRange)))
        << "camera clippingRange should be served as a GfVec2f data source";

    // clippingPlanes is served as a VtArray<GfVec4d>.
    EXPECT_TRUE(Contains(names, HdCameraSchemaTokens->clippingPlanes))
        << "camera clippingPlanes should be advertised by GetNames()";
    EXPECT_TRUE(
        HdVec4dArrayDataSource::Cast(cameraContainer->Get(HdCameraSchemaTokens->clippingPlanes)))
        << "camera clippingPlanes should be served as a VtArray<GfVec4d> data source";

    // shutterOpen/shutterClose are doubles in HdCameraSchema.
    for (const TfToken& param :
         { HdCameraSchemaTokens->shutterOpen, HdCameraSchemaTokens->shutterClose }) {
        EXPECT_TRUE(Contains(names, param))
            << "camera param '" << param.GetText() << "' should be advertised by GetNames()";
        auto ds = HdDoubleDataSource::Cast(cameraContainer->Get(param));
        EXPECT_TRUE(ds) << "camera param '" << param.GetText()
                        << "' must be served as a double data source (schema-conformant)";
    }

    // windowPolicy is not an HdCameraSchema member; it is keyed on HdCameraTokens
    // and already special-cased in Get().
    EXPECT_TRUE(Contains(names, HdCameraTokens->windowPolicy))
        << "camera windowPolicy should be advertised by GetNames()";
    EXPECT_TRUE(cameraContainer->Get(HdCameraTokens->windowPolicy))
        << "camera windowPolicy should be served";
}
