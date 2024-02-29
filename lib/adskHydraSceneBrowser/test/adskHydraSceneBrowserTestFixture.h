// Copyright 2023 Autodesk
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

#ifndef ADSK_HYDRA_SCENE_BROWSER_TEST_FIXTURE_H
#define ADSK_HYDRA_SCENE_BROWSER_TEST_FIXTURE_H

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneIndex.h>

#include <gtest/gtest.h>

#include <dataSourceTreeWidget.h>
#include <dataSourceValueTreeView.h>
#include <sceneIndexDebuggerWidget.h>
#include <sceneIndexTreeWidget.h>

struct DataSourceEntry
{
    pxr::TfToken                name;
    pxr::HdDataSourceBaseHandle dataSource;
};

class AdskHydraSceneBrowserTestFixture : public ::testing::Test
{
public:
    AdskHydraSceneBrowserTestFixture() = default;
    ~AdskHydraSceneBrowserTestFixture() override = default;

    void SetUp() override;
    void TearDown() override;

    static void SetReferenceSceneIndex(pxr::HdSceneIndexBasePtr referenceSceneIndex);

protected:
    void ComparePrimHierarchy(
        bool compareDataSourceHierarchy = false,
        bool compareDataSourceValues = false);

    void VerifySceneCorrectness();

    void
    CompareDataSourceHierarchy(DataSourceEntry rootDataSourceEntry, bool compareValues = false);

    void CompareDataSourceValue(pxr::HdSampledDataSourceHandle sampledDataSource);

    bool MatchesFallbackTextOutput(const std::string& text);

    void CompareValueContent(const pxr::VtValue& value);

    template <typename ElementType> void CompareIfArray(const pxr::VtValue& value);

    template <typename ElementType>
    void CompareArrayContents(const pxr::VtArray<ElementType>& vtArray);

    std::unique_ptr<pxr::HduiSceneIndexDebuggerWidget> _sceneBrowserWidget
        = std::make_unique<pxr::HduiSceneIndexDebuggerWidget>();
    pxr::HduiSceneIndexTreeWidget*    _primHierarchyWidget = nullptr;
    pxr::HduiDataSourceTreeWidget*    _dataSourceHierarchyWidget = nullptr;
    pxr::HduiDataSourceValueTreeView* _dataSourceValueView = nullptr;

    static pxr::HdSceneIndexBasePtr sceneIndex;
};

#endif // ADSK_HYDRA_SCENE_BROWSER_TEST_FIXTURE_H
